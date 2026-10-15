# Freeze — Sound Research Notes and Smooth-Mode Changes

Date: 2026-10-15

## Goal

At high Amount (50–100 %, "smooth mode") a short or rhythmic pluck input should become one static,
cohesive, natural sustained note: no audible grains, pulsing, flutter, clicks, metallic ringing, comb
filtering or phasey character, while keeping pitch and timbre. Below 50 % Amount the cloud stays exactly as
it is (bit-identical tests), and Amount 0 stays a bit-identical pass-through.

Fixed by earlier user decisions: 24-grain overlap in smooth mode, the Grain / Detune (0 = no detune) /
Noise / Freeze-Hold / FX controls and presets keep their meaning, the quiet 800 Hz high-passed spectral
layer stays, no UI changes and no new parameters.

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
| 1 | Freeze the body, not the onset: read 20–60 ms after onsets, level-match, cap make-up gain | `flat` copy flattens the envelope (make-up up to +18 dB) and dips 2 ms / 4 ms at note starts; grain centres may sit on attacks (the loudest segments) | **Do (change A)** |
| 2 | Asynchronous timing (±40–50 % jitter), position spray ≥ 80 ms, min. 30–50 ms between concurrent read positions (GRM) | ±25 % jitter; positions uniform over 1.5 s of loud material; nothing stops two grains from reading almost the same spot | **Do spacing (change B)**; skip jitter (measured: ±50 % jitter changes envVar by less than the seed-to-seed spread, and random arrival times make the window sum itself fluctuate more). Spray is already 1.5 s. |
| 2b | "Peaky" (triangular / Gaussian) position distribution (Beads) | uniform among loud segments | Skip: there is no position control to centre it on, and concentrating positions works against the spacing rule |
| 3 | Hann / equal-power windows, 1/sqrt(N) normalisation | Hann table, power-sum normalisation `1/sqrt(L/spacing * 0.375)` | Already done (verified). Extended by a smooth-mode leveller **(change C)** because the measured flutter is level wander that normalisation cannot remove |
| 4 | Small inherent detune (~±6 cents) | Detune knob only | Skip: conflicts with "Detune 0 = no detune", and measured no gain (envVar unchanged or worse, adds chorus) |
| 5 | All-pass diffuser (≈4 stages, g ≈ 0.6, 5–30 ms) at high Amount (> 70 %) | none | **Do (change D)**, routed per grain so it never sums a signal with its own all-passed copy (no comb filtering while it fades in). Spike: halves the click measure |
| 6 | Dark tail: −2…−3 dB shelf above 5–7 kHz, HP ~90 Hz, reverb HF damping, delay feedback ≤ 35–40 % with HP (20 + 100·fb² Hz) / LP 6–8 kHz in the loop, no modulation in loops | wet not brighter than input; reverb damping per preset; delay loop LP 4 kHz, feedback up to 0.55 (Abyss) | Skip for now: FX run at every Amount, so changing them breaks "below 50 % unchanged" and changes the presets; the shelf would dull the timbre (see measurements). Recommended as a separate, user-approved FX pass |
| 6b | Ensemble 0.15–0.25 Hz, soft saturation 15–20 % | none | Skip: modulation is exactly the phasey/chorus character to avoid; saturation changes timbre |
| 7 | Välimäki / PaulStretch random-phase IFFT "glue" floor (8192–16384 points, 20–30 %) | 800 Hz high-passed oscillator-bank layer at 0.2 | Skip: the current layer is the user's choice and already steady; a random-phase floor is a larger change worth its own listening test. Recommended next |
| 7b | Per-grain random pan ~60 % (Clouds) | each channel runs its own independent cloud (fully decorrelated) | Skip: already wider than 60 % pan spread, and mono-safe |
| 8 | Smoothing of every parameter (20–50 ms), freeze on/off crossfade 80–150 ms | Amount / FX / Noise smoothed 50 ms in the processor; grain length and detune only affect new grains; Hold only stops recording, grains play on; new clouds fade in with the grain windows | Already done (tests: knob sweep, grain-knob move, hold toggle have no clicks) |

## Changes (ranked by expected audible benefit)

All of them act only while `smoothness > 0` (Amount above 50 %, the diffuser above 70 %), and none draws
extra random numbers below that, so the output below 50 % is bit-identical.

**A. Onset-aware body reading.** In the flattened copy a note start (or a join where silence was dropped)
now fades out over 2 ms as before, then stays silent for 10 ms and fades back in over 20 ms (previously a
4 ms fade-in), so the first 30 ms of every note (pick noise, bright attack) is no longer read in smooth
mode. Grain centres are only chosen in segments that start at least 40 ms after the last onset (falls back
to all loud segments if none qualify, e.g. very fast rolls). Expected: much less attack timbre in the
cloud, fewer "pings". Test: 3.5 kHz pick burst in the smooth cloud at least ~20 dB below the plain cloud.

**B. Read-position spacing.** Two grains reading almost the same place at the same time form a comb
filter (phasing). A new smooth-mode grain tries up to 4 candidate positions and keeps the first that
reads at least 40 ms away from every playing grain of the same direction (otherwise the one farthest away).
The comparison uses each grain's current read position, so detuned and reversed grains are handled. Test:
share of grains starting within 30 ms of a playing grain's read position drops sharply.

**C. Smooth-mode leveller.** The grain sum is measured with a 10 ms detector against its own 1 s average;
a gain limited to ±6 dB and smoothed over 10 ms pulls the level back to the average, faded in with
smoothness. This removes the slow random level wander (flutter / pulsing) without touching the timbre.
Test: envelope variation of harmonic plucks at full Amount clearly lower; level, click and sustain tests
unchanged.

**D. Grain-routed all-pass diffuser above 70 %.** Four Schroeder all-passes (g = 0.6; 5.3, 7.9, 12.7 and
19.1 ms, right channel slightly longer) on a second grain bus. From 70 % to 100 % Amount a growing share of
new grains (0 → 100 %, evenly spread like the reversed grains) is sent to that bus, so the diffuser fades in
grain by grain and the cloud is never mixed with its own all-passed copy. It smears what is left of grain
edges and de-click dips. Test: no change below 70 %, a lower click measure and the same level at 100 %.
