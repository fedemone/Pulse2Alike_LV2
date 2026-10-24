
## 3. Phase
FREE, RAND, 0, 90, 180, 270 (the instrument's own list; the manual does not describe it, and no CC reaches it).
FREE never resets the LFO; RAND starts it anywhere at each note; the others at that point of the cycle, measured:
with SAW at 0 the cutoff starts at the bottom of the ramp, and 90/180/270 move the reset a quarter, half and three
quarters of the period earlier.

## 4. Which way each shape starts
From a note at phase 0 (findings, "LFO phase and direction" and "P011"). SAW and sSAW rise from the bottom (a
positive amount raises the cutoff). The fast SINE, TRI and SQR start at the middle going DOWN (SQR low for the first
half). The slow sSIN, sTRI and sSQR start at their TOP and go down (sSQR high first). The slow ones were first read
from the self-oscillating filter, where the start of the take is uncertain; P011's pulse width settled it: with
the filter open, its sTRI on the width starts wide and narrows steadily from the first window.

## 5. The slow shapes' rate
Not a fixed fraction of the fast law: measured with sTRI on the self-oscillating filter's cutoff at speeds 4, 16,
32, 48, 64 and 96 (0.0229, 0.0763, 0.214, 0.610, 1.648, 9.98 Hz), which is 0.11 of the fast law at speed 4,
0.20 at 32 and 0.24 at 64. Interpolated between the points on a log scale and extended at the ends. The 0.197
first used held only near speed 32, where it was measured.

## 6. The rate, kept (2026-10-24)
The rate's pow() (or the slow shapes' table) was worked out every control tick. `p2_lfo_step()` keeps it with the
speed and the kind of shape (fast or slow) it was for, and works it out again when either moves - a modulated LFO 1
Speed does that each tick, as before. `rateKnown` starts false in the engine's zeroed instance. Byte-identical renders.
