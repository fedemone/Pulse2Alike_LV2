/*
 * Pulse2Alike - the drive: VCA saturation, after the filter.
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
// Notes: Docs/code-notes/p2Drive.md - "// notes §k" refers there.

#include <math.h>

#include "p2Drive.h"

// notes §2: the engine's level before the VCA, in the units the curves were measured in (the instrument's output
// at Volume 40, in full scale)
#define HARDWARE_UNITS_PER_ENGINE    (0.888)
#define DC_BLOCK_HZ                  (5.0)
#define DRIVE_POINTS                 (6)
#define LEVEL_ATTACK_S               (0.003)   // notes §5: the input level's meter, quick up, slow down
#define LEVEL_RELEASE_S              (0.060)
#define LEVEL_METER_OVER_RMS_DB      (3.15)    // what that meter reads over a steady triangle's RMS

typedef struct {
    double positive, negative;   // the clipper's ceilings
    double couplingHz;
    double shelfDirect, shelfHz; // how much passes straight, and the low-pass the rest goes through
    double hardPos, hardNeg;     // notes §6: each side's knee, 0 for tanh
    double gainDb[DRIVE_POINTS]; // at kDriveAt
    double bias[DRIVE_POINTS];   // notes §6: TUBE's moves with Drive
    // notes §5: below the knee (input RMS, dB of full scale) the ceilings fall `slope` dB per dB; trim above it
    double kneeDb[DRIVE_POINTS], slope[DRIVE_POINTS], trimDb[DRIVE_POINTS];
} tCurveShape;

static const double      kDriveAt[DRIVE_POINTS] = {0.0, 16.0, 32.0, 64.0, 96.0, 127.0};

// notes §6: TUBE fitted to the level sweep's and the EXT IN sines' harmonics, each side its own knee; approximate
static const tCurveShape kTube                  = {
    0.0835, 0.0360,  105.2,  0.925, 19119.0, 1.707, 1.955, {-8.37, -0.93, 3.55, 8.16, 10.51, 12.78},
    {0.495,  0.495,  0.495,  0.495,   0.495,0.495},
    {-25.98,-31.00, -31.62, -32.50,  -32.14,-31.80}, {2.55, 3.38, 3.05, 2.48, 2.31, 2.15}, {3.51, 1.78, 1.38, 0.57, 0.83, 1.08}
};
// notes §6: FUZZ fitted to the level sweep's and the EXT IN sines' harmonics, one knee for both sides
static const tCurveShape kFuzz                  = {
    0.0697,  0.0676,   388.1,   0.552,   139.3, 12.55, 12.55, {10.97, 16.67, 18.84, 22.27, 29.26, 36.03},
    {-0.0375,-0.0375, -0.0375, -0.0375, -0.0375, -0.0375},
    {-35.37, -33.29,  -30.94,  -30.49,  -29.39,-28.32}, {5.95, 3.06, 2.15, 1.93, 1.86, 1.79}, {-5.10, -2.39, -0.95, 0.04, 0.74, 1.41}
};

static double one_pole(double hz, double rate) {
    return 1.0 - exp(-2.0 * M_PI * hz / rate);
}

void p2_drive_init(tP2Drive * d, double rate) {
    d->rate      = rate;
    d->coupled   = 0.0;
    d->lowPass   = 0.0;
    d->dcBlock   = 0.0;
    d->dcIn      = 0.0;
    d->inPower   = 0.0;
    d->inAttack  = 1.0 - exp(-1.0 / (LEVEL_ATTACK_S * rate));
    d->inRelease = 1.0 - exp(-1.0 / (LEVEL_RELEASE_S * rate));
    d->dcBlockK  = one_pole(DC_BLOCK_HZ, rate);
    d->curveSeen = -1;
}

// a per-Drive value, between the measured settings
static double at_drive(const double v[DRIVE_POINTS], double drive) {
    double d = fmin(127.0, fmax(0.0, drive));
    int    i = 0;

    while ((i < (DRIVE_POINTS - 2)) && (d > kDriveAt[i + 1])) {
        i++;
    }
    return v[i] + (((d - kDriveAt[i]) / (kDriveAt[i + 1] - kDriveAt[i])) * (v[i + 1] - v[i]));
}

// notes §5: the instrument's quiet inputs come out quieter still, and still clipped, below a knee in the input's level
static double ceiling_db(tP2Drive * d, double hw) {
    double power   = hw * hw;

    d->inPower += ((power > d->inPower) ? d->inAttack : d->inRelease) * (power - d->inPower);
    double levelDb = (10.0 * log10(fmax(d->inPower, 1e-20))) - LEVEL_METER_OVER_RMS_DB;

    return d->trimDb - (d->slope * fmax(0.0, d->kneeDb - levelDb));
}

// notes §7: the curve's coefficients when the curve changes, its per-Drive values when either does
static void follow_shape(tP2Drive * d, const tCurveShape * c, int curve, double drive) {
    if (curve != d->curveSeen) {
        d->curveSeen = curve;
        d->couplingK = one_pole(c->couplingHz, d->rate);
        d->lowPassK  = one_pole(c->shelfHz, d->rate);
    } else if (drive == d->driveSeen) {
        return;
    }
    d->driveSeen = drive;
    d->gain      = pow(10.0, at_drive(c->gainDb, drive) / 20.0);
    d->bias      = at_drive(c->bias, drive);
    d->kneeDb    = at_drive(c->kneeDb, drive);
    d->slope     = at_drive(c->slope, drive);
    d->trimDb    = at_drive(c->trimDb, drive);
}

// notes §6: a knee from round (hardness 1) to square (large); tanh where no hardness was fitted
static double clip(double u, double ceiling, double hardness) {
    double x = u / ceiling;

    if (hardness <= 0.0) {
        return ceiling * tanh(x);
    }
    return ceiling * x / pow(1.0 + pow(fabs(x), hardness), 1.0 / hardness);
}

double p2_drive_process(tP2Drive * d, tP2DriveCurve curve, double drive, double x) {
    const tCurveShape * c;
    double              u, clipped, shaped, out;

    if (curve == eP2DriveNone) {
        return x;
    }

    if (curve == eP2DriveFx) {
        return 0.0;
    }
    c           = (curve == eP2DriveTube) ? &kTube : &kFuzz;
    follow_shape(d, c, (int)curve, drive);
    double              hw      = x * HARDWARE_UNITS_PER_ENGINE;
    double              ceiling = pow(10.0, ceiling_db(d, hw) / 20.0);
    double              pos = c->positive * ceiling, neg = c->negative * ceiling;

    u           = (hw * d->gain) - d->coupled + (d->bias * ceiling);
    clipped     = (u >= 0.0) ? clip(u, pos, c->hardPos) : clip(u, neg, c->hardNeg);
    d->coupled += d->couplingK * clipped;
    d->lowPass += d->lowPassK * (clipped - d->lowPass);
    shaped      = (c->shelfDirect * clipped) + ((1.0 - c->shelfDirect) * d->lowPass);
    // the clipper's bias leaves a DC offset; the output stage is AC coupled
    out         = shaped - d->dcBlock;
    d->dcBlock += d->dcBlockK * out;
    return out / HARDWARE_UNITS_PER_ENGINE;
}
