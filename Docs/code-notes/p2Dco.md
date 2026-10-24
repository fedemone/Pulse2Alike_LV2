# p2Dco.c / p2Dco.h - notes

## 1. The pitch origin
Pitch 0 is MIDI note −36, so a MIDI note is `n·256 + 36·256`. The range is 196 semitones (`0..0xC400`).

## 2. The base periods
The period of each semitone of the bottom octave, in two-tick units, is `floor(24 MHz / f)`, with `f` the
equal-tempered frequency of MIDI note `i − 36`. `tools/dcocheck` checks the law against the frequencies captured
from the instrument (design §2.1).

The 13 periods (the octave and the next C, which the slope to B needs) are a table, `kBasePeriod` (2026-10-24):
worked out with pow() they cost two calls per oscillator per control tick - with unison or PARA-8 and Anomalies
off, a third of the engine's time. Each value is at least 0.05 of a tick from the next integer, so no rounding in
pow() could have moved a floor(); `tools/dcocheck` checks the table against the pow() law at every pitch.

## 3. The period law
`(base − slope·frac/256) >> (octave + 1)`. The period, not the frequency, is interpolated between semitones,
which is design §2.1's 0.7 cent. Integer arithmetic throughout, so the reproduction lands on whole-tick periods,
which is what the captured pitches agree with.

## 4. The pulse width
design §2.2: the pulse is high for ((W + 512)·W >> 15) / 65536 of the cycle, W = PW << 8 (at most `0x7E00`, 49.2%),
the same at every pitch - measured to ±0.001 at PW 40-127 on C4 and F#4 (findings, "pulse width", 2026-10-23).
Osc 3's SQR (`P2_WIDTH_SQUARE`) is an exact half cycle, measured to ±0.0003: its even harmonics sit 62-87 dB down
on the instrument, which a 49.2% pulse could not do.

## 5. polyBLEP
Two-sample polynomial band-limited step, the usual one. A stand-in until the captures say what the edges look
like. The real edges are analogue and slower than a step.

## 6. The wave shapes
An ideal saw, triangle and pulse with polyBLEP edges. The phase resets at the main edge. The pulse leaves the DCO
low for its last `pulseFraction` of the cycle, so that after the mix's inversion (p2Plugin notes §39) it is high
for that fraction, as the instrument's is; the saw comes out of the mix falling and jumping up, as the
instrument's does. Both checked against captures through the same chain (findings, "pulse width").

## 7. APW
Measured (findings, "APW, corrected"): over a two-cycle period, the first cycle is a plain 50% square whatever the
width, and the second is high on [½ − w, ½] and [½ + w, 1], w from the PW law. At PW 127 (w = ½) that is an
ordinary square; as the width falls it becomes a square an octave down, which is APW's character. The first
model (the width alternating w / 1 − w) matched harmonic levels in places but not this shape, and missed a patch
built on the octave below (P011) by 15 dB. Every edge is band-limited across the two-cycle period.
## 8. Sync
Measured (findings, "sync"): with OSC3 SYNC>OSC2, osc 3 is the master, and every one of its cycles starts osc 2
afresh. Osc 2 runs one cycle and then holds at the end of it until the next restart. So a slave tuned above
the master plays its ramp and then a flat stretch, the shape of a timer that the master's edge restarts and that
stops when its own period runs out. A slave tuned below the master is cut short by the next restart.
