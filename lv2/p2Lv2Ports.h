/*
 * Pulse2Alike - the LV2 plug-in's ports: which parameters are ports, and what each port's number means.
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
// Notes: Docs/code-notes/p2Lv2.md - "// notes §k" refers there.

#ifndef __P2_LV2_PORTS_H__
#define __P2_LV2_PORTS_H__

#include <stdbool.h>
#include <stdint.h>

#include "p2Params.h"

// notes §1: THESE MAY NEVER CHANGE once a bundle is out - a pedalboard or preset finds the plug-in by its URI and
// each port by its symbol
#define P2_LV2_URI          "https://github.com/chrispurusha/Pulse2Alike#lv2"
#define P2_LV2_GROUP_URI    P2_LV2_URI "-group-"
#define P2_LV2_BRAND        "Purusha"          // mod:brand, at most 11 characters on a MOD device

// notes §1: what a MOD device compares to decide a bundle is newer - raise micro for every bundle that is, and never
// let either go down
#define P2_LV2_MINOR_VERSION    (1)
#define P2_LV2_MICRO_VERSION    (0)

// The ports before the controls
enum {
    kP2Lv2PortOutLeft = 0,
    kP2Lv2PortOutRight,
    kP2Lv2PortEvents,
    kP2Lv2FirstControl
};

#define P2_LV2_TOGGLE       (1u << 0)  // OFF/ON: lv2:toggled, a switch a footswitch can take
#define P2_LV2_ENUM         (1u << 1)  // the names as scale points: lv2:enumeration
#define P2_LV2_NOT_SHOWN    (1u << 2)  // saved with the sound, kept off the GUI (pprops:notOnGUI)
#define P2_LV2_ARTIFACTS    (1u << 3)  // changing it clicks (pprops:causesArtifacts)

// notes §2: one control port. Its value is offset + scale x the parameter's plain value (whole for a stepped one) -
// which is what the panel shows for it.
typedef struct {
    uint32_t     id;              // the parameter
    const char * symbol;          // lv2:symbol
    const char * shortName;       // lv2:shortName, at most 16 and unique: what a MOD device's display shows
    double       offset;
    double       scale;
    const char * unit;            // a units: term, or NULL
    uint32_t     flags;           // P2_LV2_*
    int32_t      defaultStep;     // notes §3: steps above the plain minimum; -1 takes the engine's default
    const char * comment;         // rdfs:comment, or NULL
} tP2Lv2Port;

extern const tP2Lv2Port gP2Lv2Ports[];
extern const uint32_t   gP2Lv2PortCount;

// The port of a parameter, NULL for one that is not a port (notes §4)
const tP2Lv2Port * p2_lv2_port_of(uint32_t id);

bool p2_lv2_stepped(const tP2Lv2Port * port);
double p2_lv2_normalized(const tP2Lv2Port * port, double value);    // a port's value to the engine's 0..1
double p2_lv2_value(const tP2Lv2Port * port, double normalized);    // and back
double p2_lv2_default(const tP2Lv2Port * port);                     // the port's default value

#endif // __P2_LV2_PORTS_H__
