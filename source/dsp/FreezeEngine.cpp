#include "FreezeEngine.h"
#include <algorithm>

namespace gf
{
namespace
{
constexpr double kHalfPi = 1.5707963267948966;
constexpr float kPi = 3.14159265f;
constexpr double kTwoPi = 6.283185307179586;
} // namespace

void FreezeEngine::prepare (double newSampleRate, int /*maxBlockSize*/, int numChannels)
{
    sampleRate = newSampleRate;
    segmentLength = std::max (1, (int) std::lround (kSegmentSeconds * sampleRate));
    bufferSegments = (int) std::ceil (kBufferSeconds / kSegmentSeconds);

    window.resize (kWindowTableSize + 1);
    for (int i = 0; i <= kWindowTableSize; ++i)
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (kTwoPi * i / kWindowTableSize));
    fx.prepare (sampleRate, numChannels);
    grainPowerCoeff = (float) (1.0 - std::exp (-1.0 / (1.0 * sampleRate))); // 1 s

    channelState.assign ((size_t) std::max (1, numChannels), Channel {});
    for (auto& c : channelState)
    {
        c.staging.assign ((size_t) segmentLength, 0.0f);
        c.buffer.assign ((size_t) (segmentLength * bufferSegments), 0.0f);
        c.flat.assign (c.buffer.size(), 0.0f);
        c.segmentTime.assign ((size_t) bufferSegments, 0);
        c.energy.assign ((size_t) bufferSegments, 0.0f);
        c.floor.prepare (sampleRate);
    }
    setGrainSeconds (grainSeconds); // the floor's interval
    reset();
}

void FreezeEngine::reset()
{
    stagingFill = 0;
    sampleCounter = 0;
    grainsSpawned = grainsReversed = grainsClose = 0;
    bypassed = false;
    fx.reset();
    for (size_t ch = 0; ch < channelState.size(); ++ch)
    {
        auto& c = channelState[ch];
        std::fill (c.staging.begin(), c.staging.end(), 0.0f);
        std::fill (c.buffer.begin(), c.buffer.end(), 0.0f);
        std::fill (c.flat.begin(), c.flat.end(), 0.0f);
        c.levelReference = 0.0f;
        c.flatEnvelope = 0.0f;
        c.fadeInRemaining = 0;
        c.pendingDip = -1;
        std::fill (c.energy.begin(), c.energy.end(), 0.0f);
        c.committedSegments = 0;
        for (auto& g : c.grains)
            g = Grain {};
        // Each channel draws its own grain positions, so left and right decorrelate.
        c.random.seed (seed * 0x9e3779b97f4a7c15ull + ch + 1);
        c.floor.reset (seed * 0x2545f4914f6cdd1dull + ch + 1);
        c.floorStale = true;
        c.grainPower = 0.0f;
        c.nextSpawn = 0;
        c.reverseAccumulator = 0.5f;
    }
}

void FreezeEngine::setSeed (uint64_t newSeed)
{
    seed = newSeed;
    reset();
}

void FreezeEngine::setGrainSeconds (double seconds)
{
    grainSeconds = std::clamp (seconds, 0.040, 0.500);
    // The Grain knob also sets how fast the floor follows new notes: a new loop every 0.3 s to 4 s
    const double t = std::log (grainSeconds / 0.040) / std::log (0.500 / 0.040);
    for (auto& c : channelState)
        c.floor.setIntervalSeconds (0.3 * std::pow (4.0 / 0.3, t));
}

void FreezeEngine::setDensity (double grainsOverlapping)
{
    density = std::clamp (grainsOverlapping, 2.0, 16.0);
}

void FreezeEngine::setMaxReverseShare (float share)
{
    maxReverseShare = std::clamp (share, 0.0f, 1.0f);
}

double FreezeEngine::reversedShare() const
{
    return grainsSpawned > 0 ? (double) grainsReversed / (double) grainsSpawned : 0.0;
}

double FreezeEngine::closeReadShare() const
{
    return grainsSpawned > 0 ? (double) grainsClose / (double) grainsSpawned : 0.0;
}

double FreezeEngine::readDistance (const Channel& c, int64_t readIndex, bool reverse, const Grain* ignore) const
{
    // Distance from readIndex to where the nearest playing grain of the same direction reads now. Grains of
    // one direction move through the material at the same speed, so this distance stays (about) the same.
    double nearest = 1.0e18;
    for (const auto& g : c.grains)
        if (g.active && g.reverse == reverse && &g != ignore)
        {
            const double position = g.reverse ? (double) (g.length - 1 - g.age) * g.rate : (double) g.age * g.rate;
            nearest = std::min (nearest, std::abs ((double) readIndex - ((double) g.start + position)));
        }
    return nearest;
}

void FreezeEngine::setHold (bool shouldHold)
{
    hold = shouldHold; // (the floor reads the recording, so it holds too)
}

void FreezeEngine::setDetune (float amount)
{
    detune = std::clamp (amount, 0.0f, 1.0f);
}

float FreezeEngine::sourceAt (const Channel& c, int64_t index, float flatMix)
{
    const auto i = (size_t) (index % (int64_t) c.buffer.size());
    return flatMix > 0.0f ? c.buffer[i] + (c.flat[i] - c.buffer[i]) * flatMix : c.buffer[i];
}

void FreezeEngine::setFloorLevel (float level)
{
    floorLevel = std::max (0.0f, level);
}

void FreezeEngine::fillFloorFrame (Channel& c, int frame)
{
    // The floor's spectrum is the average of SpectralFloor::kFrames frames of the flattened copy (the notes'
    // bodies), spread over the material the grains choose from, so it holds the same notes as the cloud.
    // Frames reach no further back than the material (zeros before it). (The newest segment is not
    // flattened yet.)
    constexpr int frames = SpectralFloor::kFrames;
    const auto capacity = (int64_t) c.flat.size();
    const int64_t end = std::max<int64_t> (0, c.committedSegments - 1) * segmentLength;
    const int64_t span = std::min<int64_t> ({ end, (int64_t) std::lround (kSelectSeconds * sampleRate), capacity - segmentLength });
    const int n = c.floor.frameSize();
    const int64_t frameEnd = end - std::max<int64_t> (0, span - n) * frame / (frames - 1);
    float* x = c.floor.frameInput();
    for (int i = 0; i < n; ++i)
    {
        const int64_t index = frameEnd - n + i;
        x[i] = index >= end - span ? c.flat[(size_t) (index % capacity)] : 0.0f;
    }
    c.floor.addFrame();
}

void FreezeEngine::setMaxSmoothness (float amount)
{
    maxSmoothness = std::clamp (amount, 0.0f, 1.0f);
}

void FreezeEngine::setFxCharacter (const FxCharacter& character)
{
    fx.setCharacter (character);
}

int64_t FreezeEngine::recordedSamples (int channel) const
{
    return channelState[(size_t) channel].committedSegments * segmentLength;
}

void FreezeEngine::commitSegment (Channel& c, int64_t segmentTime)
{
    double sum = 0.0;
    for (float x : c.staging)
        sum += (double) x * x;
    const auto segmentRms = (float) std::sqrt (sum / segmentLength);

    // Silence hold: quiet segments are dropped, so the buffer keeps the last sounding material.
    // While Hold is on nothing new is recorded, so the grains keep playing what was captured.
    if (hold || segmentRms < kSilenceRms)
        return;

    const auto slot = (size_t) (c.committedSegments % bufferSegments);
    std::copy (c.staging.begin(), c.staging.end(), c.buffer.begin() + (std::ptrdiff_t) (slot * (size_t) segmentLength));
    c.energy[slot] = segmentRms;
    c.segmentTime[slot] = segmentTime;
    const float follow = c.committedSegments == 0 ? 1.0f : 0.01f; // ~1 s time constant
    c.levelReference += follow * (segmentRms - c.levelReference);
    ++c.committedSegments;

    // The previous segment's neighbours are now both known, so its flattened copy can be made
    if (c.committedSegments >= 2)
        flattenSegment (c, c.committedSegments - 2);
}

void FreezeEngine::flattenSegment (Channel& c, int64_t segment)
{
    // Flattening divides the audio by its peak envelope, which removes each note's attack and decay and
    // leaves its tone at a steady level. The envelope looks 1 ms ahead and attacks instantly, so it is
    // already up when a note starts (no overshoot). Where the recording jumps (a note start, or a join
    // where silence was dropped) the copy dips briefly to zero and fades back in, so grains never click.
    const auto capacity = c.buffer.size();
    const auto base = (size_t) (segment % bufferSegments) * (size_t) segmentLength;
    auto sampleAt = [&] (int i) { return c.buffer[(base + (size_t) i) % capacity]; }; // i may run into the next segment
    const int lookAhead = std::max (1, (int) std::lround (0.001 * sampleRate));
    const int fadeOut = std::max (1, (int) std::lround (0.002 * sampleRate));
    const int fadeIn = std::max (1, (int) std::lround (0.004 * sampleRate));
    const float release = (float) std::exp (-1.0 / (0.040 * sampleRate));

    int dips[8];
    int numDips = 0;
    const auto timeOf = [&] (int64_t s) { return c.segmentTime[(size_t) (s % bufferSegments)]; };
    if (segment > 0 && timeOf (segment) != timeOf (segment - 1) + 1)
        dips[numDips++] = 0; // silence was dropped before this segment: the recording jumps here
    if (c.pendingDip >= 0 && numDips < 8)
        dips[numDips++] = c.pendingDip;
    c.pendingDip = -1;

    for (int i = 0; i < segmentLength; ++i)
    {
        float peak = 0.0f;
        int loudest = i;
        for (int j = i; j <= i + lookAhead; ++j)
            if (std::abs (sampleAt (j)) > peak)
            {
                peak = std::abs (sampleAt (j));
                loudest = j;
            }

        // A note start: the level ahead is far above the envelope
        if (peak > 3.0f * c.flatEnvelope && c.flatEnvelope > 1.0e-5f)
        {
            int start = i;
            while (start < loudest && std::abs (sampleAt (start)) < 2.0f * c.flatEnvelope)
                ++start;
            if (start < segmentLength)
            {
                if (numDips < 8)
                    dips[numDips++] = start;
            }
            else
                c.pendingDip = start - segmentLength;
        }

        c.flatEnvelope = peak > c.flatEnvelope ? peak : peak + (c.flatEnvelope - peak) * release;
        // levelReference is an RMS level and the envelope a peak level; sqrt 2 converts for tonal material
        const float gain = std::min (8.0f, 1.4142f * c.levelReference / std::max (c.flatEnvelope, 1.0e-6f));
        c.flat[base + (size_t) i] = sampleAt (i) * gain;
    }

    // Fade-in carried over from a dip near the end of the previous segment
    for (int i = 0; i < c.fadeInRemaining && i < segmentLength; ++i)
    {
        const int k = fadeIn - c.fadeInRemaining + i;
        c.flat[base + (size_t) i] *= 0.5f - 0.5f * std::cos (kPi * (float) k / (float) fadeIn);
    }
    c.fadeInRemaining = std::max (0, c.fadeInRemaining - segmentLength);

    // Dips: fade out just before the jump (reaching back into the previous segment if needed), fade in after
    const bool previousIsContiguous = segment > 0;
    for (int d = 0; d < numDips; ++d)
    {
        const int at = dips[d];
        for (int k = 1; k <= fadeOut; ++k)
        {
            const int i = at - k;
            if (i < 0 && ! previousIsContiguous)
                break;
            const auto index = (base + capacity + (size_t) (std::ptrdiff_t) i) % capacity;
            c.flat[index] *= 0.5f - 0.5f * std::cos (kPi * (float) k / (float) fadeOut);
        }
        for (int k = 0; k < fadeIn; ++k)
        {
            const int i = at + k;
            if (i >= segmentLength)
            {
                c.fadeInRemaining = std::max (c.fadeInRemaining, fadeIn - k);
                break;
            }
            c.flat[base + (size_t) i] *= 0.5f - 0.5f * std::cos (kPi * (float) k / (float) fadeIn);
        }
    }
}

double FreezeEngine::currentGrainSeconds() const
{
    return grainSeconds * (1.0 + smoothness); // up to twice as long at full Amount
}

double FreezeEngine::spacingSeconds() const
{
    const double overlap = density + (24.0 - density) * smoothness; // up to ~24 grains overlapping
    return std::min (currentGrainSeconds() / overlap, kMaxSpacingSeconds);
}

int FreezeEngine::nextSpawnInterval (Channel& c)
{
    const double jitter = 1.0 + kSpawnJitter * (2.0 * c.random.uniform() - 1.0);
    return std::max (1, (int) std::lround (spacingSeconds() * sampleRate * jitter));
}

void FreezeEngine::spawnGrain (Channel& c, float amount)
{
    // Direction first. Some grains play backwards, more of them as Amount rises, so at full Amount attacks
    // and decays blend into one directionless wash. Reversed grains are spread evenly between forward ones
    // (error diffusion, not coin flips) so runs of one direction don't leave holes.
    c.reverseAccumulator += maxReverseShare * amount;
    const bool wantsReverse = c.reverseAccumulator >= 1.0f;
    if (wantsReverse)
        c.reverseAccumulator -= 1.0f;

    // Candidates: segments from the last kSelectSeconds of committed material, leaving out the newest
    // ones so that a whole grain fits before the write head (a reversed grain may sit up to half a grain
    // later, see below, so it leaves out twice as many). Early on, with little material recorded, a
    // reversed grain falls back to forward and a long grain to a shorter one.
    int grainLength = std::max (2, (int) std::lround (currentGrainSeconds() * sampleRate));
    const int64_t windowSegments = (int64_t) std::lround (kSelectSeconds / kSegmentSeconds);
    int64_t halfGrainSegments = 0, first = 0, last = 0;
    bool reverse = wantsReverse;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        // +1: the newest segment's flattened copy is only made once the next one arrives. Detuned grains
        // read up to 3 % further (plus interpolation neighbours), so they keep a little more distance.
        const int reach = detune > 0.0f ? (int) std::ceil (grainLength * 0.515) + 3 : grainLength / 2;
        halfGrainSegments = (reach + segmentLength - 1) / segmentLength + 1;
        last = c.committedSegments - halfGrainSegments * (reverse ? 2 : 1); // exclusive
        first = std::max<int64_t> (halfGrainSegments, c.committedSegments - windowSegments);
        if (last > first)
            break;
        if (reverse)
            reverse = false;
        else
            grainLength = std::max (2 * segmentLength, grainLength / 2);
    }
    if (last <= first)
        return;

    float loudest = 0.0f;
    double sumSquares = 0.0;
    for (int64_t s = first; s < last; ++s)
    {
        const float e = c.energy[(size_t) (s % bufferSegments)];
        loudest = std::max (loudest, e);
        sumSquares += (double) e * e;
    }
    const double targetRms = std::sqrt (sumSquares / (double) (last - first));

    // Pick uniformly among the segments loud enough (favours attacks and bodies over quiet tails).
    const float threshold = kSelectThreshold * loudest;
    int eligible = 0;
    for (int64_t s = first; s < last; ++s)
        eligible += c.energy[(size_t) (s % bufferSegments)] >= threshold ? 1 : 0;

    // Use a free voice, or steal the one that has played longest.
    Grain* voice = &c.grains[0];
    for (auto& g : c.grains)
    {
        if (! g.active)
        {
            voice = &g;
            break;
        }
        if (g.age > voice->age)
            voice = &g;
    }

    const auto candidateStart = [&] {
        int pick = c.random.below (eligible);
        int64_t chosen = first;
        for (int64_t s = first; s < last; ++s)
            if (c.energy[(size_t) (s % bufferSegments)] >= threshold && pick-- == 0)
            {
                chosen = s;
                break;
            }

        // A forward grain is centred on the chosen spot, so the loud part after it fills the second half of
        // the grain. A reversed grain plays that loud part backwards, ending at the chosen spot; it is moved
        // later by the length of the loud run so the loud part again fills the second half.
        int64_t loudRun = 0;
        if (reverse)
            for (int64_t s = chosen + 1; s < c.committedSegments && loudRun < halfGrainSegments; ++s, ++loudRun)
                if (c.energy[(size_t) (s % bufferSegments)] < threshold)
                    break;
        return chosen * segmentLength + c.random.below (segmentLength) - grainLength / 2 + loudRun * segmentLength;
    };
    const auto distanceFor = [&] (int64_t start) {
        return readDistance (c, reverse ? start + grainLength - 1 : start, reverse, voice);
    };

    // Smooth mode keeps grains reading apart: a grain reading within kMinReadSpacingSeconds of another one
    // forms a comb filter with it. Up to kSpacingTries spots are tried; the first far enough (or else the
    // farthest) wins. (Below 50 % Amount nothing changes, not even the random draws.)
    int64_t start = candidateStart();
    if (smoothness > 0.0f)
    {
        const double minDistance = kMinReadSpacingSeconds * sampleRate;
        double best = distanceFor (start);
        for (int attempt = 1; attempt < kSpacingTries && best < minDistance; ++attempt)
        {
            const int64_t other = candidateStart();
            const double d = distanceFor (other);
            if (d > best)
            {
                best = d;
                start = other;
            }
        }
    }
    voice->start = start;

    // Detune: a random pitch for this grain, up to +-50 cents x detune (no random draw at 0, so the
    // undetuned cloud stays exactly as it was)
    double rate = 1.0;
    if (detune > 0.0f)
        rate = std::pow (2.0, (2.0 * c.random.uniform() - 1.0) * 50.0 * detune / 1200.0);

    // Level-match the grain: its windowed RMS is brought towards the recent input level (within +-6 dB),
    // so short grains on loud attacks are not louder than long grains over whole notes.
    const auto capacity = (int64_t) c.buffer.size();
    double grainSquares = 0.0, windowSquares = 0.0;
    for (int i = 0; i < grainLength; i += 4)
    {
        const double w = 0.5 - 0.5 * std::cos (kTwoPi * i / (grainLength - 1));
        const auto index = (size_t) ((voice->start + (int64_t) ((double) i * rate)) % capacity);
        const double x = c.buffer[index] * (1.0f - smoothness) + c.flat[index] * smoothness;
        grainSquares += w * w * x * x;
        windowSquares += w * w;
    }
    const double grainRms = std::sqrt (grainSquares / windowSquares);
    voice->gain = grainRms > 0.0 ? (float) std::clamp (targetRms / grainRms, 0.5, 2.0) : 1.0f;
    const int64_t firstRead = reverse ? voice->start + grainLength - 1 : voice->start;
    grainsClose += readDistance (c, firstRead, reverse, voice) < kMinReadSpacingSeconds * sampleRate ? 1 : 0;
    voice->age = 0;
    voice->length = grainLength;
    voice->reverse = reverse;
    voice->flatMix = smoothness;
    voice->rate = rate;
    voice->active = true;
    ++grainsSpawned;
    grainsReversed += reverse ? 1 : 0;
}

float FreezeEngine::renderGrains (Channel& c)
{
    const auto capacity = (int64_t) c.buffer.size();
    float sum = 0.0f;
    for (auto& g : c.grains)
    {
        if (! g.active)
            continue;
        // Hann window from the table, linearly interpolated for any grain length
        const float phase = (float) g.age * (float) kWindowTableSize / (float) (g.length - 1);
        const auto idx = std::min ((int) phase, kWindowTableSize - 1);
        const float w = window[(size_t) idx] + (window[(size_t) idx + 1] - window[(size_t) idx]) * (phase - (float) idx);
        float x;
        if (g.rate == 1.0)
        {
            const int offset = g.reverse ? g.length - 1 - g.age : g.age;
            const auto index = (size_t) ((g.start + offset) % capacity);
            x = g.flatMix > 0.0f ? c.buffer[index] + (c.flat[index] - c.buffer[index]) * g.flatMix : c.buffer[index];
        }
        else
        {
            // Detuned: read at the grain's own speed, with 4-point Hermite interpolation
            const double span = (double) (g.length - 1) * g.rate;
            const double position = g.reverse ? span - (double) g.age * g.rate : (double) g.age * g.rate;
            const auto whole = (int64_t) position;
            const auto frac = (float) (position - (double) whole);
            const auto base = g.start + whole;
            const float xm1 = sourceAt (c, base - 1 + capacity, g.flatMix), x0 = sourceAt (c, base, g.flatMix);
            const float x1 = sourceAt (c, base + 1, g.flatMix), x2 = sourceAt (c, base + 2, g.flatMix);
            const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            x = ((c3 * frac + c2) * frac + c1) * frac + x0;
        }
        sum += x * w * g.gain;
        if (++g.age >= g.length)
            g.active = false;
    }
    // Power-sum normalisation for uncorrelated Hann grains at the actual overlap
    return sum * (float) (1.0 / std::sqrt (currentGrainSeconds() / spacingSeconds() * 0.375));
}

void FreezeEngine::process (float* const* channels, int numChannels, int numSamples, const float* freezeAmount,
                            const float* fxAmount)
{
    numChannels = std::min (numChannels, (int) channelState.size());

    bool bypass = true;
    for (int i = 0; i < numSamples && bypass; ++i)
        bypass = freezeAmount[i] <= 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            channelState[(size_t) ch].staging[(size_t) stagingFill] = channels[ch][i];

        if (++stagingFill == segmentLength)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                commitSegment (channelState[(size_t) ch], sampleCounter / segmentLength);
            stagingFill = 0;
        }

        if (! bypass)
        {
            const double k = freezeAmount[i] * kHalfPi;
            const auto dryGain = (float) std::cos (k);
            const auto wetGain = (float) std::sin (k);

            // From 50 % to 100 % Amount the cloud turns smooth: longer, denser grains from the flattened copy
            const float t = std::clamp ((freezeAmount[i] - 0.5f) * 2.0f, 0.0f, 1.0f);
            smoothness = maxSmoothness * t * t * (3.0f - 2.0f * t);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& c = channelState[(size_t) ch];
                if (sampleCounter >= c.nextSpawn)
                {
                    spawnGrain (c, freezeAmount[i]);
                    c.nextSpawn += nextSpawnInterval (c);
                }
                float wet = renderGrains (c);
                c.grainPower += grainPowerCoeff * (wet * wet - c.grainPower);
                if (smoothness > 0.0f && floorLevel > 0.0f)
                {
                    // The floor under the grains, at floorLevel of their level: started afresh (it fades in)
                    // when smooth mode begins
                    if (c.floorStale)
                    {
                        c.floor.clear();
                        c.floorStale = false;
                    }
                    if (const int frame = c.floor.pendingFrame(); frame >= 0)
                        fillFloorFrame (c, frame);
                    wet += c.floor.processSample() * std::sqrt (c.grainPower) * floorLevel * smoothness;
                }
                else
                    c.floorStale = true;
                if (fxAmount != nullptr)
                {
                    const float f = fxAmount[i];
                    const float effect = fx.processSample (ch, wet);
                    wet = (wet + f * effect) / std::sqrt (1.0f + f * f);
                }
                channels[ch][i] = channels[ch][i] * dryGain + wet * wetGain;
            }
        }

        ++sampleCounter;
    }

    // While bypassed, capture keeps running but no grains or effects play; the cloud restarts
    // immediately when raised.
    if (bypass && ! bypassed)
        fx.reset();
    bypassed = bypass;
    if (bypass)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channelState[(size_t) ch];
            for (auto& g : c.grains)
                g.active = false;
            c.nextSpawn = sampleCounter;
            c.grainPower = 0.0f;
        }
}
} // namespace gf
