#pragma once
// Random-phase spectral floor (after Välimäki, Rämö & Esqueda, "Creating Endless Sounds", DAFx-18): the
// average magnitude spectrum of a few recorded frames, given random phases and turned back into sound by
// one large inverse FFT. The result is a loop that repeats without a seam (a spectrum of whole bins is
// periodic in the frame length) and keeps the material's tone colour and pitch with no attacks, rhythm or
// grain edges. New loops (fresh noise phases, the newest material) crossfade in one after another, so the
// floor follows new notes and does not repeat exactly.
//
// Plain random phases make every partial beat: its energy is spread over the few bins of the window's main
// lobe, and bins with random phases beat at the bin spacing (about 1.5-3 Hz here, heard as pulsing). So
// clear spectral peaks (partials) are gathered into their nearest bin and keep their phase from loop to
// loop: each one is a steady sine, crossfaded linearly. Everything else (noise, breath) keeps random phases
// and crossfades at equal power.
//
// One instance per channel. JUCE-free and real-time safe: everything is allocated in prepare(), and the
// FFT work of a new loop is spread over several calls, one transform per call.
#include "Fft.h"
#include "Random.h"
#include <complex>
#include <cstdint>
#include <vector>

namespace gf
{
class SpectralFloor
{
public:
    static constexpr int kFrames = 4; // frames averaged per loop

    void prepare (double sampleRate);
    void reset (uint64_t seed);

    // A new loop is made this often; each one crossfades in over the same time.
    void setIntervalSeconds (double seconds);

    // While a new loop is being made the floor asks for frames: when pendingFrame() is 0..kFrames-1, fill
    // frameInput() with frameSize() samples of the material (frame 0 the newest, the last the oldest) and
    // call addFrame(). The loop is built a little later, normalised to an RMS of 1; silent frames make none.
    int frameSize() const { return size; }
    int pendingFrame() const { return job >= 0 && job < kFrames && jobWait == 0 ? job : -1; }
    float* frameInput() { return input.data(); }
    void addFrame();

    // Forget the loops: silent until a new one is made, which then fades in.
    void clear();

    float processSample();

private:
    void startJob();
    void buildLoop();

    Fft fft;
    Random random;
    double sampleRate = 48000.0;
    int size = 32768, half = 16384, interval = 48000, retry = 2400, step = 1024;
    int sinceLoop = 0, position = 0, current = 0, fadeIn = 1, fadeLength = 1;
    int job = -1, jobWait = 0; // frame being asked for (kFrames: build the loop), samples until that step
    int lowestBin = 1, lobe = 2;
    bool hasLoop = false;
    double sumSquares = 0.0;

    std::vector<float> window, input, power, phase;
    std::vector<char> peak, wasPeak; // bins holding a gathered partial in the new / the previous loop
    std::vector<float> tonal[2], noise[2]; // partials (coherent from loop to loop) and the rest
    std::vector<std::complex<float>> spectrum;
};
} // namespace gf
