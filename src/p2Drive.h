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

#ifndef __P2_DRIVE_H__
#define __P2_DRIVE_H__

typedef enum {
    eP2DriveNone = 0,
    eP2DriveTube,
    eP2DriveFuzz,
    eP2DriveFx        // notes §3: the return of an effects board that was never made
} tP2DriveCurve;

typedef struct {
    double rate;
    double coupled;   // the coupling capacitor ahead of the clipper
    double lowPass;   // the shelf after it
    double dcBlock;
    double dcIn;
    double inPower;   // notes §5: the input's level, for the quiet-input loss
    double inAttack, inRelease;

    // notes §7: what depends only on the rate, the curve and the Drive value, kept until one of them moves
    double dcBlockK;
    int    curveSeen;               // -1 before the first sample
    double couplingK, lowPassK;     // the curve's at this rate
    double driveSeen;
    double gain, bias, kneeDb, slope, trimDb;
} tP2Drive;

void p2_drive_init(tP2Drive * d, double rate);
// x in the engine's units before the VCA; drive 0..127 (modulation included)
double p2_drive_process(tP2Drive * d, tP2DriveCurve curve, double drive, double x);

#endif // __P2_DRIVE_H__
