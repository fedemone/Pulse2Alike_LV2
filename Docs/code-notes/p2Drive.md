# p2Drive.c - notes

The drive is analogue, at the VCA after the filter ("Amplifier with saturation", the end of the chain). The
curve picks one of the instrument's circuits; Drive sets how hard it is driven. NONE passes the signal untouched,
and Drive then does nothing (measured: identical output at every Drive).

## 1. The curves
Each is: gain set by Drive, then a clipper with its own positive and negative ceilings, with a bias and a coupling
capacitor charged by its own output (which moves the clipping point as it is driven, the slope seen on the
captured flats), then a one-pole shelf. TUBE is lopsided (the manual's "asymmetrical", strong 2nd harmonic),
FUZZ symmetric and close to a square. Fitted cycle by cycle to 1 kHz sines through EXT at three levels and five
Drive settings (`tools/drive_fit.py`), within about 13% (TUBE) and 11% (FUZZ) of the peak. Checked against a saw
it was not fitted to: levels relative to NONE within 0.4-2.6 dB, FUZZ's odd harmonics within 2 dB; TUBE's 3rd
harmonic about 15 dB weak and FUZZ's even ones about 10 dB weak. **Approximate.**

## 2. Units
The curves were measured as the instrument's output at Volume 40. The engine's signal before the VCA maps onto
that by 0.888: the engine at Volume 40 is 7.9 dB below the instrument (the same saw: −22.0 against −14.1 dBFS),
times the VCA at 40 and the plug-in's output gain.

## 3. FX
Not in the manual: it selects the return of an effects board planned for the instrument and never made. With no
board, nothing comes back, and the instrument is silent (measured, with the external input and with the
oscillators, at every Drive). Anomalies on: silent, as the instrument. Off: as NONE.

The 0.888 is in the instrument's units and does not depend on the plug-in's output gain: when that gain rose to
match the instrument (p2Plugin notes §19), the engine's level before the VCA was unchanged.

## 3. FUZZ, refitted to the instrument's own oscillator (2026-10-19)
A triangle from osc 1 through the open LP24, captured with Drive Curve NONE (the input) and FUZZ at Drive 0, 16,
32, 64, 96 and 127, at osc levels 127 and 64 (`/Volumes/ext-ssd/Captures/Pulse2/drive_tri/`). The same structure as
§1, fitted cycle by cycle with the NONE take as the input in output units: 7% of the peak against 23% for the 1 kHz
EXT IN fit, which it replaces. Drive 16 is now a table point (P003 sits at 17).
Open, both seen in the same captures:
- below about -35 dB of input the instrument's FUZZ falls away faster than its input (osc level 16: -6 dB at Drive 0,
  where level 24 gains 2 dB) - an expansion this structure cannot make. The engine is 6-14 dB loud there, and P003's
  sustain (filter closed to 24) is there: P003 is 6-9 dB loud and 20 dB dark. Two expander forms (after the gain, on
  the raw input) did not fit; the shapes say the small-signal path is rounded, not gated.
- TUBE fits neither this structure nor an input-coupled one: its positive part narrows as Drive rises, a bias that
  moves with Drive, which the feedback coupling here cancels. TUBE keeps its 1 kHz fit.

## 4. What the level sweep showed, and the two ways forward (2026-10-19)
Captures: `/Volumes/ext-ssd/Captures/Pulse2/drive_sweep/` - osc 1's triangle through the open LP24 at 17 levels
(12-127) at note 48, and 5 levels at notes 72 and 84; each with Drive Curve NONE (the input) and FUZZ and TUBE at
Drive 0, 16, 32, 64, 127. `drive_dyn/` holds 3 s notes at quiet levels.
- Both curves limit hard: above about -31 dB of input the output sits at -25 to -27 dB (Volume 40).
- Both are static shapes: the loss at the top follows the harmonic number, not the frequency, at all three pitches
  (FUZZ at Drive 16: harmonic 3 down 1.4-3 dB, harmonic 17 down 5.5-10.5 dB), so the rounding comes from the curve,
  not from a filter. FUZZ is a symmetric soft clip whose tops dome (still following the triangle); TUBE is
  asymmetric, a pulse whose duty changes with Drive (harmonic 3 about 14 dB down, 5 only 3-5 dB at Drive 16).
- Quiet inputs come out disproportionately quiet (FUZZ Drive 0: 4-5 dB of output per dB of input below about
  -35 dB), but the waveform shows no dead zone at the zero crossings, so it is not a static curve. Over a 3 s note
  at level 18 the FUZZ gain settles from -5.8 to -3.0 dB against NONE in about 0.75 s; 0.7 s notes in the sweep read
  -6.7 dB for the same case - settling, and possibly drift between sessions or dependence on what played before
  (bias or coupling capacitors charging).
- Structures tried and rejected: a coupling loop around the clipper (cancels any bias); input coupling + bias +
  clipper; expanders after the gain and on the raw input; a diode-like dead zone (right levels, wrong shapes:
  steps at the zero crossings the instrument does not have); a 957 Hz output low-pass (a fit to one pitch; 20-30 dB
  wrong at notes 72 and 84). Fit tools: a C copy of the drive model driven from Python (scipy least squares), one
  aligned cycle per take, plus harmonic-spectrum scoring - spectra, not waveforms, decide, as phase is not heard.

Two ways forward (todo, top):
1. Pragmatic, for P003 now: keep the static FUZZ fit (§3) and add a level-dependent gain fitted to the sweep's
   output levels, so quiet inputs come out as quiet as the instrument's. Brings P003's sustain (6-9 dB loud) to
   within a couple of dB; does not reproduce the settling or the exact quiet-input shape. About half an hour, no
   new captures.
2. Thorough, both curves: model the quiet-input behaviour as what it appears to be - a slow settling (bias or
   coupling) - from long notes repeated at several quiet levels and Drives, with the settling's time constant and
   its dependence on the previous note; then fit the static curves as measured shapes (a table or spline per
   Drive, not a formula), FUZZ symmetric and TUBE asymmetric with its moving duty. Several hours, with the rig to
   itself for stretches.

## 5. Quiet inputs: the ceilings fall with the input's level (2026-10-20, way 1 of §4)
Against the level sweep (`tools/drive_level.py`, the model run on each NONE take beside the instrument's take), the
static curves match above about -31 dB of input RMS and the instrument falls away below it: at Drive 16-127 FUZZ
loses 1.3-1.6 dB more per dB of input, TUBE 1.7-1.8; Drive 0 is steeper. The instrument's quiet output is still
clipped (FUZZ's 3rd harmonic -16 dB at osc level 16, as loud as at 40), so the loss is not a gain after the curve:
the clipper's ceilings (and its bias) are lowered instead, by `trim - slope x (knee - level)` dB below the knee,
with knee, slope and trim per Drive point (fitted, C model driven from scipy). Note 48: FUZZ within 0.25-0.3 dB rms
at Drive 16-127, 0.9 at Drive 0; TUBE 0.4-1.7. The input's level is a meter on its power, 3 ms up and 60 ms down,
which reads 3.15 dB over a steady triangle's RMS at notes 48, 72 and 84 alike (subtracted).
Open: notes 72 and 84 lose less (2.6 dB rms with note 48's numbers) - the loss depends on pitch; the settling seen
over long quiet notes (§4) is not modelled; the 7th and 9th harmonics are short at every level (the static shape).
P003 (FUZZ 17, note 48) against its NONE: the instrument -0.5 dB, the plug-in +5.8 before, +2.2 now; FUZZ's added
250 Hz-1 kHz now +6 dB as the instrument's +7. Its remaining darkness is mostly upstream: P003 on NONE is already
about 10 dB darker in the plug-in than on the instrument above 250 Hz.

## 6. The curve's knee, fitted to harmonics (2026-10-20)
The clipper's tanh became `x / (1 + |x|^h)^(1/h)` (scaled by the ceiling): h near 1 is round, large h nearly square,
and tanh sits about h = 2.5. `tools/drive_shape.py` fits it with the gain per Drive, the ceilings, bias, coupling and
shelf to harmonics 1-15 of the level sweep (inputs above the §5 knee; notes 48, 72, 84) and the EXT IN sines (100 Hz
to 3 kHz, -20 and -10 dBFS), with the C model in `tools/drive_model.c`; then §5's loss is refitted on the new shape
over every level. Spectra decide, as in §4.
FUZZ: one hardness for both sides (12.5) - a free pair went lopsided to copy the even harmonics the sweep's triangle
brings in, and the sines show FUZZ makes none of its own. The ceilings and bias stay free: forced equal and zero, the
coupling loop loses its operating point and the fit falls apart (17 dB rms). Harmonic rms over all takes 11.4 dB with
the old shape, 4.3 with this one; on the sines at -20/-10 dBFS 2.6-4.9 dB against 4.7-10.6. Its one loss: at
-30 dBFS input the small remaining asymmetry gives even harmonics about 15 dB above the instrument's. P003 against
its NONE: level -1.1 dB (instrument -0.5); FUZZ adds +7.0 dB at 250 Hz-1 kHz (+7), +13.5 at 1-4 kHz (+24.5) and +9.5
at 4-16 kHz (+11.5) - it added nothing above 1 kHz before.
TUBE: each side its own knee (1.71 positive, 1.96 negative), every other constant shared: harmonic rms 10.9 -> 8.2 dB on
the fitted takes, and better than the old shape on most of the sines (worse at Drive 0 with -30 dBFS in). Its bias
sits near the fit's bound (0.495 of 0.5), so it stays **approximate**. A bias per Drive (the moving bias of §3) was
tried and rejected: the five went to the bounds with alternating signs (+1 +1 -1 +1 -1), the clipper saturated by the
bias and the coupling loop undoing it - 0.8 dB better on paper, nothing physical. §1's and §3's constants are
superseded by these; their measurements stand.
Fit files: `drive_shape.py <curve> shape`, then `loss`, then `sines` to compare with the old shape; the result is
written to $TMPDIR/p2_drive_shape_<curve>.json and copied into the tables by hand (Drive 96 interpolated).

## 7. What is worked out once (2026-10-24)
A curve's coupling and shelf coefficients depend only on the rate, and its gain, bias, knee, slope and trim only on
Drive, but each was worked out again for every oversampled sample - three exp() and a pow() per sample at 4x, which
on the MOD Dwarf (glibc 2.27, before the fast exp/log/pow of 2.28) was a large part of the drive's cost.
`follow_shape()` now keeps them, the coefficients until the curve changes and the per-Drive values until Drive does
(osc 3 on DRIVE moves it every sample, and then they are worked out every sample as before). The same values,
computed by the same expressions: renders are byte-identical before and after, x86 and aarch64 alike.
