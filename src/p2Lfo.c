/*
 * Pulse2Alike - the LFOs.
 *
 * Copyright (C) 2026 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If
 * not, see <https://www.gnu.org/licenses/>.
 */
// Notes: Docs/code-notes/p2Lfo.md - "// notes §k" refers there.

#include <math.h>
#include <stdbool.h>

#include "p2Lfo.h"

// notes §1
#define HZ_AT_64            (6.897)
#define SPEED_PER_OCTAVE    (12.0)
#define SHAPE_COUNT_FAST    (5)

double p2_lfo_hz(double speed) {
    return HZ_AT_64 * pow(2.0, (speed - 64.0) / SPEED_PER_OCTAVE);
}

// notes §5: the slow shapes' rate, measured at six speeds and interpolated on a log scale (extended at the ends)
static const double kSlowSpeed[] = {4.0, 16.0, 32.0, 48.0, 64.0, 96.0};
static const double kSlowHz[]    = {0.0229, 0.0763, 0.2136, 0.6104, 1.6479, 9.9792};
#define SLOW_POINTS    ((int)(sizeof(kSlowSpeed) / sizeof(kSlowSpeed[0])))

static double slow_hz(double speed) {
    int    i = 0;

    while ((i < SLOW_POINTS - 2) && (speed > kSlowSpeed[i + 1])) {
        i++;
    }
    double t = (speed - kSlowSpeed[i]) / (kSlowSpeed[i + 1] - kSlowSpeed[i]);

    return kSlowHz[i] * pow(kSlowHz[i + 1] / kSlowHz[i], t);
}

static double next_random(tP2Lfo * l) {
    if (l->random == 0) {
        l->random = 0x2545F491u;
    }
    l->random ^= l->random << 13;
    l->random ^= l->random >> 17;
    l->random ^= l->random << 5;
    return ((double)l->random / 2147483648.0) - 1.0;
}

// notes §3
void p2_lfo_note(tP2Lfo * l, tP2LfoPhase phase) {
    if (phase == eP2LfoPhaseRandom) {
        l->phase = (next_random(l) + 1.0) / 2.0;
    } else if (phase >= eP2LfoPhase0) {
        l->phase = 0.25 * (phase - eP2LfoPhase0);
    }
}

// notes §2
double p2_lfo_step(tP2Lfo * l, double dt, double speed, int shape) {
    int    base = shape % SHAPE_COUNT_FAST;

    // notes §6: the rate's pow() only when the speed or the kind of shape moves
    if (!l->rateKnown || (speed != l->speedSeen) || ((shape >= SHAPE_COUNT_FAST) != l->slowSeen)) {
        l->rateKnown = true;
        l->speedSeen = speed;
        l->slowSeen  = shape >= SHAPE_COUNT_FAST;
        l->hzSeen    = l->slowSeen ? slow_hz(speed) : p2_lfo_hz(speed);
    }
    double hz   = l->hzSeen;
    double p    = l->phase + (hz * dt);

    if (p >= 1.0) {
        p      -= floor(p);
        l->held = next_random(l);
    }
    l->phase = p;

    // notes §4: the fast SINE, TRI and SQR start at the middle going down; the slow ones at their top
    bool   slow = shape >= SHAPE_COUNT_FAST;

    switch (base) {
        case 0:  return slow ? cos(2.0 * M_PI * p) : -sin(2.0 * M_PI * p);

        case 1:

            if (slow) {
                return (p < 0.5) ? (1.0 - (4.0 * p)) : ((4.0 * p) - 3.0);
            }
            return (p < 0.25) ? (-4.0 * p) : ((p < 0.75) ? ((4.0 * p) - 2.0) : (4.0 - (4.0 * p)));

        case 2:  return (2.0 * p) - 1.0;

        case 3:  return ((p < 0.5) == slow) ? 1.0 : -1.0;

        default: return l->held;
    }
}
