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

#include <math.h>
#include <stddef.h>

#include "p2Lv2Ports.h"

#define SEMI       "units:semitone12TET"
#define BIPOLAR    (-64.0)   // the panel's bipolar dials: 0..127 shown as -64..+63

#define DIAL(id, symbol, shortName)            {id, symbol, shortName, 0.0, 1.0, NULL, 0, -1, NULL}
#define BIPOLAR_DIAL(id, symbol, shortName)    {id, symbol, shortName, BIPOLAR, 1.0, NULL, 0, -1, NULL}
#define SEMITONES(id, symbol, shortName)       {id, symbol, shortName, 0.0, 1.0, SEMI, 0, -1, NULL}
#define LIST(id, symbol, shortName)            {id, symbol, shortName, 0.0, 1.0, NULL, P2_LV2_ENUM, -1, NULL}
#define TOGGLE(id, symbol, shortName)          {id, symbol, shortName, 0.0, 1.0, NULL, P2_LV2_TOGGLE, -1, NULL}

// notes §4: in parameter id order, which only ever grows at the end; every parameter not here is not a port
const tP2Lv2Port gP2Lv2Ports[] = {
    {
        kP2ParamOsc1Shape, "osc1_shape", "Osc1 Shape", 0.0, 1.0, NULL, P2_LV2_ENUM, -1,
        "UNISON-M/P and UNIAPW-M/P are eight detuned oscillators; PARA-8 and PARA-4 are paraphonic voices sharing the "
        "filter and VCA."
    },
    DIAL(kP2ParamOsc1Width, "osc1_width", "Osc1 Width"),
    DIAL(kP2ParamOsc1Level, "osc1_level", "Osc1 Level"),
    {
        kP2ParamOsc2Shape, "osc2_shape", "Osc2 Shape", 0.0, 1.0, NULL, P2_LV2_ENUM, -1,
        "X3-PWM and X1-PWM are not modelled yet: they play APW and SAW."
    },
    DIAL(kP2ParamOsc2Width, "osc2_width", "Osc2 Width"),
    SEMITONES(kP2ParamOsc2Semitone, "osc2_semitone", "Osc2 Semitone"),
    DIAL(kP2ParamOsc2Level, "osc2_level", "Osc2 Level"),
    DIAL(kP2ParamVolume, "volume", "Volume"),
    DIAL(kP2ParamCutoff, "cutoff", "Cutoff"),
    DIAL(kP2ParamResonance, "resonance", "Resonance"),
    LIST(kP2ParamFilterType, "filter_type", "Filter Type"),
    SEMITONES(kP2ParamOsc1Semitone, "osc1_semitone", "Osc1 Semitone"),
    BIPOLAR_DIAL(kP2ParamOsc1Detune, "osc1_detune", "Osc1 Detune"),
    TOGGLE(kP2ParamOsc1Keytrack, "osc1_keytrack", "Osc1 Keytrack"),
    BIPOLAR_DIAL(kP2ParamOsc2Detune, "osc2_detune", "Osc2 Detune"),
    TOGGLE(kP2ParamOsc2Keytrack, "osc2_keytrack", "Osc2 Keytrack"),
    {
        kP2ParamOsc3Shape, "osc3_shape", "Osc3 Shape", 0.0, 1.0, NULL, P2_LV2_ENUM, -1,
        "EXT has no input in the plug-in, so it is silent."
    },
    LIST(kP2ParamOsc3Routing, "osc3_routing", "Osc3 Routing"),
    SEMITONES(kP2ParamOsc3Semitone, "osc3_semitone", "Osc3 Semitone"),
    BIPOLAR_DIAL(kP2ParamOsc3Detune, "osc3_detune", "Osc3 Detune"),
    TOGGLE(kP2ParamOsc3Sync, "osc3_sync", "Osc3 Sync"),
    DIAL(kP2ParamOsc3Level, "osc3_level", "Osc3 Level"),
    DIAL(kP2ParamNoiseLevel, "noise_level", "Noise Level"),
    DIAL(kP2ParamGlideRate, "glide_rate", "Glide Rate"),
    LIST(kP2ParamGlideMode, "glide_mode", "Glide Mode"),
    DIAL(kP2ParamUnisonDetune, "unison_detune", "Unison Detune"),
    BIPOLAR_DIAL(kP2ParamFilterEnvAmount, "filter_env_amount", "Filter Env Amt"),
    BIPOLAR_DIAL(kP2ParamFilterKeytrack, "filter_keytrack", "Filter Keytrack"),
    BIPOLAR_DIAL(kP2ParamFilterVelocity, "filter_velocity", "Filter Velocity"),
    DIAL(kP2ParamDrive, "drive", "Drive"),
    LIST(kP2ParamDriveCurve, "drive_curve", "Drive Curve"),
    {
        kP2ParamPan, "pan", "Pan", BIPOLAR, 1.0, NULL, 0, -1,
        "Kept with the sound; not modelled yet - the engine is mono."
    },
    BIPOLAR_DIAL(kP2ParamAmpVelocity, "amp_velocity", "Amp Velocity"),
    DIAL(kP2ParamEnvFAttack, "filter_env_attack", "Filt Env Attack"),
    DIAL(kP2ParamEnvFDecay, "filter_env_decay", "Filt Env Decay"),
    DIAL(kP2ParamEnvFSustain, "filter_env_sustain", "Filt Env Sustain"),
    DIAL(kP2ParamEnvFRelease, "filter_env_release", "Filt Env Release"),
    LIST(kP2ParamEnvFLoop, "filter_env_loop", "Filt Env Loop"),
    LIST(kP2ParamEnvFTrigger, "filter_env_trigger", "Filt Env Trigger"),
    DIAL(kP2ParamEnvAAttack, "amp_env_attack", "Amp Env Attack"),
    DIAL(kP2ParamEnvADecay, "amp_env_decay", "Amp Env Decay"),
    DIAL(kP2ParamEnvASustain, "amp_env_sustain", "Amp Env Sustain"),
    DIAL(kP2ParamEnvARelease, "amp_env_release", "Amp Env Release"),
    LIST(kP2ParamEnvALoop, "amp_env_loop", "Amp Env Loop"),
    LIST(kP2ParamEnvATrigger, "amp_env_trigger", "Amp Env Trigger"),
    DIAL(kP2ParamLfo1Speed, "lfo1_speed", "LFO1 Speed"),
    LIST(kP2ParamLfo1Shape, "lfo1_shape", "LFO1 Shape"),
    LIST(kP2ParamLfo1Phase, "lfo1_phase", "LFO1 Phase"),
    DIAL(kP2ParamLfo2Speed, "lfo2_speed", "LFO2 Speed"),
    DIAL(kP2ParamLfo2Delay, "lfo2_delay", "LFO2 Delay"),
    LIST(kP2ParamLfo2Phase, "lfo2_phase", "LFO2 Phase"),
    LIST(kP2ParamMod1Source, "mod1_source", "Mod1 Source"),
    BIPOLAR_DIAL(kP2ParamMod1Amount, "mod1_amount", "Mod1 Amount"),
    LIST(kP2ParamMod1Target, "mod1_target", "Mod1 Target"),
    LIST(kP2ParamMod2Source, "mod2_source", "Mod2 Source"),
    BIPOLAR_DIAL(kP2ParamMod2Amount, "mod2_amount", "Mod2 Amount"),
    LIST(kP2ParamMod2Target, "mod2_target", "Mod2 Target"),
    LIST(kP2ParamMod3Source, "mod3_source", "Mod3 Source"),
    BIPOLAR_DIAL(kP2ParamMod3Amount, "mod3_amount", "Mod3 Amount"),
    LIST(kP2ParamMod3Target, "mod3_target", "Mod3 Target"),
    LIST(kP2ParamMod4Source, "mod4_source", "Mod4 Source"),
    BIPOLAR_DIAL(kP2ParamMod4Amount, "mod4_amount", "Mod4 Amount"),
    LIST(kP2ParamMod4Target, "mod4_target", "Mod4 Target"),
    LIST(kP2ParamMod5Source, "mod5_source", "Mod5 Source"),
    BIPOLAR_DIAL(kP2ParamMod5Amount, "mod5_amount", "Mod5 Amount"),
    LIST(kP2ParamMod5Target, "mod5_target", "Mod5 Target"),
    LIST(kP2ParamMod6Source, "mod6_source", "Mod6 Source"),
    BIPOLAR_DIAL(kP2ParamMod6Amount, "mod6_amount", "Mod6 Amount"),
    LIST(kP2ParamMod6Target, "mod6_target", "Mod6 Target"),
    LIST(kP2ParamMod7Source, "mod7_source", "Mod7 Source"),
    BIPOLAR_DIAL(kP2ParamMod7Amount, "mod7_amount", "Mod7 Amount"),
    LIST(kP2ParamMod7Target, "mod7_target", "Mod7 Target"),
    LIST(kP2ParamMod8Source, "mod8_source", "Mod8 Source"),
    BIPOLAR_DIAL(kP2ParamMod8Amount, "mod8_amount", "Mod8 Amount"),
    LIST(kP2ParamMod8Target, "mod8_target", "Mod8 Target"),
    LIST(kP2ParamArpActive, "arp_active", "Arp Active"),
    {
        kP2ParamArpRange, "arp_range", "Arp Range", 1.0, 1.0, NULL, 0, -1,
        "Octaves."
    },
    // notes §2: the panel shows 46 + 2 per step, in BPM
    {
        kP2ParamArpTempo, "arp_tempo", "Arp Tempo", 46.0, 2.0, "units:bpm", 0, -1,
        "The arpeggiator's own clock, when the host's transport is not rolling."
    },
    LIST(kP2ParamArpClock, "arp_clock", "Arp Clock"),
    {
        kP2ParamArpPattern, "arp_pattern", "Arp Pattern", 0.0, 1.0, NULL, 0, -1,
        "Kept with the sound; what it selects on the instrument is not measured yet."
    },
    LIST(kP2ParamArpMode, "arp_mode", "Arp Mode"),
    LIST(kP2ParamArpStep, "arp_step_duration", "Arp Step Length"),
    {kP2ParamArpSwing,  "arp_swing",  "Arp Swing",   -14.0, 1.0, "units:pc",                0, -1, NULL},
    {kP2ParamArpDelay,  "arp_delay",  "Arp Delay",   -64.0, 1.0, "units:pc",                0, -1, NULL},
    {
        kP2ParamArpLength, "arp_length", "Arp Length", 1.0, 1.0, NULL, 0, -1,
        "Steps in the pattern."
    },
    SEMITONES(kP2ParamBendUp, "bend_up", "Bend Up"),
    SEMITONES(kP2ParamBendDown, "bend_down", "Bend Down"),
    DIAL(kP2ParamOsc1Fade, "osc1_fade", "Osc1 Env A Fade"),
    DIAL(kP2ParamOsc2Fade, "osc2_fade", "Osc2 Env A Fade"),
    // notes §3: 2x here, not the engine's 4x
    {
        kP2ParamOversample, "oversampling", "Oversampling", 0.0, 1.0, NULL, P2_LV2_ENUM | P2_LV2_ARTIFACTS, 1,
        "The engine's internal rate. 2x by default here, for a MOD device's CPU; the desktop plug-ins default to 4x, "
        "which aliases less at high cutoffs and drive. Changing it clicks once."
    },
    {
        kP2ParamAnomalies, "anomalies", "Anomalies", 0.0, 1.0, NULL, P2_LV2_TOGGLE, -1,
        "On reproduces this Pulse 2's measured departures from the textbook circuit - the 1 ms modulation staircase, "
        "FX's silence, the arpeggiator's quirks; off is the idealised instrument, and costs more CPU."
    },
    DIAL(kP2ParamMaster, "master", "Master Volume"),
    {
        kP2ParamArpStep1, "arp_step1", "Arp Step 1", 0.0, 1.0, NULL, P2_LV2_NOT_SHOWN, -1,
        "One arpeggiator step as the instrument stores it: the event in bits 0-3, the time in bits 4-5, glide in bit 6."
    },
    {kP2ParamArpStep2,  "arp_step2",  "Arp Step 2",    0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep3,  "arp_step3",  "Arp Step 3",    0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep4,  "arp_step4",  "Arp Step 4",    0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep5,  "arp_step5",  "Arp Step 5",    0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep6,  "arp_step6",  "Arp Step 6",    0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep7,  "arp_step7",  "Arp Step 7",    0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep8,  "arp_step8",  "Arp Step 8",    0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep9,  "arp_step9",  "Arp Step 9",    0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep10, "arp_step10", "Arp Step 10",   0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep11, "arp_step11", "Arp Step 11",   0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep12, "arp_step12", "Arp Step 12",   0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep13, "arp_step13", "Arp Step 13",   0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep14, "arp_step14", "Arp Step 14",   0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep15, "arp_step15", "Arp Step 15",   0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    {kP2ParamArpStep16, "arp_step16", "Arp Step 16",   0.0, 1.0, NULL,       P2_LV2_NOT_SHOWN, -1, NULL},
    LIST(kP2ParamArpAccent, "arp_accent", "Arp Accent"),
    {
        kP2ParamHiss, "hiss", "Hiss", 0.0, 1.0, NULL, P2_LV2_TOGGLE, -1,
        "The instrument's own hiss, before and after the filter."
    },
};

const uint32_t   gP2Lv2PortCount = sizeof(gP2Lv2Ports) / sizeof(gP2Lv2Ports[0]);

const tP2Lv2Port * p2_lv2_port_of(uint32_t id) {
    for (uint32_t i = 0; i < gP2Lv2PortCount; i++) {
        if (gP2Lv2Ports[i].id == id) {
            return &gP2Lv2Ports[i];
        }
    }

    return NULL;
}

bool p2_lv2_stepped(const tP2Lv2Port * port) {
    return gP2Params[port->id].stepCount > 0;
}

// notes §2: whatever a host sends - past the ends, between steps - lands on a value the engine can hold
double p2_lv2_normalized(const tP2Lv2Port * port, double value) {
    const tSynthLibParam * p     = &gP2Params[port->id];
    double                 plain = (value - port->offset) / port->scale;

    if (!isfinite(plain)) {
        plain = p->plainMin;
    }

    if (p2_lv2_stepped(port)) {
        plain = round(plain);
    }
    plain = fmin(p->plainMax, fmax(p->plainMin, plain));
    return (plain - p->plainMin) / (p->plainMax - p->plainMin);
}

double p2_lv2_value(const tP2Lv2Port * port, double normalized) {
    const tSynthLibParam * p     = &gP2Params[port->id];
    double                 plain = p->plainMin + (normalized * (p->plainMax - p->plainMin));

    return port->offset + (port->scale * (p2_lv2_stepped(port) ? round(plain) : plain));
}

double p2_lv2_default(const tP2Lv2Port * port) {
    const tSynthLibParam * p = &gP2Params[port->id];

    return (port->defaultStep >= 0) ? (port->offset + (port->scale * (p->plainMin + port->defaultStep)))
                                    : p2_lv2_value(port, p->defaultNormalized);
}

// notes §8: the instrument's CCs, as midi.guide's table for the Pulse 2 has them (Docs/design.md §4 has the same).
// Those down to Osc 2 Fade are the ones the desktop plug-in drives the instrument with (p2Plugin.c, kHwCc); the LFOs,
// bend range and mod slots are the manual's, not yet measured against the instrument. CC 7 is the sound's Volume, as
// on the instrument (p2Plugin notes §28), and CC 10, the table's PAN, its Pan the same way. The mod wheel (CC 1) is
// not here: it is a parameter that is not a port, which take_midi() gives the engine as it is.
const tP2Lv2Cc gP2Lv2Ccs[] = {
    {kP2ParamOsc1Shape,       34}, {kP2ParamOsc1Width,    35}, {kP2ParamOsc1Semitone,     70}, {kP2ParamOsc1Detune,      71},
    {kP2ParamOsc1Keytrack,    73}, {kP2ParamOsc1Level,    45}, {kP2ParamOsc2Shape,        38}, {kP2ParamOsc2Width,       39},
    {kP2ParamOsc2Semitone,    36}, {kP2ParamOsc2Detune,   37}, {kP2ParamOsc2Keytrack,     40}, {kP2ParamOsc2Level,       46},
    {kP2ParamOsc3Shape,       44}, {kP2ParamOsc3Routing,  72}, {kP2ParamOsc3Semitone,     42}, {kP2ParamOsc3Detune,      43},
    {kP2ParamOsc3Sync,        41}, {kP2ParamOsc3Level,    47}, {kP2ParamNoiseLevel,       48}, {kP2ParamCutoff,          50},
    {kP2ParamResonance,       56}, {kP2ParamFilterType,   49}, {kP2ParamFilterEnvAmount,  52}, {kP2ParamFilterKeytrack,  51},
    {kP2ParamFilterVelocity,  53}, {kP2ParamDrive,        76}, {kP2ParamDriveCurve,       77}, {kP2ParamPan,             59},
    {kP2ParamVolume,          57}, {kP2ParamAmpVelocity,  58}, {kP2ParamEnvFAttack,       14}, {kP2ParamEnvFDecay,       15},
    {kP2ParamEnvFSustain,     16}, {kP2ParamEnvFRelease,  17}, {kP2ParamEnvFLoop,         74}, {kP2ParamEnvFTrigger,     29},
    {kP2ParamEnvAAttack,      18}, {kP2ParamEnvADecay,    19}, {kP2ParamEnvASustain,      20}, {kP2ParamEnvARelease,     21},
    {kP2ParamEnvALoop,        75}, {kP2ParamEnvATrigger,  31}, {kP2ParamGlideRate,         5}, {kP2ParamGlideMode,       62},
    {kP2ParamArpActive,       79}, {kP2ParamArpRange,     80}, {kP2ParamArpTempo,         81}, {kP2ParamArpClock,        82},
    {kP2ParamArpMode,         83}, {kP2ParamArpStep,      84}, {kP2ParamArpSwing,         85}, {kP2ParamArpDelay,        86},
    {kP2ParamArpLength,       87}, {kP2ParamArpAccent,    88}, {kP2ParamUnisonDetune,    102}, {kP2ParamOsc1Fade,       103},
    {kP2ParamOsc2Fade,       104},
    {kP2ParamLfo1Speed,       24}, {kP2ParamLfo1Shape,    25}, {kP2ParamLfo2Speed,        26}, {kP2ParamLfo2Delay,       27},
    {kP2ParamBendUp,         105}, {kP2ParamBendDown,    106},
    {kP2ParamMod1Source,     108}, {kP2ParamMod1Amount,  109}, {kP2ParamMod1Target,      110},
    {kP2ParamMod2Source,     111}, {kP2ParamMod2Amount,  112}, {kP2ParamMod2Target,      113},
    {kP2ParamMod3Source,     114}, {kP2ParamMod3Amount,  115}, {kP2ParamMod3Target,      116},
    {kP2ParamMod4Source,     117}, {kP2ParamMod4Amount,  118}, {kP2ParamMod4Target,      119},
    {kP2ParamMod5Source,      90}, {kP2ParamMod5Amount,   91}, {kP2ParamMod5Target,       92},
    {kP2ParamMod6Source,      93}, {kP2ParamMod6Amount,   94}, {kP2ParamMod6Target,       95},
    {kP2ParamMod7Source,      96}, {kP2ParamMod7Amount,   97}, {kP2ParamMod7Target,       98},
    {kP2ParamMod8Source,      99}, {kP2ParamMod8Amount,  100}, {kP2ParamMod8Target,      101},
    {kP2ParamVolume,           7}, {kP2ParamPan,          10},
};

const uint32_t gP2Lv2CcCount = sizeof(gP2Lv2Ccs) / sizeof(gP2Lv2Ccs[0]);

// notes §8: a sound byte on the port, read as p2_patch_from_syx() reads a dump's - from the parameter's lowest byte,
// one byte a step, clamped to its range as the instrument clamps a CC
double p2_lv2_byte_value(const tP2Lv2Port * port, int byte) {
    const tSynthLibParam * p    = &gP2Params[port->id];
    double                 span = p->plainMax - p->plainMin;
    int32_t                low  = 0;
    double                 v;

    for (int b = 0; b < P2_SOUND_BYTES; b++) {
        if (gP2SoundBytes[b].paramId == port->id) {
            low = gP2SoundBytes[b].rawLow;    // the last, as p2_patch_from_syx() takes it
        }
    }

    v = (span > 0.0) ? ((double)(byte - low) / span) : 0.0;
    return p2_lv2_value(port, fmin(1.0, fmax(0.0, v)));
}
