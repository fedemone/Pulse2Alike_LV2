/*
 * Pulse2Alike - the filter: one resonant 4-pole loop, four outputs.
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
// Notes: Docs/code-notes/p2Filter.md - "// notes §k" refers there.

#include <math.h>
#include <string.h>

#include "p2Filter.h"

// notes §1
#define OSC_HZ_AT_ZERO          (23.93)
#define CUTOFF_STEPS_PER_OCT    (11.977)
#define OSC_HZ_CEILING          (23600.0)
#define CEILING_SHARPNESS       (8.0)

// notes §2
static const double kFeedbackAt[][2]        = {
    { 0.0, 0.00}, {16.0, 0.06}, {32.0, 0.78}, {48.0, 1.54}, {64.0, 2.32},
    {80.0, 3.15}, {88.0, 3.52}, {92.0, 3.78}, {96.0, 3.97}, {97.0, 4.00},
};
#define FEEDBACK_ABOVE_97    (0.03)

// notes §3
static const double kMix[eP2FilterCount][6] = {
    {-0.17298,  0.17199, 0.00074, -0.05179,  0.23875, 1.54050}, // LP24 - notes §7: fitted to the noise through the filter
    { 0.00000, -0.00068, 0.00144,  1.54098, -0.28386, 0.16416}, // LP12 - the 10-19 leak refit, x 0.902
    {  0.0039,  -0.0118,  0.9137,  -0.9947,   0.1210, -0.0345}, // BP12
    { -0.0032,   0.4867, -1.0194,   0.4379,   0.2133, -0.1162}, // HP12
};
#define SHELF_ZERO_HZ    (250.2)
#define SHELF_POLE_HZ    (179.3)
#define TOP2_HZ          (42533.0)

// notes §4
#define LOOP_DRIVE       (3.1)

double p2_filter_oscillation_hz(double cutoff) {
    double hz = OSC_HZ_AT_ZERO * pow(2.0, cutoff / CUTOFF_STEPS_PER_OCT);

    return hz / pow(1.0 + pow(hz / OSC_HZ_CEILING, CEILING_SHARPNESS), 1.0 / CEILING_SHARPNESS);
}

double p2_filter_feedback(double resonance) {
    size_t n = sizeof(kFeedbackAt) / sizeof(kFeedbackAt[0]);

    if (resonance <= kFeedbackAt[0][0]) {
        return kFeedbackAt[0][1];
    }

    if (resonance >= kFeedbackAt[n - 1][0]) {
        return kFeedbackAt[n - 1][1] + ((resonance - kFeedbackAt[n - 1][0]) * FEEDBACK_ABOVE_97);
    }

    for (size_t i = 1; i < n; i++) {
        if (resonance <= kFeedbackAt[i][0]) {
            double t = (resonance - kFeedbackAt[i - 1][0]) / (kFeedbackAt[i][0] - kFeedbackAt[i - 1][0]);

            return kFeedbackAt[i - 1][1] + (t * (kFeedbackAt[i][1] - kFeedbackAt[i - 1][1]));
        }
    }

    return kFeedbackAt[n - 1][1];
}

static double tpt_gain(double hz, double sampleRate) {
    double limit = 0.49 * sampleRate;
    double g     = tan(M_PI * fmin(hz, limit) / sampleRate);

    return g / (1.0 + g);
}

void p2_filter_init(tP2Filter * f, double sampleRate) {
    memset(f, 0, sizeof(*f));
    f->leakScale  = 1.0;
    f->sampleRate = sampleRate;

    // first-order shelf by the bilinear transform, prewarped at its own corners
    double z = tan(M_PI * SHELF_ZERO_HZ / sampleRate);
    double p = tan(M_PI * SHELF_POLE_HZ / sampleRate);
    double a = 1.0 + p;

    // H(s) = (1 + s/wz) / (1 + s/wp), bilinear: unity at DC, p/z at the top
    f->shelfB0    = (1.0 + (1.0 / z)) * p / a;
    f->shelfB1    = (1.0 - (1.0 / z)) * p / a;
    f->shelfA1    = (1.0 - (1.0 / p)) * p / a;
    f->top2G      = tpt_gain(TOP2_HZ, sampleRate);
    p2_filter_set(f, 127.0, 0.0, eP2FilterLp24);
}

void p2_filter_reset(tP2Filter * f) {
    memset(f->stage, 0, sizeof(f->stage));
    f->shelfX = f->shelfY = f->top2 = 0.0;
}

// notes §9: set every sample, so the tan() and pow()s are spent only on a value that moved
void p2_filter_set(tP2Filter * f, double cutoff, double resonance, tP2FilterMode mode) {
    if (!f->known || (cutoff != f->cutoffSeen)) {
        f->G          = tpt_gain(p2_filter_oscillation_hz(cutoff), f->sampleRate);
        f->cutoffSeen = cutoff;
    }

    if (!f->known || (resonance != f->resonanceSeen)) {
        f->k             = p2_filter_feedback(resonance);
        f->resonanceSeen = resonance;
    }
    f->known   = true;
    memcpy(f->mix, kMix[((int)mode >= 0) && (mode < eP2FilterCount) ? mode : eP2FilterLp24], sizeof(f->mix));
    f->lowPass = (mode == eP2FilterLp24);   // notes §7: LP12 fits better with its leak after the saturation
}

static double one_pole(double * state, double G, double x) {
    double v = (x - *state) * G;
    double y = v + *state;

    *state = y + v;
    return y;
}

// notes §5
double p2_filter_process(tP2Filter * f, double x) {
    double G     = f->G;
    double G2    = G * G;
    double S     = 0.0;

    // what stage 4 would output with zero loop input, each state carried through the stages after it
    S         = (G2 * G * (1.0 - G) * f->stage[0]) + (G2 * (1.0 - G) * f->stage[1]) + (G * (1.0 - G) * f->stage[2]) + ((1.0 - G) * f->stage[3]);

    double u     = (x - (f->k * S)) / (1.0 + (f->k * G2 * G2));
    double uMix  = u;    // notes §7: the low-passes' leak takes the loop input before the saturation

    u         = tanh(LOOP_DRIVE * u) / LOOP_DRIVE;

    double y1    = one_pole(&f->stage[0], G, u);
    double y2    = one_pole(&f->stage[1], G, y1);
    double y3    = one_pole(&f->stage[2], G, y2);
    double y4    = one_pole(&f->stage[3], G, y3);
    double out   = (f->leakScale * ((f->mix[0] * x) + (f->mix[1] * (f->lowPass ? uMix : u)))) + (f->mix[2] * y1) + (f->mix[3] * y2) + (f->mix[4] * y3) + (f->mix[5] * y4);

    double shelf = (f->shelfB0 * out) + (f->shelfB1 * f->shelfX) - (f->shelfA1 * f->shelfY);

    f->shelfX = out;
    f->shelfY = shelf;
    return one_pole(&f->top2, f->top2G, shelf);
}
