# Freeze — Sound Research Notes and Smooth-Mode Changes

Date: 2026-10-15

## Goal

At high Amount (50–100 %, "smooth mode") a short or rhythmic pluck input should become one static,
cohesive, natural sustained note: no audible grains, pulsing, flutter, clicks, metallic ringing, comb
filtering or phasey character, while keeping pitch and timbre. Below 50 % Amount the cloud stays exactly as
it is (bit-identical tests), and Amount 0 stays a bit-identical pass-through.

Fixed by earlier user decisions: 24-grain overlap in smooth mode, the Grain / Detune (0 = no detune) /
Noise / Freeze-Hold / FX controls and presets keep their meaning, no UI changes and no new parameters.
The quiet 800 Hz high-passed spectral layer was to stay unless clearly bettered; during this work the user
decided to replace it with a random-phase spectral floor (change B).

Sources of the findings: Mutable Instruments Clouds/Beads manuals and firmware, PaulStretch notes, Henke
Granulator II, Sound On Sound granular tutorials, Output, GRM Freeze, Valhalla, Eventide, Välimäki et al.
"Creating Endless Sounds" (DAFx-18), Laroche & Dolson on phasiness.

## Baseline measurements

Probe: the engine run on 8 s test signals (mono, spectral layer off, seeds 1–3 averaged) and measured from
2 s to 8 s with the helpers in `tests/Measures.h`. Signals: `sine` = `pluckTrain` (220 Hz, tau 25 ms),
`rich` = 8-harmonic 220 Hz plucks (tau 60 ms), `bright` = 4-harmonic 220 Hz body plus a 3.5 kHz "pick"
burst (tau 5 ms), `arp` = the render tool's chord arpeggio. `envVar` = std/mean of 10 ms RMS windows,
`click` = max |second difference| / RMS (only meaningful on `sine`; saw waveforms have steep edges of their
own), `attack` = 3.5 kHz level / 220 Hz level in the output.

| Amount 100 %, grain 0.12 s | envVar | onset skew | click | level vs input | attack |
|---|---|---|---|---|---|
| sine | 0.534 | +0.29 | 0.0119 | −2.52 dB | |
| rich | 0.323 | −0.01 | | −1.15 dB | |
| bright | 0.340 | −0.01 | | −0.77 dB | 0.0213 |
| arp | 0.218 | −0.01 | | −0.62 dB | |

With 0.3 s and 0.5 s grains the numbers are similar (rich envVar 0.31–0.32, sine level down to −3.6 dB).
For comparison the plain cloud (smoothing off) reads the attack burst at about 0.09–0.11.

What the numbers say:
- The remaining "flutter" is level wander of the summed cloud (envVar 0.3 on harmonic plucks, 0.5+ on a
  pure tone). 24 random-phase grains give a Rayleigh-like envelope whatever the overlap; per-grain level
  matching and the 1/sqrt(N) power normalisation keep the average right but cannot stop it.
- The cloud is not brighter than the input (energy above 6 kHz 0.0030 vs 0.0028 dry on the arpeggio,
  reverb presets lower), so a dark tilt would only dull the timbre.
- Attack material still reaches the smooth cloud (3.5 kHz burst at 0.02–0.03 of the body).

## Findings against the current engine

| # | Finding | Engine before | Decision |
|---|---|---|---|
| 1 | Freeze the body, not the onset: read 20–60 ms after onsets until ~30 dB below the peak, level-match, make-up ≤ ~12 dB | `flat` copy flattens the envelope (make-up up to +18 dB) and dips 2 ms / 4 ms at note starts; grain centres may sit on attacks (the loudest segments); grain level match ±6 dB | **Done (change C)**: attack gap in the copy. Restricting grain *centres* to bodies was tried and dropped (see below). The 18 dB flattening cap plus the −12 dB selection threshold already end a body ~30 dB below its peak |
| 2 | Asynchronous timing (±40–50 % jitter), position spray ≥ 80 ms, min. 30–50 ms between concurrent read positions (GRM) | ±25 % jitter; positions uniform over 1.5 s of loud material; nothing stops two grains from reading almost the same spot (51 % of smooth grains started within 40 ms of a playing one) | **Done: spacing (change A)**. Jitter skipped: ±50 % changed envVar by less than the seed-to-seed spread (seed-averaged: rich 0.323 → 0.334, arp 0.218 → 0.236), and random arrival times make the window sum itself fluctuate more. Spray is already 1.5 s |
| 2b | "Peaky" (triangular / Gaussian) position distribution (Beads) | uniform among loud segments | Skip: there is no position control to centre it on, and concentrating positions works against the spacing rule |
| 3 | Hann / equal-power windows, 1/sqrt(N) normalisation | Hann table, power-sum normalisation `1/sqrt(L/spacing * 0.375)` | Already done (verified). A smooth-mode leveller against the remaining level wander was tried and dropped (see below) |
| 4 | Small inherent detune (~±6 cents) | Detune knob only | Skip: conflicts with "Detune 0 = no detune", and measured no gain (seed-averaged envVar rich 0.323 → 0.339, bright 0.340 → 0.370; it adds chorus) |
| 5 | All-pass diffuser (≈4 stages, g ≈ 0.6, 5–30 ms) at high Amount (> 70 %) | none | **Done (change D)**, routed per grain so it never sums a signal with its own all-passed copy (no comb filtering while it fades in) |
| 6 | Dark tail: −2…−3 dB shelf above 5–7 kHz, HP ~90 Hz, reverb HF damping, delay feedback ≤ 35–40 % with HP (20 + 100·fb² Hz) / LP 6–8 kHz in the loop, no modulation in loops | wet not brighter than input; reverb damping per preset; delay loop LP 4 kHz, feedback up to 0.55 (Abyss) | Skip for now: FX run at every Amount, so changing them breaks "below 50 % unchanged" and changes the presets; the shelf would dull the timbre (see measurements). Recommended as a separate, user-approved FX pass |
| 6b | Ensemble 0.15–0.25 Hz, soft saturation 15–20 % | none | Skip: modulation is exactly the phasey/chorus character to avoid; saturation changes timbre |
| 7 | Välimäki / PaulStretch random-phase IFFT "glue" floor (8192–16384 points, 20–30 %) | 800 Hz high-passed oscillator-bank layer at 0.2, analysing the raw input (it replayed the pick attacks: 3.5 kHz burst at 0.10 of the body) | **Done (change B), user decision**: replaces the oscillator-bank layer |
| 7b | Per-grain random pan ~60 % (Clouds) | each channel runs its own independent cloud (fully decorrelated) | Skip: already wider than 60 % pan spread, and mono-safe |
| 8 | Smoothing of every parameter (20–50 ms), freeze on/off crossfade 80–150 ms | Amount / FX / Noise smoothed 50 ms in the processor; grain length and detune only affect new grains; Hold only stops recording, grains play on; new clouds fade in with the grain windows | Already done (tests: knob sweep, grain-knob move, hold toggle have no clicks) |

## Changes made

All of them act only while `smoothness > 0` (Amount above 50 %; the diffuser from 70 %), and none draws
extra random numbers below that. Verified against the original engine: all five presets, Amount 0 and
ramps up to 20 / 45 / 50 %, Detune 0 and 0.6, FX 0.8, stereo — byte-identical output.

**A. Read-position spacing** (`kMinReadSpacingSeconds` 40 ms, `kSpacingTries` 16). Two grains reading
almost the same place at the same time form a comb filter (phasing). A new smooth grain tries up to 16
positions and keeps the first that is 40 ms from where every playing grain of its direction reads now
(else the farthest). Detuned and reversed grains are handled through their current read position. Grains
starting within 40 ms of a playing one: 51 % → 13 % (the rest is where 1.5 s of material is too crowded
for ~12 grains per direction).

**B. Random-phase spectral floor** (`SpectralFloor`, replaces `SpectralFreeze`; user decision). Every
0.3–4 s (Grain knob, as the old layer's follow time) the power spectrum of 4 Hann frames of the flattened
copy, spread over the 1.5 s the grains choose from, is given random phases and turned into a seamless loop
by one inverse FFT; it crossfades in over the next interval, so the floor follows new notes, and holds
with Hold (it reads the recording). Level 25 % of a 1 s average of the grain level, faded in with
smoothness.
- Plain random phases made every partial beat at the bin spacing (floor alone on a steady tone: envelope
  variation 0.46, i.e. 1.5–3 Hz pulsing). So clear peaks (15 dB above the spectrum ±16 Hz away) are gathered
  from ±8 Hz (short notes smear partials that far) into their nearest bin and keep their phase from loop to
  loop: steady sines, crossfaded linearly. The rest (noise, breath) keeps random phases and crossfades at
  equal power. Floor alone on a steady tone: envelope variation 0.009, no seam at the wrap.
- 32768 points at 44.1/48 kHz (65536 at 96 kHz) instead of the suggested 8192–16384: a gathered partial
  sits on a bin, so the bin spacing sets its pitch error (≤ 0.73 Hz: ≤ 6 cents at 220 Hz; 16384 would
  allow 11 cents at 220 Hz, 23 at 110 Hz). The FFTs run one per 20 ms while a loop is made, so the worst
  512-sample block is no slower than with the old layer (2.3 ms vs 3.0 ms, 20 s stereo bench).
- Full range (only below 30 Hz removed), unlike the old 800 Hz high-pass: as a floor it fills the moments
  where grains of one tone cancel, which are deepest on the strong low partials, so a high-pass would leave
  exactly those uncovered.

**C. Attack gap: freeze the body, not the onset** (`kAttackSkipSeconds` 10 ms, `kAttackFadeSeconds`
20 ms). After every note start (or join where silence was dropped, and the start of the recording) the
flattened copy that smooth grains and the floor read stays silent for 10 ms and fades back in over 20 ms
(was a 4 ms fade-in), so pick noise and bright attacks are no longer frozen. The fade-out before a jump is
4 ms (was 2 ms, the steepest edge left), and each dip restarts the gap instead of multiplying with an
earlier one. Smooth grains may be level-matched by up to +12 dB (was +6 dB): the attacks carried much of a
short pluck's level.

**D. Grain-routed all-pass diffuser from 70 %** (`kDiffuserGain` 0.6; 257, 379, 613, 919 samples at
48 kHz = 5.4, 7.9, 12.8, 19.1 ms, +17 samples per further channel). From 70 % to 100 % Amount a growing
share of new grains (0 → 100 %, spread evenly like the reversed grains) plays through the diffuser, so it
fades in grain by grain and the cloud is never mixed with its own all-passed copy. It smears what is left
of grain edges and de-click dips; power is unchanged.

**Tests.** Pitch dominance, detune spread and gap filling were judged on seed 1 only; over 10 seeds the
unchanged engine already failed the pitch check on 4 and the detune check on 1, so any change to random
draws could flip them. They now average (in dB) over 5 seeds or take the median of 3, with the same
thresholds (separate commit, verified green on the unchanged engine first).

### Tried and dropped

- **Grain centres only on note bodies** (≥ 40 ms after onsets): with 240 ms+ grains on ~200 ms notes it
  removed no attack (grains span the onsets anyway; pick ratio 0.021 → 0.033) and made positions less varied.
  The attack gap in the copy does the job (0.021 → 0.0016).
- **Smooth-mode leveller** (10 ms detector vs 1 s average, gain −6…+12 dB): envelope variation on harmonic
  plucks 0.31 → 0.18–0.24, but it turns the random cancellations of a pure tone into fast phase swings: the
  click measure on sine clouds rose 2–4x (0.012 → 0.03–0.05) and its gain modulation spread a pure tone's
  spectrum (±30-cent sidebands up 60–100 %). Gentler settings kept most side effects for less gain.

## Results

Engine at full Amount, real output (floor on), mean of 5 seeds, measured 2–8 s. `click` = max |second
difference| / RMS (meaningful on `sine`; saws have steep edges of their own), `min win` = quietest 10 ms
window vs the overall level, `pick` = 3.5 kHz burst / 220 Hz body.

| signal, grain | envVar before → after | click before → after | level vs input | min win | pick |
|---|---|---|---|---|---|
| sine 0.12 s | 0.526 → 0.554 | 0.0120 → 0.0062 | −2.51 → −1.17 dB | −23.4 → −22.7 dB | |
| sine 0.50 s | 0.591 → 0.559 | 0.0157 → 0.0061 | −3.36 → −1.73 dB | −27.4 → −22.5 dB | |
| rich 0.12 s | 0.325 → 0.312 | 0.102 → 0.047 | −1.33 → +0.32 dB | −9.1 → −9.7 dB | |
| bright 0.12 s | 0.353 → 0.349 | 0.106 → 0.016 | −0.86 → +0.56 dB | −10.8 → −10.9 dB | 0.103 → 0.0016 |
| bright 0.50 s | 0.317 → 0.329 | 0.117 → 0.018 | −0.69 → −0.16 dB | −11.5 → −14.0 dB | 0.082 → 0.0025 |
| arp 0.12 s | 0.225 → 0.217 | 1.20 → 0.61 | −0.56 → +0.24 dB | −6.5 → −6.2 dB | |
| arp 0.50 s | 0.237 → 0.221 | 1.41 → 0.62 | −0.18 → +0.39 dB | −11.3 → −9.6 dB | |

Renders (`render --test-pluck`, left channel, 2–8 s; Ice has FX 0):

| render | envVar | click | level vs input | energy > 6 kHz (dry 0.00283) |
|---|---|---|---|---|
| Ice before → after | 0.227 → 0.219 | 1.08 → 0.61 | −0.66 → +0.10 dB | 0.00300 → 0.00304 |
| Haze before → after | 0.231 → 0.233 | 1.05 → 0.49 | −0.19 → +0.95 dB | 0.00256 → 0.00229 |
| Abyss before → after | 0.239 → 0.222 | 0.85 → 0.46 | +1.09 → +1.47 dB | 0.00192 → 0.00199 |

Amount sweeps: identical up to 50 %; Ice 50–60 % envVar 0.360 → 0.299.

What it adds up to: attacks and clicks are largely gone from the smooth cloud, the level matches the input
better and pure-tone dropouts are shallower. The slow random level wander of the grain sum (envelope
variation ~0.2–0.35 on harmonic material) is essentially unchanged — it is inherent to summing random-phase
grains (Rayleigh statistics) and is the main thing left between this and a perfectly static note.

## Recommended next

- Phase-coherent grain placement (WSOLA-style: nudge each smooth grain by up to half a period to match the
  cloud's phase) is the one known cure for the remaining level wander on tonal input; it needs its own level
  normalisation (coherent sums add in amplitude, not power).
- Raising the floor to 0.3 (top of the 20–30 % range) would fill dips a little more; worth a listening test.
- FX pass (needs a user ruling, since FX run at every Amount): high-pass in the delay feedback loop
  (20 + 100·fb² Hz), feedback ≤ 0.4 for Ember / Abyss (now 0.45 / 0.55), reverb HF damping from ~5–6 kHz.
