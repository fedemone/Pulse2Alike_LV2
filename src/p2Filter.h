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

#ifndef __P2_FILTER_H__
#define __P2_FILTER_H__

#include <stdbool.h>

typedef enum {
    eP2FilterLp24 = 0,
    eP2FilterLp12,
    eP2FilterBp12,
    eP2FilterHp12,
    eP2FilterCount
} tP2FilterMode;

typedef struct {
    double sampleRate;

    // coefficients, from p2_filter_set()
    double G;               // one stage's TPT gain
    double k;               // loop feedback
    double mix[6];          // input, loop input, stages 1-4
    double shelfB0, shelfB1, shelfA1;
    double top2G;
    bool   known;               // notes §9: G and k are for cutoffSeen and resonanceSeen
    double cutoffSeen, resonanceSeen;

    // state
    double stage[4];
    double shelfX, shelfY;
    double top2;
    bool   lowPass;
    double leakScale;   // notes §8: the leak around the loop, which a gain ahead of the filter does not reach
} tP2Filter;

double p2_filter_oscillation_hz(double cutoff);
double p2_filter_feedback(double resonance);

void p2_filter_init(tP2Filter * f, double sampleRate);
void p2_filter_reset(tP2Filter * f);
void p2_filter_set(tP2Filter * f, double cutoff, double resonance, tP2FilterMode mode);
double p2_filter_process(tP2Filter * f, double x);

#endif // __P2_FILTER_H__
