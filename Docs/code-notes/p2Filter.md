# p2Filter.c / p2Filter.h - notes

The filter as measured (design §3a, findings 2026-10-17): one resonant 4-pole loop, the mode choosing a mix of
its signals. Checked offline by `tools/do-filtercheck`.

## 1. The cutoff law
The pole frequency is the measured self-oscillation frequency, `23.93 Hz × 2^(cutoff / 11.977)`. Above about
23.6 kHz it bends over smoothly (the ceiling seen above cutoff 118). In an ideal loop the self-oscillation
frequency equals the pole frequency, so the plug-in rings exactly where the instrument does. The small-signal fit
preferred poles 5.6 % higher (`beta` = 1.056). That disagreement is open: the fit trades it against its free
output mixes.

## 2. The feedback
k against resonance, from the measured bass loss (`20·log10(1 + k)`). The bass loss is linear, so these are
the reliable numbers, unlike the peak heights, which compress. k reaches 4 at resonance 97, where the
instrument starts to oscillate, then rises 0.03 per step (provisional: it sets how hard the oscillation drives
the saturation).

## 3. The output mixes, shelf and top poles
From the free-mix linear fit (`tools/filter_fit.py`, 0.46 dB rms over every sweep): each mode is a weighted sum
of the input, the loop input and the four stage outputs. The shelf (+2.9 dB below about 200 Hz) and the 42.5 kHz
pole sit outside the loop. The fit also found a 5.7 kHz pole; it belongs to the EXT IN path the sweeps went through,
not to the filter: a saw from the instrument's own oscillator, filter open, is flat to 7.4 kHz (CT's recording, then
saws at four levels), and the pole had cut the engine's 3.7 dB at 3.7 kHz and 5.4 dB at 5.5 kHz. Removed 2026-10-19.
The noise keeps it as its own colour (p2Plugin notes §34). EXT IN captures only supplement oscillator captures now.

## 4. The saturation
`tanh(3.1·u)/3.1` at the loop input. The scale is chosen so a sine of amplitude 0.1 (−20 dBFS at EXT IN, in the
plug-in's units) gives a third harmonic near the measured −41 dB. Provisional: one saturation point, where the
instrument's distortion differs per output (findings). The level series will refine it.

## 5. The loop solve
Zero-delay: the TPT one-poles let stage 4's output be written as `G^4·u + S`, with S from the stage states.
So the loop input is solved exactly (`u = (x − k·S) / (1 + k·G^4)`) before the saturation, with no one-sample
delay in the feedback to detune the resonance.

## 6. The stopband
The first fit of the output mix (sweeps at -45 dBFS) could not see far below the cutoff, and left small weights on
the input and the first stage. They set a floor: far above a low cutoff the response stopped falling (LP24 about
48 dB down), where the instrument's goes on past 100 dB. A saw through LP12 and LP24 at cutoffs 0-127
(`tools/filter_mix_fit.py`, `lowcut/`) refitted just those weights; the later stages', which carry the resonant
shape, are as before. Weighted error over all the harmonics: LP12 3.5 -> 2.1 dB. LP24 5.1 -> 3.7. Removing LP24's leak outright
made P001/P013/P014 10-17 dB darker than the instrument: it does leak, only less than the first fit said. LP12's level low
down fell 0.9 dB, which it had been loud by. This was the plug-in's sustain being brighter than the instrument's at
low notes in P004/P007/P008/P009/P012 (CT).

LP12's whole mix is then scaled by 0.902 (-0.9 dB): with the filter open it measured 0.8-1.0 dB louder than the
instrument against LP24 (which matches within 0.2 dB), at oscillator levels 60 and 127 and at Volume 31 and 40.

## 7. LP24's mix, fitted to the noise (2026-10-19)
The instrument's own noise generator through the filter (`/Volumes/ext-ssd/Captures/Pulse2/noise_stopband/`: LP12
and LP24, cutoff 0-60 and open, resonance 0 and 41, note 60), each spectrum relative to its own open take, so the
noise's colour and the rig drop out. A linear model of the loop (four matched one-poles, feedback k, the six taps,
the shelf and the 42.5 kHz pole) was fitted by least squares on third-octave dB, bands under -55 dB of the open
level counting only when the model rose above them (the rig's floor).
- LP24: the leak (the input against the loop input) is real - without it no mix fits - but it is a near
  cancellation, -0.173 against +0.172, which the loop's saturation breaks if the loop input is taken after it (the
  engine came out 10-14 dB high in the stopband). So LP24's leak takes the loop input before the saturation. The
  engine is within about 3 dB of the instrument at every cutoff and resonance measured (it had a notch and a
  10 dB bump moving with the cutoff); P013 within 1 dB.
- LP12 keeps the mix of §6: a fitted LP12 mix matched the noise better but leant on the same kind of cancellation,
  and with real patches - three oscillators, the routed mix gain - the saturation broke it (P006's sustain 21-26 dB
  bright, several patches 4-5 dB). Its stopband stays about 2.5 dB high at 1-2.5 kHz at cutoff 0 (P006's noise,
  open).

## 8. The leak does not see the routing gain
With osc 3 routed away from MIX the instrument's mix reaches the filter 1.8 times as hot (p2Plugin notes §37) - but
only what passes through the filter rises. P006's noise alone with the filter shut (`p006_noise_steps/`): LEVEL2
against MIX raises the instrument's output +5 dB up to 250 Hz, +3.7 at 500, +1.7 at 1 kHz, +0.5 at 2 kHz, while its
stopband levels off near -45 dB - a leak floor, carried around the gain. The engine's leak terms (input and loop
input) therefore take the input without the routing gain (`leakScale`); P006's late noise went from +3.3 dB at
1-4 kHz to within ~1 dB in every octave, with nothing else in P006 moved, and nothing changes with osc 3 on MIX.

## 9. Coefficients for a value that moved (2026-10-24)
The engine sets the filter every host sample, and each set cost a tan() and three pow()s for G and the feedback
table for k, whether or not the cutoff or the resonance had changed. `p2_filter_set()` now keeps the cutoff and
resonance it last worked them out for (`known`, `cutoffSeen`, `resonanceSeen`) and does it again only for a value
that differs; `p2_filter_init()` clears `known`. A held note with the cutoff still costs nothing; an envelope on
the cutoff costs what it did. Byte-identical renders.
