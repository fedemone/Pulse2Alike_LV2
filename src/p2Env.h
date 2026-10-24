/*
 * Pulse2Alike - the envelope generator (both envelopes).
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
// Notes: Docs/code-notes/p2Env.md - "// notes §k" refers there.

#ifndef __P2_ENV_H__
#define __P2_ENV_H__

#include <stdbool.h>

typedef enum {
    eP2EnvIdle = 0,
    eP2EnvAttack,
    eP2EnvDecay,
    eP2EnvRelease
} tP2EnvStage;

typedef struct {
    tP2EnvStage stage;
    double      level;         // 0..1, the generator\'s own level (the VCA curve is applied after)
    double      phase;         // attack: 0..1 along the curve
    double      attackFrom;    // the level the attack started from (a retrigger mid-release)
    bool        rising;        // D-D loop: decaying back up towards full
    double      timeScale;     // notes §6: decay and release times x this (the filter envelope's are slower)
    // notes §7: the last step's rate, for the stage, dial value and dt it was worked out for
    tP2EnvStage stepStage;
    double      stepValue, stepDt, stepScale, stepRate;
} tP2Env;

typedef enum {
    eP2LoopOff = 0,
    eP2LoopAttackDecay,        // A-D: attack again from the sustain level, once decay has reached it
    eP2LoopDecayDecay          // D-D: between full and the sustain level, both ways at the decay rate
} tP2EnvLoop;

double p2_env_attack_seconds(double value);     // to −1 dB
double p2_env_decay_seconds(double value);      // to −40 dB; release uses the same law
double p2_env_sustain_level(double value);      // the generator's level, 0..1
double p2_env_vca_gain(double e);               // the VCA's amplitude for an envelope level
double p2_vca_gain(double x);                   // the VCA curve, of envelope × volume, 0..1

void p2_env_reset(tP2Env * e);
void p2_env_gate_on(tP2Env * e);
void p2_env_gate_off(tP2Env * e);
// One step of dt seconds with the four dial values (0..127); returns the level.
double p2_env_step(tP2Env * e, double dt, double attack, double decay, double sustain, double release, tP2EnvLoop loop);

#endif // __P2_ENV_H__
