# p2Env.c / p2Env.h - notes

One generator for both envelopes, from the amp-envelope captures (findings 2026-10-17). The filter envelope uses
the same laws until its own session says otherwise.

## 1. The tables
Measured every 8 steps plus 127: attack to −1 dB, decay/release to −40 dB, sustain in dB. Times are interpolated
on a log scale, since they grow by ratios (doubling every 8 steps through the middle). The 127 entries are
extrapolated from the curve's shape (attack: −12 dB at 34 s; release: −20 dB at 39.8 s); value 0's sustain is
silence.

## 2. Generator and VCA curve
The generator is simple: a linear attack from 0 to 1, then an exponential decay towards the sustain level
(S/127), and an exponential release towards 0. What the ear hears is the VCA's response to it, `p2_env_vca_gain()`.
That response is the measured sustain table read as a curve of the envelope's level. One curve explains three
separate measurements (findings, "one generator, one VCA curve"):
- the sustain levels;
- the amp attack's slow start (−40/−20/−6 dB at E ≈ 0.12/0.38/0.71, against 0.13/0.39/0.73 measured);
- the filter envelope decaying more slowly in log terms than the amp envelope (E's time constant 0.48 s against
  0.53 s measured at decay 64).

The times were measured at the output, so they convert to the generator through where the curve crosses
−1 dB (E = 0.955) and −40 dB (E = 0.124). The filter envelope is the same generator with no curve: it adds
2 × (amount − 64) × E cutoff steps.

## 3. Retriggering
A retrigger starts the linear attack from wherever the level is.

## 5. Loops
Measured on the amp envelope (findings, "envelope loops"). **A-D:** once decay has reached the sustain level,
attack again from there (not from zero); the cycle at A 40, D 50, S 64 was 1.05 s. **D-D:** decay to the sustain
level, then back up to full at the same decay rate, and so on. Sustain sets the lower level; the period hardly
changes with it (1.9 / 1.75 / 1.6 s at S 32 / 64 / 96). The manual's "full scale to zero ... Sustain controls the
speed" is not what the instrument does. A loop turns when the decay has arrived within 1/1024 of its target, which is why it waits at the sustain level
before A-D's next attack. The threshold is from the captures: the three waits (A-D S 64, D-D S 32 and 96) each
give 1/1000 or so, given the decay's own time constant.

## 6. The filter envelope is slower
Measured on the self-oscillating filter's cutoff (amount full, Sustain 0) at Decay 48, 56, 63 and 72: the filter
envelope's time constant is 1.08 times the engine's at every one, while the amp envelope (times to -20 and -30 dB
at Decay 56 and 63) matches within about 2%. The two share the table, which was measured through the amp envelope;
the filter envelope's decay and release are scaled by 1.08 (timeScale). Its release is assumed to go with its decay.

## 7. The step's rate, kept (2026-10-24)
A step's increment (attack) or multiplier (decay, release) is a pure function of the stage, its dial, dt and
timeScale, but cost a table lookup on a log scale and an exp() every control tick - every host sample with
Anomalies off. `step_rate()` keeps the last one with the four it was worked out for, and works it out again only
when one of them differs. The same expressions as before, so renders are byte-identical.
