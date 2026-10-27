/*
 * Pulse2Alike - the digitally controlled oscillator.
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
// Notes: Docs/code-notes/p2Dco.md - "// notes §k" refers there.

#include <math.h>

#include "p2Dco.h"

int32_t p2_pitch_from_midi_note(double note) {
    return (int32_t)lround(note * P2_PITCH_PER_SEMITONE) + P2_PITCH_MIDI_ZERO;
}

// notes §2: floor(24 MHz / f) for the 13 semitones from MIDI note -36 up - a table, as the instrument's would be,
// because the pow() it was worked out with cost two calls per oscillator per control tick. tools/dcocheck checks it
// against that law at every pitch.
static const uint32_t kBasePeriod[13] = {
    23483943, 22165891, 20921815, 19747563, 18639218, 17593079, 16605655,
    15673651, 14793957, 13963636, 13179917, 12440185, 11741971
};

static uint32_t base_period(int32_t semitoneInOctave) {
    return kBasePeriod[semitoneInOctave];
}

// notes §3
uint32_t p2_dco_period_ticks(int32_t pitch) {
    if (pitch < 0) {
        pitch = 0;
    } else if (pitch > P2_PITCH_MAX) {
        pitch = P2_PITCH_MAX;
    }
    int32_t  semitone = pitch / P2_PITCH_PER_SEMITONE;
    int32_t  fraction = pitch % P2_PITCH_PER_SEMITONE;
    int32_t  note     = semitone % 12;
    int32_t  octave   = semitone / 12;
    uint32_t base     = base_period(note);
    uint32_t slope    = base - base_period(note + 1);

    return (base - ((slope * (uint32_t)fraction) >> 8)) >> (octave + 1);
}

// notes §4
// notes §4
double p2_dco_pulse_fraction(int32_t width) {
    if (width <= 0) {
        return 0.0;
    }

    if (width >= P2_WIDTH_SQUARE) {
        return 0.5;
    }

    if (width > P2_WIDTH_MAX) {
        width = P2_WIDTH_MAX;
    }
    return (double)(((uint32_t)(width + 0x200) * (uint32_t)width) >> 15) / 65536.0;
}

void p2_dco_set(tP2Dco * dco, int32_t pitch, int32_t width, tP2Shape shape, double sampleRate) {
    uint32_t period = p2_dco_period_ticks(pitch);

    dco->increment     = (P2_DCO_CLOCK_HZ / (double)period) / sampleRate;
    dco->pulseFraction = p2_dco_pulse_fraction(width);
    dco->shape         = shape;
}

// notes §5
static double poly_blep(double t, double dt) {
    if (t < dt) {
        t /= dt;
        return (t + t) - (t * t) - 1.0;
    }

    if (t > (1.0 - dt)) {
        t = (t - 1.0) / dt;
        return (t * t) + (t + t) + 1.0;
    }
    return 0.0;
}

// A step at position `edge` of a two-cycle period, seen from u (both 0..2): poly_blep's correction, or 0 when far
static double edge_blep(double u, double edge, double dt) {
    double d = u - edge;

    d += (d < 0.0) ? 2.0 : 0.0;

    if (d < dt) {
        return poly_blep(d, dt);
    }
    return (d > (2.0 - dt)) ? poly_blep(d - 1.0, dt) : 0.0;
}

// notes §7: over two cycles - a plain square, then high on [1.5 - w, 1.5) and [1.5 + w, 2)
static double apw(double u, double w, double dt) {
    double out = ((u < 0.5) || ((u >= (1.5 - w)) && (u < 1.5)) || (u >= (1.5 + w))) ? 1.0 : -1.0;

    out -= edge_blep(u, 0.5, dt);           // falls
    out -= edge_blep(u, 1.5, dt);
    out += edge_blep(u, 1.5 - w, dt);       // rises
    out += edge_blep(u, 1.5 + w, dt);
    return out;
}

// notes §6
void p2_dco_restart(tP2Dco * dco) {
    dco->phase = 0.0;
    dco->held  = false;
}

float p2_dco_next(tP2Dco * dco) {
    double t  = dco->held ? 0.999999 : dco->phase;     // notes §8: a held slave sits at the end of its cycle
    double dt = dco->held ? 0.0 : dco->increment;
    double out;

    switch (dco->shape) {
        case eP2ShapeSaw:
            out = ((2.0 * t) - 1.0) - poly_blep(t, dt);
            break;

        case eP2ShapeTriangle:
            out = (t < 0.5) ? ((4.0 * t) - 1.0) : (3.0 - (4.0 * t));
            break;

        case eP2ShapeApw:
            out = apw(t + ((dco->cycle & 1) ? 1.0 : 0.0), dco->pulseFraction, dt);
            break;

        case eP2ShapePulse:
        default:
        {
            double width = dco->pulseFraction;
            double rise  = 1.0 - width;
            double fall  = t - rise;

            if (fall < 0.0) {
                fall += 1.0;
            }
            out  = (t >= rise) ? 1.0 : -1.0;
            out += poly_blep(fall, dt) - poly_blep(t, dt);
            out  = -out;            // notes §6: high for the width's fraction once the mix has inverted it
            break;
        }
    }
    dco->phase  += dt;
    dco->wrapped = false;

    if (dco->phase >= 1.0) {
        if (dco->syncHold) {
            dco->held  = true;
            dco->phase = 0.999999;
        } else {
            dco->phase  -= 1.0;
            dco->wrapped = true;
        }
        dco->cycle++;
    }
    return (float)out;
}
