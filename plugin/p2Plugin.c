/*
 * Pulse2Alike - a reproduction inspired by the Waldorf Pulse 2, as a plug-in.
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
// Notes: Docs/code-notes/p2Plugin.md - "// notes §k" refers there.

#include <ctype.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "synthlibPlugin.h"
#include "synthlibLog.h"
#include "p2Dco.h"
#include "p2Drive.h"
#include "p2Arp.h"
#include "p2Filter.h"
#include "p2Env.h"
#include "p2Lfo.h"
#include "p2Draw.h"
#include "p2Params.h"
#include "p2Patch.h"
#include "p2Hardware.h"
#ifndef P2_NO_EDITOR
#include "synthlibPanelView.h"
#include "prefs.h"
#include "p2Prefs.h"
#endif

// touch /tmp/pulse2alike-log, read /tmp/pulse2alike.log - see SynthLib's plugin/synthlibLog.h.
const char           gSynthLibLogName[] = "pulse2alike";

#ifndef P2A_VERSION_STRING
#define P2A_VERSION_STRING    "0.1.0"
#endif

#ifndef P2A_AU_VERSION
#define P2A_AU_VERSION    (0x00000100)
#endif

// THESE MAY NEVER CHANGE: a saved project finds the plug-in by them.
static const uint8_t gProcessorUid[16]  = {0x3B, 0x8E, 0x51, 0xC7, 0x0A, 0x64, 0x4D, 0x2F, 0x9E, 0x13, 0x7C, 0x48, 0xD5, 0x26, 0xA1, 0x90};
static const uint8_t gControllerUid[16] = {0xC4, 0x71, 0x0E, 0x9B, 0x52, 0xF3, 0x48, 0x86, 0xB0, 0x2D, 0x6A, 0x1F, 0x83, 0xE7, 0x5C, 0x39};

#define FOUR_CC(a, b, c, d)    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))
#define P2A_AU_TYPE                FOUR_CC('a', 'u', 'm', 'u')
#define P2A_AU_SUBTYPE             FOUR_CC('P', '2', 'a', 'l')
#define P2A_AU_MANUFACTURER        FOUR_CC('C', 'P', 'u', 'r')
#define P2A_AU_BUNDLE_ID           "com.chrispurusha.pulse2alike.au"

#define DCO_COUNT                  (3)
#define LEVEL_SILENT_BELOW         (11.41)  // notes §9
#define OSC3_LEVEL_FULL            (1.19)   // notes §36: +1.5 dB at level 127
#define OSC3_LEVEL_EXPONENT        (0.709)
#define LEVEL_EXPONENT             (0.979)
#define DETUNE_PITCH_PER_STEP      (2)       // 1/256 semitone units
#define NOTE_STACK_SIZE            (16)
#define OVERSAMPLE_DEFAULT         (4)       // notes §33: the Oversampling setting picks 1, 2 or 4
#define DECIMATOR_TAPS             (64)
#define CONTROL_GLIDE_SECONDS      (0.003)   // notes §7
#define CONTROL_TICK_SECONDS       (0.001)   // notes §10
#define CONTROL_SMOOTH_SECONDS     (0.0003)
#define KEYTRACK_REFERENCE_NOTE    (64.0)    // measured: keytrack changes nothing at note 64
#define ENV_CUTOFF_PER_UNIT        (2.0)     // cutoff steps per filter-env amount step from 64, measured
#define OSC_INTO_FILTER            (0.3)     // notes §4
// notes §25: osc 3 as a control - AC coupled, then into the cutoff, osc 2's level or the drive
#define OSC3_TO_CUTOFF_STEPS       (125.0)   // per unit of osc 3 (+-1) at full level, fitted to P002
#define OSC3_TO_DRIVE              (30.0)    // drive units per unit at full level, measured
#define OSC3_FB_GAIN               (5.7)     // the drive's output back into the mixer, at full level: measured with a saw
#define FEEDTHROUGH_COUPLING_HZ    (3)       // notes §43
#define OSC_EVEN_COUPLING_HZ       (20.0)    // notes §44: the asymmetry's DC goes no further
#define OSC_EVEN                   (0.06)    // notes §44: each oscillator level stage's asymmetry
#define HW_UNITS                   (0.888)   // the engine's pre-VCA level in the instrument's output units at Volume 40
#define VCA_FEEDTHROUGH            (0.0)     // notes §43: the VCA's control leaking into its output
#define VCA_COUPLING_HZ            (2)
#define LEVEL_FEEDTHROUGH          (0.0)     // notes §43: osc level modulation leaking into the mix as DC
#define ROUTED_MIX_GAIN            (1.8)     // notes §37: osc 3 routed away from the mix
#define MIX_SLEW_PER_SECOND        (15000.0) // notes §40: the filter input's slew, fitted to saws
#define MIX_COUPLING_HZ            (5.0)     // notes §37: the mix reaches the filter AC-coupled
#define OSC3_TO_LEVEL2             (1.0)     // per unit of osc 3 at full level - notes §25
#define LEVEL2_MEASURED            (1)       // notes §52: 1 the measured law; 0 the linear one it replaced
#define LEVEL2_RISE                (46.4)    // notes §52: per unit of osc 3, rising, at full depth
#define LEVEL2_FALL                (86.2)    //   and falling
#define LEVEL2_ZERO                (-4.2)    //   depth = ((level - LEVEL2_ZERO) / (127 - LEVEL2_ZERO))^LEVEL2_POWER
#define LEVEL2_POWER               (3.42)
#define LEVEL2_COUPLING_S          (0.0080)  //   the path's AC coupling, a time constant
#define LEVEL2_LAG_S               (0.0003)  //   and its low-pass
#define LEVEL2_CEILING             (3.0)     //   osc 2's level can rise no higher
#define OSC3_COUPLING_HZ           (14.0)
#define OUTPUT_GAIN                (3.926)   // notes §19: the instrument's own level
#define FAULT_LEVEL                (16.0)    // notes §47: 24 dB over full scale, far past any sound

// notes §13: the mod matrix's targets, in the instrument's list order
enum {
    eModPitch = 0,
    eModOsc1Pitch,
    eModOsc2Pitch,
    eModOsc3Pitch,
    eModPulseWidth1,
    eModPulseWidth2,
    eModOsc1Level,
    eModOsc2Level,
    eModOsc3Level,
    eModNoiseLevel,
    eModCutoff,
    eModResonance,
    eModVolume,
    eModPanning,
    eModLfo1Speed,
    eModMod1Amount,
    eModDrive,
    eModGlideRate,
    eModEnvFRates,
    eModEnvARates,
    MOD_TARGETS      = 32,
    eModVcfEnvAmount = 31
};
#define MOD_SLOTS               (8)
#define MOD_DEPTH_PER_SQUARE    (1.0 / 32.0)    // ±(amount − 64)² / 32 per unit of source, in the target's units: measured

// notes §51: what an amount weighs, in 1/256 of full scale, at |amount − 64| = 0..64 - measured on the instrument.
// Near (d/4)² but not equal: 1/8 doubling to 1 at d 4, straight runs between, full scale at 127
static const double kModWeight[65] = {
    0.0,   0.125,  0.25,   0.5,   1.0,   1.5,   2.0,   3.0,   4.0,   5.0,   6.0,   7.0,   8.0,  10.0,  12.0,  14.0, 16.0,
    18.0,   20.0,  22.0,  24.0,  27.5, 30.25,  33.0,  36.0,  39.0,  42.0,  45.0,  49.0,  52.0,  56.0,  60.0,  64.0,
    68.0,   72.0,  76.0,  81.0,  85.0,  90.0,  95.0, 100.0, 105.0, 110.0, 115.0, 121.0, 128.0, 132.0, 138.0, 144.0,
    150.0, 156.0, 162.0, 169.0, 175.0, 182.0, 189.0, 196.0, 203.0, 210.0, 217.0, 225.0, 232.0, 240.0, 256.0, 256.0
};

// notes §51: an amount's weight as the d² it stands in for (16 x the table), between entries by interpolation
static double mod_square(double d) {
    double a = fmin(64.0, fabs(d));
    int    i = (int)a;

    return 16.0 * ((i >= 64) ? kModWeight[64] : (kModWeight[i] + ((a - i) * (kModWeight[i + 1] - kModWeight[i]))));
}

// notes §18: the math destinations, in the mod target list's order from M-ACCU, and as sources from 17
enum {
    eMathAccu = 0,
    eMathProduct,
    eMathDelay,
    eMathSmooth,
    eMathMin,
    eMathMax,
    MATH_COUNT
};
#define eModMathFirst            (25)
#define DELAY_LINE_MS            (1024)

#define PARA8_VOICE_LEVEL        (0.263)    // notes §12: one voice against the same pulse played mono, measured
#define PARA4_VOICE_LEVEL        (0.363)
#define NOISE_FLOOR              (1e-5)     // about -100 dB: the analogue floor
#define NOISE_LEVEL              (1.4)      // notes §34: measured against the saw and in P006
#define NOISE_REFERENCE_RATE     (768000.0) // notes §34: the internal rate NOISE_LEVEL was measured at
#define NOISE_BANDWIDTH_HZ       (22000.0)
#define NOISE_COLOUR_HZ          (8500.0)   // notes §34: the noise is darker than white, measured on the noise itself
#define HISS_INTO_FILTER         (1.19e-3)  // notes §38: the instrument's hiss, measured against a saw
#define HISS_AFTER_FILTER        (2.03e-4)
#define FILTER_ENV_TIME_SCALE    (1.08)     // p2Env notes §6
#define PULSE_LEVEL              (0.525)    // notes §30: a pulse against the saw, measured
#define LFO_SLOW_SHAPES_FROM     (5)        // sSIN on: free-running, never restarted by a note

// notes §21: unison - 8 oscillators, detune × 1.25 / 256 semitone apart, symmetric about the note
#define UNISON_OSCS              (8)
#define UNISON_SPACING           (1.25) // 1/256-semitone units per step of Unison Detune
#define UNISON_VOICE_GAIN        (0.19) // each oscillator, so the eight sum near the measured level

// notes §12: a paraphonic voice - one pulse (PARA-8) or two (PARA-4), fading on its own into the shared filter
#define PARA_VOICES_MAX          (8)
#define PARA_FADE_SECONDS        (0.02)   // the fade in, which Fade does not change
// notes §26: a released voice decays as a release stage of R - 5.5 - 0.61 (127 - Fade)
#define FADE_RELEASE_OFFSET      (5.5)
#define FADE_RELEASE_SLOPE       (0.61)
#define FADE_SILENT              (1e-4)
#define SHAPE_PARA8              (8)
#define KEYTRACK_OFF_NOTE        (60.0)      // notes §31
#define SHAPE_PARA4              (9)

typedef struct {
    int      note;          // -1 when free
    bool     held;
    double   fade;          // 0..1, osc 1's part
    double   fade2;         // osc 2's part, in PARA-4
    uint32_t order;         // when it was allocated
    tP2Dco   a, b;
} tP2Voice;

typedef struct {
    double       sampleRate;
    double       param[kP2ParamCount]; // normalized
    tP2Dco       dco[DCO_COUNT];
    tP2Voice     voice[PARA_VOICES_MAX];
    tP2Dco       unison[UNISON_OSCS]; // notes §21
    tP2Drive     drive;
    tP2Arp       arp;                 // notes §32
    double       arpTicks;            // its own clock, when no host is playing
    int          arpActiveSeen;       // notes §45: Arp Active as the engine last followed it
    bool         arpGlide;            // the arp step now starting glides
    double       source[25];          // the mod sources at the last control tick (the arp\'s accent)
    double       driveOut;            // notes §25: the last drive output, which osc 3\'s FB shape feeds back
    double       releaseVelocity;     // notes §28: the last note-off\'s
    bool         sustained[128];      // notes §28: released while the pedal was down
    double       mixCoupled;          // notes §37
    double       filterCoupled;
    double       osc3Coupled;         // notes §25: the coupling capacitor on osc 3\'s control path
    double       level2Coupled;       // notes §52: osc 3 on LEVEL2, its own coupling
    double       level2Lag;
    uint32_t     voiceClock;          // allocation order, for stealing the most recent

    uint8_t      notes[NOTE_STACK_SIZE];
    int          noteCount;
    int64_t      glidePitch;       // notes §17: the glided note, 1/256 semitone units
    bool         glideKnown;       // a note has sounded, so there is somewhere to glide from
    double       glideClock;       // time not yet spent in 1 ms glide steps
    tP2Lfo       lfo1, lfo2;
    double       math[MATH_COUNT]; // notes §18
    double       delayLine[DELAY_LINE_MS];
    int          delayAt;
    double       smooth;
    _Atomic int  programShown;                        // notes §20: the last program change's sound number, for the panel
    int          preset;                              // notes §48: main thread; the preset the sound came from, 0 none
    _Atomic bool startPreset;                         // notes §48: a new instance loads P001 - first block or first frame
    double       presetsChecked;                      // notes §48: when the presets file was last looked at
    bool         haveBase;
    uint8_t      baseDump[P2_DUMP_LENGTH];            // notes §23: main thread; the bytes a save keeps
    char         soundName[P2_SOUND_NAME_LENGTH + 1]; // notes §24: main thread, as the panel shows and a save writes it
    char         patchPath[1024];                     // the file it came from or went to; "" for none
    double       lfo2Fade;                            // LFO 2's delay, 0..1 since the note
    double       velocity;                            // the last note's, 0..1
    double       mod[MOD_TARGETS];                    // what the mod matrix adds to each target, at the last tick
    tP2Env       ampEnv;
    tP2Env       filterEnv;
    int          tickCountdown;   // host samples to the next control tick
    double       vcaTarget;       // what the last tick asked for
    double       vca;             // smoothed, applied to the audio
    double       cutoffTarget;
    double       cutoffNow;       // the cutoff the filter is set to, smoothed

    tP2Filter    filter;
    double       cutoff;     // gliding towards the parameter, in steps
    double       resonance;
    double       lastNote;
    int          oversample; // notes §33: 1, 2 or 4
    double       decimator[DECIMATOR_TAPS];
    double       history[DECIMATOR_TAPS];
    uint32_t     historyPos;
    uint32_t     noise;                // xorshift state
    double       noiseLp[4];           // notes §34: the band limit, two biquad states
    double       noiseColour;
    double       levelTarget[5];       // notes §43: osc 1-3, noise, the feedthrough - set per control tick
    double       levelNow[5];          //   and glided per sample
    bool         levelsSet;
    double       feedCoupled;
    double       oscEvenDc[3];
    double       vcaCoupled;
    double       hwSent[kP2ParamCount]; // notes §41: what the instrument's edit buffer was last given
    bool         hwValid;
    bool         hwDumpDue;
    double       hwLastDump;
    double       hwDumpNotBefore;
    int          hwProgram;      // notes §41: the library number the instrument was last sent, 0 none
    double       hwChannelKnown; // notes §46: the channel as last read or saved, negative before the first
    int          hwLock;         // notes §49: this instance's hold on the instrument, -1 none
    bool         hwBusy;         // notes §49: Link on, but another instance owns the instrument
    bool         backingUp;      // notes §50: this instance is reading the instrument's sounds
    bool         backupLock;     //   and took the lock for it (Link did not already hold it)
    double       slewed;         // notes §40
    double       hissLp[8];      // notes §38: the same, for the two hiss sources
    uint32_t     faults;         // notes §47
} tP2Plugin;

static double plain(const tP2Plugin * p, uint32_t id) {
    return gP2Params[id].plainMin + (p->param[id] * (gP2Params[id].plainMax - gP2Params[id].plainMin));
}

static int32_t plain_int(const tP2Plugin * p, uint32_t id) {
    return (int32_t)lround(plain(p, id));
}

// notes §2
static int32_t width_from_dial(int32_t dial) {
    return dial << 8;
}

// notes §8: the shapes the DCO plays so far; every other entry of the list plays a saw until it is built
static tP2Shape dco_shape(int32_t listIndex) {
    return ((listIndex >= 0) && (listIndex < eP2ShapeCount)) ? (tP2Shape)listIndex : eP2ShapeSaw;
}

// notes §28: the wheel, -1..+1, scaled by Bend Up or Bend Down
static double bend_semitones(const tP2Plugin * p) {
    double b = plain(p, kP2ParamPitchBend);

    return b * ((b >= 0.0) ? plain(p, kP2ParamBendUp) : plain(p, kP2ParamBendDown));
}

// notes §9
static int32_t osc_pitch(const tP2Plugin * p, double note, uint32_t semiId, uint32_t detuneId) {
    int      which = (semiId == kP2ParamOsc1Semitone) ? eModOsc1Pitch : ((semiId == kP2ParamOsc2Semitone) ? eModOsc2Pitch : eModOsc3Pitch);
    double   mod   = p->mod[eModPitch] + p->mod[which] + bend_semitones(p);
    uint32_t keyId = (semiId == kP2ParamOsc1Semitone) ? kP2ParamOsc1Keytrack : ((semiId == kP2ParamOsc2Semitone) ? kP2ParamOsc2Keytrack : UINT32_MAX);

    // notes §31: Keytrack OFF holds the oscillator at note 60, its offsets and modulation still applied
    if ((keyId != UINT32_MAX) && (plain_int(p, keyId) == 0) && (plain_int(p, kP2ParamOsc1Shape) < SHAPE_PARA8)) {
        note = KEYTRACK_OFF_NOTE;
    }
    return p2_pitch_from_midi_note(note + plain(p, semiId) + mod) + ((plain_int(p, detuneId) - 64) * DETUNE_PITCH_PER_STEP);
}

static void set_unison(tP2Plugin * p);

static void update_dcos(tP2Plugin * p) {
    // notes §42: pitch and its modulation go on through the release, and the oscillators run from the start
    double  note   = (double)p->glidePitch / 256.0;
    double  rate   = p->sampleRate * p->oversample;
    int32_t shape3 = plain_int(p, kP2ParamOsc3Shape);

    p2_dco_set(&p->dco[0], osc_pitch(p, note, kP2ParamOsc1Semitone, kP2ParamOsc1Detune), width_from_dial((int32_t)lround(fmin(127.0, fmax(0.0, plain(p, kP2ParamOsc1Width) + p->mod[eModPulseWidth1])))),
               dco_shape(plain_int(p, kP2ParamOsc1Shape)), rate);
    p2_dco_set(&p->dco[1], osc_pitch(p, note, kP2ParamOsc2Semitone, kP2ParamOsc2Detune), width_from_dial((int32_t)lround(fmin(127.0, fmax(0.0, plain(p, kP2ParamOsc2Width) + p->mod[eModPulseWidth2])))),
               dco_shape(plain_int(p, kP2ParamOsc2Shape)), rate);
    // osc 3: SQR is an exact square (p2Dco notes §4); SAW and TRI as the others
    p2_dco_set(&p->dco[2], osc_pitch(p, note, kP2ParamOsc3Semitone, kP2ParamOsc3Detune), P2_WIDTH_SQUARE,
               (shape3 == 0) ? eP2ShapePulse : dco_shape(shape3), rate);
    set_unison(p);
}

// 0 off, 1 UNISON-M, 2 UNISON-P, 3 UNIAPW-M, 4 UNIAPW-P
static int unison_mode(const tP2Plugin * p) {
    int shape = plain_int(p, kP2ParamOsc1Shape);

    return ((shape >= 4) && (shape <= 7)) ? (shape - 3) : 0;
}

// notes §21: the eight oscillators' pitches - all on the newest note (M), or shared between the held notes (P)
static void set_unison(tP2Plugin * p) {
    int    mode  = unison_mode(p);
    int    notes = ((mode == 2) || (mode == 4)) ? ((p->noteCount < UNISON_OSCS) ? p->noteCount : UNISON_OSCS) : 1;
    double rate  = p->sampleRate * p->oversample;
    double step  = plain(p, kP2ParamUnisonDetune) * UNISON_SPACING;

    if ((mode == 0) || (p->noteCount == 0)) {
        return;
    }

    for (int i = 0; i < UNISON_OSCS; i++) {
        int    group = i % notes;
        int    size  = (UNISON_OSCS / notes) + ((group < (UNISON_OSCS % notes)) ? 1 : 0);
        int    k     = i / notes;                       // its place within the group
        double note  = (notes == 1) ? ((double)p->glidePitch / 256.0) : (double)p->notes[p->noteCount - 1 - group];
        double off   = (k - ((size - 1) / 2.0)) * step;

        p2_dco_set(&p->unison[i], osc_pitch(p, note, kP2ParamOsc1Semitone, kP2ParamOsc1Detune) + (int32_t)lround(off),
                   width_from_dial((int32_t)lround(fmin(127.0, fmax(0.0, plain(p, kP2ParamOsc1Width) + p->mod[eModPulseWidth1])))),
                   (mode >= 3) ? eP2ShapeApw : eP2ShapePulse, rate);
    }
}

// notes §26: the per-sample multiplier of a released voice - a release stage, shortened by Fade
static double voice_fade_out(const tP2Plugin * p, uint32_t fadeParam) {
    double value   = plain(p, kP2ParamEnvARelease) - FADE_RELEASE_OFFSET - (FADE_RELEASE_SLOPE * (127.0 - plain(p, fadeParam)));
    double seconds = (value <= 0.0) ? 0.0 : p2_env_decay_seconds(value);   // to -40 dB

    return (seconds <= 0.0) ? 0.0 : exp(-log(100.0) / (seconds * p->sampleRate * p->oversample));
}

static int para_voices(const tP2Plugin * p) {
    int shape = plain_int(p, kP2ParamOsc1Shape);

    return (shape == SHAPE_PARA8) ? 8 : ((shape == SHAPE_PARA4) ? 4 : 0);
}

static void set_voice_pitch(tP2Plugin * p, tP2Voice * v) {
    double rate = p->sampleRate * p->oversample;

    p2_dco_set(&v->a, osc_pitch(p, v->note, kP2ParamOsc1Semitone, kP2ParamOsc1Detune), width_from_dial((int32_t)lround(fmin(127.0, fmax(0.0, plain(p, kP2ParamOsc1Width) + p->mod[eModPulseWidth1])))),
               eP2ShapePulse, rate);
    p2_dco_set(&v->b, osc_pitch(p, v->note, kP2ParamOsc2Semitone, kP2ParamOsc2Detune), width_from_dial((int32_t)lround(fmin(127.0, fmax(0.0, plain(p, kP2ParamOsc2Width) + p->mod[eModPulseWidth2])))),
               eP2ShapePulse, rate);
}

// notes §12: the same note again reuses its voice; otherwise a free one, otherwise the most recent is stolen
static void para_note_on(tP2Plugin * p, int note, int count) {
    tP2Voice * pick = NULL;

    for (int i = 0; (i < count) && (pick == NULL); i++) {
        if (p->voice[i].note == note) {
            pick = &p->voice[i];
        }
    }

    for (int i = 0; (i < count) && (pick == NULL); i++) {
        if (!p->voice[i].held && (p->voice[i].fade <= 0.0) && (p->voice[i].fade2 <= 0.0)) {
            pick = &p->voice[i];
        }
    }

    if (pick == NULL) {
        pick = &p->voice[0];

        for (int i = 1; i < count; i++) {
            if (p->voice[i].order > pick->order) {
                pick = &p->voice[i];
            }
        }
    }
    pick->note  = note;
    pick->held  = true;
    pick->order = ++p->voiceClock;
    set_voice_pitch(p, pick);
}

static void para_note_off(tP2Plugin * p, int note) {
    for (int i = 0; i < PARA_VOICES_MAX; i++) {
        if (p->voice[i].note == note) {
            p->voice[i].held = false;
        }
    }
}

static double level_gain(const tP2Plugin * p, uint32_t id) {
    int    target = (id == kP2ParamOsc1Level) ? eModOsc1Level : ((id == kP2ParamOsc2Level) ? eModOsc2Level :
                                                                 ((id == kP2ParamOsc3Level) ? eModOsc3Level : eModNoiseLevel));
    double cc     = fmin(127.0, plain(p, id) + p->mod[target]);

    if (cc <= LEVEL_SILENT_BELOW) {
        return 0.0;
    }
    double x      = (cc - LEVEL_SILENT_BELOW) / (127.0 - LEVEL_SILENT_BELOW);

    // notes §36: osc 3 is louder than osc 1 and 2, and less tapered
    return pow(x, LEVEL_EXPONENT);
}

// notes §36: osc 3 IN THE MIX is louder and less tapered than osc 1 and 2; its control routings keep the shared law,
// which their depths were measured with
static double osc3_mix_gain(const tP2Plugin * p) {
    double cc = fmin(127.0, plain(p, kP2ParamOsc3Level) + p->mod[eModOsc3Level]);

    return (cc <= LEVEL_SILENT_BELOW) ? 0.0 : OSC3_LEVEL_FULL * pow((cc - LEVEL_SILENT_BELOW) / (127.0 - LEVEL_SILENT_BELOW), OSC3_LEVEL_EXPONENT);
}

// notes §34: a Butterworth low-pass at NOISE_BANDWIDTH_HZ (RBJ), b = {b0, b1, b0}, a = {a1, a2}
static void noise_band_limit_coefficients(double rate, double c[5]) {
    double w     = 2.0 * M_PI * fmin(NOISE_BANDWIDTH_HZ, 0.45 * rate) / rate;
    double alpha = sin(w) / (2.0 * M_SQRT1_2);
    double a0    = 1.0 + alpha;

    c[0] = (1.0 - cos(w)) / (2.0 * a0);
    c[1] = (1.0 - cos(w)) / a0;
    c[2] = c[0];
    c[3] = -2.0 * cos(w) / a0;
    c[4] = (1.0 - alpha) / a0;
}

static double band_limit(double x, const double c[5], double * z) {
    double y = (c[0] * x) + z[0];

    z[0] = (c[1] * x) - (c[3] * y) + z[1];
    z[1] = (c[2] * x) - (c[4] * y);
    return y;
}

// notes §44: an oscillator's level stage, a little asymmetric; its DC goes no further
static double osc_stage(tP2Plugin * p, int k, double v, double hp) {
    double even = (OSC_EVEN * v * v) - p->oscEvenDc[k];

    p->oscEvenDc[k] += hp * even;
    return v + even;
}

static double white_noise(tP2Plugin * p) {
    p->noise ^= p->noise << 13;
    p->noise ^= p->noise >> 17;
    p->noise ^= p->noise << 5;
    return ((double)p->noise / 2147483648.0) - 1.0;
}

// notes §5
static void build_decimator(tP2Plugin * p) {
    double cutoff = 0.45 / p->oversample;    // of the oversampled rate
    double sum    = 0.0;

    for (int i = 0; i < DECIMATOR_TAPS; i++) {
        double n      = i - ((DECIMATOR_TAPS - 1) / 2.0);
        double sinc   = (n == 0.0) ? (2.0 * cutoff) : (sin(2.0 * M_PI * cutoff * n) / (M_PI * n));
        double window = 0.42 - (0.5 * cos((2.0 * M_PI * i) / (DECIMATOR_TAPS - 1))) + (0.08 * cos((4.0 * M_PI * i) / (DECIMATOR_TAPS - 1)));

        p->decimator[i] = sinc * window;
        sum            += p->decimator[i];
    }

    for (int i = 0; i < DECIMATOR_TAPS; i++) {
        p->decimator[i] /= sum;
    }
}

// notes §33: a new factor rebuilds what depends on the rate; the filter and drive start again from rest
static void apply_oversample(tP2Plugin * p) {
    int factor = 1 << plain_int(p, kP2ParamOversample);

    if (factor == p->oversample) {
        return;
    }
    p->oversample = factor;
    build_decimator(p);
    memset(p->history, 0, sizeof(p->history));
    p2_filter_init(&p->filter, p->sampleRate * p->oversample);
    p2_drive_init(&p->drive, p->sampleRate * p->oversample);
    update_dcos(p);
}

// notes §47: an invalid or runaway output - the signal path's state logged, then put back to rest
static void recover_signal_path(tP2Plugin * p, double acc) {
    if (p->faults++ == 0) {
        synthlib_log_line("FAULT: output %g; filter %g %g %g %g, slewed %g, mix %g, filterCoupled %g, drive %g/%g/%g, "
                          "noise %g %g, hiss %g %g %g %g, dco %g %g %g, vca %g, cutoff %g, hiss %d, oversample %d",
                          acc, p->filter.stage[0], p->filter.stage[1], p->filter.stage[2], p->filter.stage[3], p->slewed,
                          p->mixCoupled, p->filterCoupled, p->driveOut, p->drive.coupled, p->drive.inPower,
                          p->noiseLp[0], p->noiseLp[2], p->hissLp[0], p->hissLp[2], p->hissLp[4], p->hissLp[6],
                          p->dco[0].increment, p->dco[1].increment, p->dco[2].increment, p->vca, p->cutoffNow,
                          plain_int(p, kP2ParamHiss), p->oversample);
    } else {
        synthlib_log_line("FAULT again (%u so far)", p->faults);
    }
    p2_filter_reset(&p->filter);
    p2_drive_init(&p->drive, p->sampleRate * p->oversample);
    memset(p->history, 0, sizeof(p->history));
    memset(p->noiseLp, 0, sizeof(p->noiseLp));
    memset(p->hissLp, 0, sizeof(p->hissLp));
    memset(p->oscEvenDc, 0, sizeof(p->oscEvenDc));
    p->slewed      = p->mixCoupled = p->filterCoupled = p->osc3Coupled = p->level2Coupled = p->level2Lag = p->driveOut = 0.0;
    p->feedCoupled = p->vcaCoupled = p->noiseColour = 0.0;
}

static void update_filter(tP2Plugin * p) {
    p2_filter_set(&p->filter, p->cutoffNow, fmin(127.0, fmax(0.0, p->resonance + p->mod[eModResonance])),
                  (tP2FilterMode)plain_int(p, kP2ParamFilterType));
}

static double highest_note(const tP2Plugin * p) {
    double n = (p->noteCount > 0) ? p->notes[0] : p->lastNote;

    for (int i = 1; i < p->noteCount; i++) {
        n = fmax(n, p->notes[i]);
    }

    return n;
}

static double lowest_note(const tP2Plugin * p) {
    double n = (p->noteCount > 0) ? p->notes[0] : p->lastNote;

    for (int i = 1; i < p->noteCount; i++) {
        n = fmin(n, p->notes[i]);
    }

    return n;
}

// notes §17: the per-millisecond pull towards the note, ×2^17 - fitted to the measured glides
static int64_t glide_pull(double rate) {
    double i = fmin(127.0, fmax(0.0, rate)) + 10.0;

    return (i <= 84.0) ? (int64_t)llround(pow(2.0, 17.129 - (0.12697 * i))) : (int64_t)fmax(1.0, round(265.164 - (2.0916 * i)));
}

static void glide_step(tP2Plugin * p, double dt) {
    int64_t target = (int64_t)llround(p->lastNote * 256.0);
    int64_t pull   = glide_pull(plain(p, kP2ParamGlideRate) + p->mod[eModGlideRate]);

    p->glideClock += dt;

    while (p->glideClock >= 0.001) {
        int64_t x = (target - p->glidePitch) * pull;

        // floor + ceil of x / 2^17: at least one unit a millisecond while not there (the measured slow glides)
        p->glidePitch += (x >> 17) - ((-x) >> 17);
        p->glideClock -= 0.001;
    }
}

// notes §10: one control tick - the envelopes advanced by dt, and the targets the audio then glides to
static void control_tick(tP2Plugin * p, double dt) {
    double envF       = p2_env_step(&p->filterEnv, dt, plain(p, kP2ParamEnvFAttack), plain(p, kP2ParamEnvFDecay),
                                    plain(p, kP2ParamEnvFSustain), plain(p, kP2ParamEnvFRelease), (tP2EnvLoop)plain_int(p, kP2ParamEnvFLoop));
    double envA       = p2_env_step(&p->ampEnv, dt, plain(p, kP2ParamEnvAAttack), plain(p, kP2ParamEnvADecay),
                                    plain(p, kP2ParamEnvASustain), plain(p, kP2ParamEnvARelease), (tP2EnvLoop)plain_int(p, kP2ParamEnvALoop));
    double note       = (p->noteCount > 0) ? (double)p->notes[p->noteCount - 1] : p->lastNote;

    glide_step(p, dt);
    double lfo1       = p2_lfo_step(&p->lfo1, dt, fmin(127.0, fmax(0.0, plain(p, kP2ParamLfo1Speed) + p->mod[eModLfo1Speed])), plain_int(p, kP2ParamLfo1Shape));
    double lfo2       = p2_lfo_step(&p->lfo2, dt, plain(p, kP2ParamLfo2Speed), 1) * p->lfo2Fade;       // notes §14

    p->lfo2Fade              = fmin(1.0, p->lfo2Fade + (dt / fmax(p2_env_attack_seconds(plain(p, kP2ParamLfo2Delay)), 1e-3)));

    // notes §13: the eight slots, summed per target
    double source[25] = {0};

    double wheel      = plain(p, kP2ParamModWheel) / 127.0;
    double pressure   = plain(p, kP2ParamPressure) / 127.0;

    source[1]                = lfo1;
    source[2]                = lfo1 * wheel;          // notes §28
    source[3]                = lfo1 * pressure;
    source[11]               = plain(p, kP2ParamPitchBend);
    source[12]               = wheel;
    source[13]               = pressure;
    source[14]               = plain(p, kP2ParamBreath) / 127.0;
    source[16]               = p->releaseVelocity;
    source[4]                = lfo2;
    source[5]                = lfo2 * envA;
    source[6]                = envF;
    source[7]                = envA;
    source[8]                = p->velocity;
    source[9]                = (note - KEYTRACK_REFERENCE_NOTE) / 64.0;
    source[10]               = source[9];
    source[23]               = (highest_note(p) - KEYTRACK_REFERENCE_NOTE) / 64.0;
    source[24]               = (lowest_note(p) - KEYTRACK_REFERENCE_NOTE) / 64.0;
    memcpy(p->source, source, sizeof(p->source));

    // notes §18: the math destinations start each tick afresh, except the delay line and the smoother
    double mod1Shift        = p->mod[eModMod1Amount];
    double sum[MOD_TARGETS] = {0};

    p->math[eMathAccu]       = 0.0;
    p->math[eMathProduct]    = 1.0;
    p->math[eMathMin]        = 1.0;
    p->math[eMathMax]        = -1.0;
    p->math[eMathDelay]      = p->delayLine[p->delayAt];
    p->delayLine[p->delayAt] = 0.0;
    memset(p->mod, 0, sizeof(p->mod));

    for (int m = 0; m < MOD_SLOTS; m++) {
        int    src          = plain_int(p, kP2ParamMod1Source + (3 * m));
        int    target       = plain_int(p, kP2ParamMod1Target + (3 * m));
        double d            = plain(p, kP2ParamMod1Amount + (3 * m)) - 64.0 + ((m == 0) ? mod1Shift : 0.0);

        d            = fmin(63.0, fmax(-64.0, d));

        if ((src <= 0) || (src >= 25) || (target < 0) || (target >= MOD_TARGETS)) {
            continue;
        }
        double in           = (src >= 17 && src <= 22) ? p->math[src - 17] : source[src]; // M-ACCU..M-MAX as sources
        double sign         = (d < 0.0) ? -1.0 : 1.0;

        if (target < eModMathFirst) {
            p->mod[target] += in * sign * mod_square(d) * MOD_DEPTH_PER_SQUARE;
            continue;
        }
        // notes §18
        double contribution = in * sign * mod_square(d) / 4096.0;

        sum[target] += contribution;
        double clipped      = fmin(1.0, fmax(-1.0, sum[target]));

        switch (target - eModMathFirst) {
            case eMathAccu:    p->math[eMathAccu]     = clipped;
                break;
            case eMathProduct: p->math[eMathProduct] *= clipped;
                break;
            case eMathMin:     p->math[eMathMin]      = fmin(p->math[eMathMin], clipped);
                break;
            case eMathMax:     p->math[eMathMax]      = fmax(p->math[eMathMax], clipped);
                break;
            case eMathDelay:
            {
                int ahead = (int)fabs(d) * 16;              // ms
                int at    = (p->delayAt + ahead) % DELAY_LINE_MS;

                p->delayLine[at] = fmin(1.0, fmax(-1.0, p->delayLine[at] + (sign * in)));
                break;
            }
            case eMathSmooth:
            {
                double k = fmax(1.0, fabs(d));

                p->smooth           += ((sign * in) - p->smooth) * fmin(1.0, (k + (k * k / 256.0)) / 2048.0 * (dt / 0.001));
                p->math[eMathSmooth] = p->smooth;
                break;
            }
        }
    }

    p->delayAt      = (p->delayAt + 1) % DELAY_LINE_MS;

    update_dcos(p);

    for (int v = 0; v < PARA_VOICES_MAX; v++) {
        if (p->voice[v].note >= 0) {
            set_voice_pitch(p, &p->voice[v]);
        }
    }

    // notes §15: one VCA curve, of envelope × the volume velocity has moved
    double volume = plain(p, kP2ParamVolume) + (p->velocity * 127.0 * (plain(p, kP2ParamAmpVelocity) - 64.0) / 64.0) + p->mod[eModVolume];

    p->vcaTarget    = p2_vca_gain(envA * fmin(127.0, fmax(0.0, volume)) / 127.0);
    // notes §29: velocity deepens the filter envelope; it is not an offset of its own
    p->cutoffTarget = p->cutoff
                      + (envF * (((plain(p, kP2ParamFilterEnvAmount) - 64.0 + p->mod[eModVcfEnvAmount]) * ENV_CUTOFF_PER_UNIT)
                                 + ((plain(p, kP2ParamFilterVelocity) - 64.0) * p->velocity * 127.0 / 128.0)))
                      + p->mod[eModCutoff]
                      + (((plain(p, kP2ParamFilterKeytrack) - 64.0) / 32.0) * (note - KEYTRACK_REFERENCE_NOTE));
    p->cutoffTarget = fmin(127.0, fmax(0.0, p->cutoffTarget));   // the instrument stops at its range's ends too
}

static bool anomalies_on(const tP2Plugin * p) {
    return plain_int(p, kP2ParamAnomalies) != 0;
}

// notes §7: coeff 1 jumps there; per sample it is glide_coefficient()'s
static double glide_coefficient(const tP2Plugin * p) {
    return 1.0 - exp(-1.0 / (CONTROL_GLIDE_SECONDS * p->sampleRate));
}

static void glide_controls(tP2Plugin * p, double coeff) {
    p->cutoff    += (plain(p, kP2ParamCutoff) - p->cutoff) * coeff;    // the knob, before any modulation
    p->resonance += (plain(p, kP2ParamResonance) - p->resonance) * coeff;
}

// notes §35: the instrument's oscillators run free, so their phases at a note are anyone's; each instance starts its
// own somewhere (the same each time it is created, so an offline render repeats)
static void free_running_phases(tP2Plugin * p) {
    static uint32_t instances = 0;
    uint32_t        seed      = 0x9E3779B9u * ++instances;
    tP2Dco *        dcos[]    = {&p->dco[0], &p->dco[1], &p->dco[2]};

    for (int i = 0; i < 3; i++) {
        seed           = (seed * 1664525u) + 1013904223u;
        dcos[i]->phase = (double)(seed >> 8) / 16777216.0;
    }

    for (int i = 0; i < UNISON_OSCS; i++) {
        seed               = (seed * 1664525u) + 1013904223u;
        p->unison[i].phase = (double)(seed >> 8) / 16777216.0;
    }
}

static void * p2_create(const tSynthLibPluginDesc * desc) {
    tP2Plugin * p = (tP2Plugin *)calloc(1, sizeof(tP2Plugin));

    (void)desc;

    if (p == NULL) {
        return NULL;
    }
    p->sampleRate          = 48000.0;
    p->noise               = 0x9E3779B9u;
    p->oversample          = OVERSAMPLE_DEFAULT;
    free_running_phases(p);
    snprintf(p->soundName, sizeof(p->soundName), "INIT");
    p2_arp_reset(&p->arp);
    p->hwChannelKnown      = -1.0;
    p->hwLock              = -1;
#ifndef P2_NO_EDITOR
    p2_prefs_init();
#endif
    p->filterEnv.timeScale = FILTER_ENV_TIME_SCALE;
    p->ampEnv.timeScale    = 1.0;

    for (uint32_t i = 0; i < kP2ParamCount; i++) {
        p->param[i] = gP2Params[i].defaultNormalized;
    }

    p->oversample          = 1 << plain_int(p, kP2ParamOversample);
    build_decimator(p);
    p2_presets_load();
    p->startPreset         = true;
    p2_filter_init(&p->filter, p->sampleRate * p->oversample);
    p2_drive_init(&p->drive, p->sampleRate * p->oversample);
    glide_controls(p, 1.0);
    p->cutoffNow           = p->cutoffTarget = p->cutoff;
    p->lastNote            = KEYTRACK_REFERENCE_NOTE;
    p->glidePitch          = (int64_t)KEYTRACK_REFERENCE_NOTE * 256;   // notes §42: running before the first note
    update_filter(p);

    synthlib_log_line("created - Pulse2Alike %s", P2A_VERSION_STRING);
    return p;
}

static void p2_destroy(void * inst) {
#ifndef P2_NO_EDITOR
    if (((tP2Plugin *)inst)->backingUp) {
        static uint8_t discard[P2_PATCH_SLOTS][P2_DUMP_LENGTH];

        p2_hardware_backup_cancel();
        (void)p2_hardware_backup_take(discard);
    }
    p2_hardware_release(&((tP2Plugin *)inst)->hwLock);
#endif
    free(inst);
}

// notes §24: the plug-in's own state, after the parameters - the sound's name, the file, the dump a save keeps and
// (P2S2, notes §48) the preset it came from
#define STATE_MAGIC_V1     "P2S1"
#define STATE_MAGIC        "P2S2"
#define STATE_LENGTH_V1    (4 + (P2_SOUND_NAME_LENGTH + 1) + 1 + P2_DUMP_LENGTH + 1024)
#define STATE_LENGTH       (STATE_LENGTH_V1 + 2)

static size_t p2_get_state(void * inst, void * out, size_t len) {
    tP2Plugin * p = (tP2Plugin *)inst;
    uint8_t *   o = (uint8_t *)out;

    if ((out == NULL) || (len < STATE_LENGTH)) {
        return STATE_LENGTH;
    }
    memcpy(o, STATE_MAGIC, 4);
    memcpy(o + 4, p->soundName, P2_SOUND_NAME_LENGTH + 1);
    o[4 + P2_SOUND_NAME_LENGTH + 1] = p->haveBase ? 1 : 0;
    memcpy(o + 4 + P2_SOUND_NAME_LENGTH + 2, p->baseDump, P2_DUMP_LENGTH);
    memcpy(o + 4 + P2_SOUND_NAME_LENGTH + 2 + P2_DUMP_LENGTH, p->patchPath, 1024);
    o[STATE_LENGTH_V1]              = (uint8_t)(p->preset >> 8);
    o[STATE_LENGTH_V1 + 1]          = (uint8_t)(p->preset & 0xFF);
    return STATE_LENGTH;
}

static void p2_set_state(void * inst, const void * data, size_t len) {
    tP2Plugin *     p  = (tP2Plugin *)inst;
    const uint8_t * d  = (const uint8_t *)data;

    bool            v2 = (data != NULL) && (len >= STATE_LENGTH) && (memcmp(d, STATE_MAGIC, 4) == 0);

    if (!v2 && ((data == NULL) || (len < STATE_LENGTH_V1) || (memcmp(d, STATE_MAGIC_V1, 4) != 0))) {
        return;
    }
    atomic_store(&p->startPreset, false);   // a project's sound, not P001
    p->preset                          = v2 ? ((d[STATE_LENGTH_V1] << 8) | d[STATE_LENGTH_V1 + 1]) : 0;
    p->preset                          = ((p->preset >= 1) && (p->preset <= P2_PATCH_SLOTS)) ? p->preset : 0;
    atomic_store(&p->programShown, p->preset);
    memcpy(p->soundName, d + 4, P2_SOUND_NAME_LENGTH + 1);
    p->soundName[P2_SOUND_NAME_LENGTH] = '\0';
    p->haveBase                        = d[4 + P2_SOUND_NAME_LENGTH + 1] != 0;
    memcpy(p->baseDump, d + 4 + P2_SOUND_NAME_LENGTH + 2, P2_DUMP_LENGTH);
    memcpy(p->patchPath, d + 4 + P2_SOUND_NAME_LENGTH + 2 + P2_DUMP_LENGTH, 1024);
    p->patchPath[1023]                 = '\0';

    // notes §48: a project from before the preset was saved - its file's number, or the slot its dump came from
    if (!v2) {
        int slot = (p->haveBase && (p->baseDump[5] != 0x7F)) ? (((p->baseDump[5] << 7) | p->baseDump[6]) + 1) : 0;

        p->preset = (p2_patch_file_number(p->patchPath) != 0) ? p2_patch_file_number(p->patchPath) : slot;
        p->preset = ((p->preset >= 1) && (p->preset <= P2_PATCH_SLOTS)) ? p->preset : 0;
        atomic_store(&p->programShown, p->preset);
    }
}

static void p2_prepare(void * inst, const tSynthLibSetup * setup) {
    tP2Plugin * p = (tP2Plugin *)inst;

    p->sampleRate = setup->sampleRate;

    if (setup->offline) {
        atomic_store(&p->startPreset, false);   // notes §48: an offline render plays what it is given
    }
    p2_filter_init(&p->filter, p->sampleRate * p->oversample);
    p2_drive_init(&p->drive, p->sampleRate * p->oversample);
    glide_controls(p, 1.0);
    p->cutoffNow  = p->cutoffTarget = p->cutoff;
    p->lastNote   = KEYTRACK_REFERENCE_NOTE;
    p->glidePitch = (int64_t)KEYTRACK_REFERENCE_NOTE * 256;
    update_filter(p);
    update_dcos(p);
}

static void p2_reset(void * inst) {
    tP2Plugin * p = (tP2Plugin *)inst;

    p->noteCount     = 0;

    for (int v = 0; v < PARA_VOICES_MAX; v++) {
        p->voice[v] = (tP2Voice){
            .note = -1
        };
    }

    p2_env_reset(&p->ampEnv);
    p2_env_reset(&p->filterEnv);
    p2_arp_reset(&p->arp);
    p->arpActiveSeen = plain_int(p, kP2ParamArpActive);
    memset(p->sustained, 0, sizeof(p->sustained));
    p->vca           = p->vcaTarget = 0.0;
    p->cutoffNow     = p->cutoffTarget = p->cutoff;
    p2_filter_reset(&p->filter);
    memset(p->history, 0, sizeof(p->history));
}

static void p2_set_active(void * inst, bool active) {
    (void)active;
    p2_reset(inst);
}

static void remove_note(tP2Plugin * p, uint8_t note) {
    for (int i = 0; i < p->noteCount; i++) {
        if (p->notes[i] == note) {
            memmove(&p->notes[i], &p->notes[i + 1], (size_t)(p->noteCount - i - 1));
            p->noteCount--;
            return;
        }
    }
}

// notes §3
// notes §11: SNG1/SNG2 trigger on a fresh note only (no other key held), RET1-3 on every note; SNG2 and RET2
// restart from zero, the others from wherever the envelope is
static bool trigger(tP2Env * e, int mode, bool fresh) {
    if ((mode <= 1) && !fresh) {
        return false;
    }

    if ((mode == 1) || (mode == 3)) {
        e->level = 0.0;
    }
    p2_env_gate_on(e);
    return true;
}

static void play_note_on(tP2Plugin * p, uint8_t note, float velocity) {
    remove_note(p, note);
    p->sustained[note & 127] = false;

    if (p->noteCount == NOTE_STACK_SIZE) {
        memmove(&p->notes[0], &p->notes[1], NOTE_STACK_SIZE - 1);
        p->noteCount--;
    }
    bool first = (p->noteCount == 0);

    p->velocity              = velocity;

    p->notes[p->noteCount++] = note;
    p->lastNote              = note;

    // notes §17: OFF jumps, NORM always glides, the fingered modes glide only on a legato note
    int  mode  = plain_int(p, kP2ParamGlideMode);
    bool glide = p->glideKnown && ((mode == 1) || ((mode >= 2) && !first) || p->arpGlide);

    if (!glide) {
        p->glidePitch = (int64_t)note * 256;
    }
    p->glideKnown            = true;
    update_dcos(p);

    if (para_voices(p) > 0) {
        para_note_on(p, note, para_voices(p));
    }

    // notes §27: the filter envelope's trigger restarts LFO 2 (and its delay) and a fast-shaped LFO 1
    if (trigger(&p->filterEnv, plain_int(p, kP2ParamEnvFTrigger), first)) {
        p->lfo2Fade = 0.0;
        p2_lfo_note(&p->lfo2, (tP2LfoPhase)plain_int(p, kP2ParamLfo2Phase));

        if (plain_int(p, kP2ParamLfo1Shape) < LFO_SLOW_SHAPES_FROM) {
            p2_lfo_note(&p->lfo1, (tP2LfoPhase)plain_int(p, kP2ParamLfo1Phase));
        }
    }
    trigger(&p->ampEnv, plain_int(p, kP2ParamEnvATrigger), first);
}

static void release_note(tP2Plugin * p, uint8_t note) {
    remove_note(p, note);
    update_dcos(p);
    para_note_off(p, note);

    if (p->noteCount == 0) {
        p2_env_gate_off(&p->ampEnv);
        p2_env_gate_off(&p->filterEnv);
    }
}

// notes §32: with the arpeggiator on, keys go to it and it plays the voice
static bool arp_on(const tP2Plugin * p) {
    return plain_int(p, kP2ParamArpActive) != eP2ArpOff;
}

static void arp_settings(const tP2Plugin * p, tP2ArpSettings * s) {
    int    accent  = plain_int(p, kP2ParamArpAccent);

    s->active   = plain_int(p, kP2ParamArpActive);
    s->mode     = plain_int(p, kP2ParamArpMode);
    s->octaves  = plain_int(p, kP2ParamArpRange) + 1;
    s->clock    = plain_int(p, kP2ParamArpClock);
    s->duration = plain_int(p, kP2ParamArpStep);
    s->swing    = (plain(p, kP2ParamArpSwing) - 14.0) / 100.0;   // measured: the first of a pair's share
    s->delay    = (plain(p, kP2ParamArpDelay) - 64.0) / 100.0;
    s->length   = plain_int(p, kP2ParamArpLength) + 1;
    s->quirks   = anomalies_on(p);

    for (int i = 0; i < P2_ARP_STEPS; i++) {
        s->steps[i] = (uint8_t)plain_int(p, kP2ParamArpStep1 + (uint32_t)i);
    }

    // notes §32: a bipolar source is centred on 64, a unipolar one runs from 0 (the instrument's own scaling)
    bool   bipolar = (accent == 1) || (accent == 2) || (accent == 4) || (accent == 5) || ((accent >= 9) && (accent <= 11));
    double v       = p->source[(accent > 0) && (accent < 25) ? accent : 0];

    s->accent   = (accent == 0) ? -1.0 : fmin(127.0, fmax(0.0, bipolar ? (64.0 + (63.0 * v)) : (127.0 * v)));
}

static void play_note_off(tP2Plugin * p, uint8_t note);

static void arp_note(void * user, int note, int velocity, bool on, bool glide) {
    tP2Plugin * p = (tP2Plugin *)user;

    if (on) {
        p->arpGlide = glide;
        play_note_on(p, (uint8_t)note, (float)velocity / 127.0f);
        p->arpGlide = false;
    } else {
        play_note_off(p, (uint8_t)note);
    }
}

// notes §45: Arp Active changed (by hand or by a program change) - the voice handed between the keys and the arp
static void arp_follow_active(tP2Plugin * p) {
    int active = plain_int(p, kP2ParamArpActive);
    int was    = p->arpActiveSeen;

    if (active == was) {
        return;
    }
    p->arpActiveSeen = active;

    if (was == eP2ArpOff) {
        while (p->noteCount > 0) {
            release_note(p, p->notes[p->noteCount - 1]);
        }
        memset(p->sustained, 0, sizeof(p->sustained));
    }

    if ((active == eP2ArpOn) || ((active == eP2ArpHold) && (was == eP2ArpOff) && !anomalies_on(p))) {
        p2_arp_forget_released(&p->arp);
    }
}

static void p2_note_on(void * inst, uint8_t channel, uint8_t note, float velocity, uint32_t sampleOffset) {
    tP2Plugin *    p = (tP2Plugin *)inst;
    tP2ArpSettings s;

    (void)channel;
    (void)sampleOffset;

    arp_follow_active(p);
    arp_settings(p, &s);
    p2_arp_key(&p->arp, &s, note, (int)lround(velocity * 127.0f), true);   // notes §45: the arp knows every key

    if (!arp_on(p)) {
        play_note_on(p, note, velocity);
    }
}

static void play_note_off(tP2Plugin * p, uint8_t note) {
    // notes §28: with the pedal down the note goes on sounding until the pedal is lifted
    if (plain(p, kP2ParamSustain) >= 64.0) {
        p->sustained[note & 127] = true;
        return;
    }
    release_note(p, note);
}

static void p2_note_off(void * inst, uint8_t channel, uint8_t note, float velocity, uint32_t sampleOffset) {
    tP2Plugin *    p = (tP2Plugin *)inst;

    (void)channel;
    (void)sampleOffset;

    p->releaseVelocity = velocity;
    arp_follow_active(p);
    tP2ArpSettings s;

    arp_settings(p, &s);
    p2_arp_key(&p->arp, &s, note, 0, false);

    if (!arp_on(p)) {
        play_note_off(p, note);
    }
}

static void release_sustained(tP2Plugin * p) {
    for (int n = 0; n < 128; n++) {
        if (p->sustained[n]) {
            p->sustained[n] = false;
            release_note(p, (uint8_t)n);
        }
    }
}

// notes §43: the mixer's levels, with their modulation, as of this control tick
static void level_targets(tP2Plugin * p) {
    int32_t shape3 = plain_int(p, kP2ParamOsc3Shape);

    p->levelTarget[0] = level_gain(p, kP2ParamOsc1Level);
    p->levelTarget[1] = level_gain(p, kP2ParamOsc2Level);
    p->levelTarget[2] = (shape3 != 3) ? level_gain(p, kP2ParamOsc3Level) : 0.0;   // EXT: no input in the plug-in yet
    p->levelTarget[3] = level_gain(p, kP2ParamNoiseLevel);
    p->levelTarget[4] = LEVEL_FEEDTHROUGH * (p->levelTarget[0] + p->levelTarget[1] + p->levelTarget[2]);

    if (!p->levelsSet) {
        memcpy(p->levelNow, p->levelTarget, sizeof(p->levelNow));
        p->levelsSet = true;
    }
}

static void preset_change(tP2Plugin * p, int number);

static void p2_process(void * inst, const float *const * in, uint32_t numIn, float ** out, uint32_t numOut,
                       uint32_t frames, const tSynthLibTransport * t) {
    tP2Plugin *    p         = (tP2Plugin *)inst;

    if (atomic_exchange(&p->startPreset, false)) {   // notes §48
        preset_change(p, 1);
    }
    // notes §30: a pulse is quieter than the DCO's saw; the three oscillators' share in the mix
    int32_t        shape1    = plain_int(p, kP2ParamOsc1Shape);
    double         mix1      = ((shape1 == 1) || (shape1 == 2)) ? 1.0 : PULSE_LEVEL;
    double         mix2      = (plain_int(p, kP2ParamOsc2Shape) == 0) ? PULSE_LEVEL : 1.0;
    int32_t        shape3    = plain_int(p, kP2ParamOsc3Shape);
    bool           feedback  = shape3 == 4;
    double         coupling  = 1.0 - exp(-2.0 * M_PI * OSC3_COUPLING_HZ / (p->sampleRate * p->oversample));
    double         coupling2 = 1.0 - exp(-1.0 / (LEVEL2_COUPLING_S * p->sampleRate * p->oversample));
    double         lag2      = 1.0 - exp(-1.0 / (LEVEL2_LAG_S * p->sampleRate * p->oversample));
    double         depth2    = pow(fmax(0.0, fmin(127.0, plain(p, kP2ParamOsc3Level) + p->mod[eModOsc3Level]) - LEVEL2_ZERO) / (127.0 - LEVEL2_ZERO), LEVEL2_POWER);
    int            routing   = feedback ? 0 : plain_int(p, kP2ParamOsc3Routing);     // notes §25
    double         mixHp     = 1.0 - exp(-2.0 * M_PI * MIX_COUPLING_HZ / (p->sampleRate * p->oversample));
    double         feedHp    = 1.0 - exp(-2.0 * M_PI * FEEDTHROUGH_COUPLING_HZ / (p->sampleRate * p->oversample));
    double         evenHp    = 1.0 - exp(-2.0 * M_PI * OSC_EVEN_COUPLING_HZ / (p->sampleRate * p->oversample));
    double         vcaHp     = 1.0 - exp(-2.0 * M_PI * VCA_COUPLING_HZ / (p->sampleRate * p->oversample));
    double         mixGain   = OSC_INTO_FILTER * ((routing != 0) ? ROUTED_MIX_GAIN : 1.0);

    p->filter.leakScale = 1.0 / ((routing != 0) ? ROUTED_MIX_GAIN : 1.0);   // notes §37
    int            curve     = plain_int(p, kP2ParamDriveCurve);
    double         drive     = plain(p, kP2ParamDrive) + p->mod[eModDrive];
    double         rateIn    = p->sampleRate * p->oversample;
    double         noiseRate = NOISE_LEVEL * sqrt(rateIn / NOISE_REFERENCE_RATE);
    double         noiseLp[5];
    double         colour    = 1.0 - exp(-2.0 * M_PI * NOISE_COLOUR_HZ / rateIn);
    double         hiss      = (plain_int(p, kP2ParamHiss) != 0) ? sqrt(rateIn / NOISE_REFERENCE_RATE) : 0.0;
    int            voices    = para_voices(p);
    bool           unison    = unison_mode(p) != 0;
    double         paraGain  = (voices == 8) ? PARA8_VOICE_LEVEL : PARA4_VOICE_LEVEL;   // notes §12
    double         fadeStep  = 1.0 / (PARA_FADE_SECONDS * p->sampleRate * p->oversample);
    // notes §26: a released voice fades only while other keys hold the amp envelope; with every key up, the
    // envelope's own release is the release
    double         fadeOut1  = (p->noteCount > 0) ? voice_fade_out(p, kP2ParamOsc1Fade) : 1.0;
    double         fadeOut2  = (p->noteCount > 0) ? voice_fade_out(p, kP2ParamOsc2Fade) : 1.0;
    bool           steps     = anomalies_on(p);
    double         master    = pow(plain(p, kP2ParamMaster) / 127.0, 2.0); // notes §19
    int            tick      = steps ? (int)lround(CONTROL_TICK_SECONDS * p->sampleRate) : 1;
    double         smooth    = 1.0 - exp(-1.0 / (CONTROL_SMOOTH_SECONDS * p->sampleRate));
    double         glide     = glide_coefficient(p);

    noise_band_limit_coefficients(rateIn, noiseLp);

    if ((curve == eP2DriveFx) && !steps) {
        curve = eP2DriveNone;      // notes §25: FX is silent on the instrument, a measured quirk
    }
    (void)in;
    (void)numIn;

    tP2ArpSettings arp;
    bool           hostClock = (t != NULL) && t->playing && t->tempoValid && t->musicTimeValid && (t->tempo > 0.0);
    double         bpm       = hostClock ? t->tempo : (46.0 + (2.0 * plain(p, kP2ParamArpTempo)));   // notes §32
    double         tickStep  = bpm / 60.0 * P2_ARP_TICKS_PER_QUARTER / p->sampleRate;

    arp_follow_active(p);
    arp_settings(p, &arp);

    if (hostClock) {
        p->arpTicks = t->projectTimeMusic * P2_ARP_TICKS_PER_QUARTER;
    }

    for (uint32_t i = 0; i < frames; i++) {
        if (arp.active != eP2ArpOff || p->arp.running) {
            p2_arp_run(&p->arp, &arp, p->arpTicks, hostClock, arp_note, p);
        }
        p->arpTicks  += tickStep;
        glide_controls(p, glide);

        if (--p->tickCountdown <= 0) {
            control_tick(p, (double)tick / p->sampleRate);
            level_targets(p);
            p->tickCountdown = tick;
        }

        if (!p->levelsSet) {
            level_targets(p);
        }

        for (int k = 0; k < 5; k++) {
            p->levelNow[k] += (p->levelTarget[k] - p->levelNow[k]) * smooth;
        }

        double level1 = p->levelNow[0], level2 = p->levelNow[1], level3 = p->levelNow[2];
        double noise = noiseRate * p->levelNow[3];
        p->vca       += (p->vcaTarget - p->vca) * smooth;
        p->cutoffNow += (p->cutoffTarget - p->cutoffNow) * smooth;
        update_filter(p);
        // notes §53: moves only with the control tick, so worked out once a sample rather than once an oversample
        double gain3 = (!feedback && (routing == 0)) ? osc3_mix_gain(p) : 0.0;

        for (int o = 0; o < p->oversample; o++) {
            double osc3    = p2_dco_next(&p->dco[2]);
            bool   sync    = plain_int(p, kP2ParamOsc3Sync) != 0;

            p->dco[1].syncHold = sync;

            if (sync && p->dco[2].wrapped) {          // notes §22: osc 3's edge restarts osc 2
                p2_dco_restart(&p->dco[1]);
            }
            // notes §34: the floor a self-oscillation starts from; band-limited, so the level is the same at any rate
            double white   = band_limit(band_limit((noise + NOISE_FLOOR) * white_noise(p), noiseLp, &p->noiseLp[0]), noiseLp, &p->noiseLp[2]);

            p->noiseColour    += colour * (white - p->noiseColour);
            double sources = p->noiseColour;
            double osc     = 0.0;                             // the oscillators alone
            double cv3     = osc3 - p->osc3Coupled;           // notes §25

            p->osc3Coupled    += coupling * cv3;
            double amp2    = level2;

            if (feedback) {
                // notes §25: subtracted, as on the instrument; the seed is the noise floor a loop starts from
                sources -= level3 * OSC3_FB_GAIN * p->driveOut;
            } else if (routing == 0) {
                osc += osc_stage(p, 2, gain3 * osc3 * ((shape3 == 0) ? PULSE_LEVEL : 1.0), evenHp);
            } else if (routing == 1) {
                // notes §52: osc 3 inverted and AC coupled, then osc 2's level times (1 + depth x it), floored
                double l2 = -osc3 - p->level2Coupled;

                p->level2Coupled += coupling2 * l2;
                p->level2Lag     += lag2 * (l2 - p->level2Lag);
                double k2 = (p->level2Lag >= 0.0) ? LEVEL2_RISE : LEVEL2_FALL;

                amp2              = LEVEL2_MEASURED
                                    ? fmin(LEVEL2_CEILING, fmax(0.0, level2 * (1.0 + (k2 * depth2 * p->level2Lag))))
                                    : fmax(0.0, level2 + (level3 * cv3 * OSC3_TO_LEVEL2));
            } else if (routing == 2) {
                p2_filter_set(&p->filter, fmin(127.0, fmax(0.0, p->cutoffNow + (level3 * cv3 * OSC3_TO_CUTOFF_STEPS))),
                              fmin(127.0, fmax(0.0, p->resonance + p->mod[eModResonance])), (tP2FilterMode)plain_int(p, kP2ParamFilterType));
            }

            if (unison) {
                double sum = 0.0;

                for (int u = 0; u < UNISON_OSCS; u++) {
                    sum += p2_dco_next(&p->unison[u]);
                }

                osc += (mix1 * level1 * UNISON_VOICE_GAIN * sum) + (mix2 * amp2 * p2_dco_next(&p->dco[1]));
            } else if (voices == 0) {
                osc += osc_stage(p, 0, mix1 * level1 * p2_dco_next(&p->dco[0]), evenHp) + osc_stage(p, 1, mix2 * amp2 * p2_dco_next(&p->dco[1]), evenHp);
            } else {
                // notes §12: PARA-8 one pulse per voice, PARA-4 two; each fades on its own
                for (int v = 0; v < voices; v++) {
                    tP2Voice * pv = &p->voice[v];

                    pv->fade  = pv->held ? fmin(1.0, pv->fade + fadeStep) : ((pv->fade * fadeOut1 < FADE_SILENT) ? 0.0 : pv->fade * fadeOut1);
                    pv->fade2 = pv->held ? fmin(1.0, pv->fade2 + fadeStep) : ((pv->fade2 * fadeOut2 < FADE_SILENT) ? 0.0 : pv->fade2 * fadeOut2);

                    if ((pv->fade > 0.0) || (pv->fade2 > 0.0)) {
                        double pulse = pv->fade * p2_dco_next(&pv->a);

                        if (voices == 4) {
                            pulse += (level2 / fmax(level1, 1e-9)) * pv->fade2 * p2_dco_next(&pv->b);
                        }
                        osc += PULSE_LEVEL * paraGain * level1 * pulse;     // both of a voice's pulses
                    }
                }
            }
            // notes §6: oscillators and filter run all the time; the amp envelope is the VCA, after the filter
            osc                       = sources - osc; // notes §39: the oscillators' polarity, as the instrument's

            if (hiss > 0.0) {
                osc += band_limit(band_limit(HISS_INTO_FILTER * hiss * white_noise(p), noiseLp, &p->hissLp[0]), noiseLp, &p->hissLp[2]);
            }
            double coupled  = osc - p->mixCoupled;

            p->mixCoupled            += mixHp * coupled;
            double step     = MIX_SLEW_PER_SECOND / rateIn; // notes §40

            double into     = mixGain * coupled;
            p->slewed                += fmax(-step, fmin(step, into - p->slewed));
            double filtered = p2_filter_process(&p->filter, p->slewed) - p->filterCoupled;

            p->filterCoupled         += mixHp * filtered;
            // notes §44: no saturation after the filter - the instrument's open filter is clean at every level

            // notes §43: the level controls' leak, coupled on its own
            double leak     = p->levelNow[4] - p->feedCoupled;

            p->feedCoupled           += feedHp * leak;
            filtered                 += leak;

            if (hiss > 0.0) {
                filtered += band_limit(band_limit(HISS_AFTER_FILTER * hiss * white_noise(p), noiseLp, &p->hissLp[4]), noiseLp, &p->hissLp[6]);
            }
            p->driveOut               = p2_drive_process(&p->drive, (tP2DriveCurve)curve,
                                                         drive + ((routing == 3) ? (level3 * cv3 * OSC3_TO_DRIVE) : 0.0), filtered);
            double vcaLeak  = (VCA_FEEDTHROUGH * p->vca) - p->vcaCoupled;  // notes §43

            p->vcaCoupled            += vcaHp * vcaLeak;
            double y        = (p->driveOut * p->vca) + vcaLeak;

            p->history[p->historyPos] = y;
            p->historyPos             = (p->historyPos + 1) % DECIMATOR_TAPS;
        }

        double acc    = 0.0;

        for (int k = 0; (p->oversample > 1) && (k < DECIMATOR_TAPS); k++) {
            acc += p->decimator[k] * p->history[(p->historyPos + DECIMATOR_TAPS - 1 - k) % DECIMATOR_TAPS];
        }

        if (p->oversample == 1) {
            acc = p->history[(p->historyPos + DECIMATOR_TAPS - 1) % DECIMATOR_TAPS];
        }

        if (!isfinite(acc) || (fabs(OUTPUT_GAIN * acc) > FAULT_LEVEL)) {
            recover_signal_path(p, acc);
            acc = 0.0;
        }
        float  sample = (float)(OUTPUT_GAIN * acc * master);

        for (uint32_t ch = 0; ch < numOut; ch++) {
            if ((out != NULL) && (out[ch] != NULL)) {
                out[ch][i] = sample;
            }
        }
    }
}

// notes §20: a preset by number - the cached sound, onto every parameter it carries, and the host told of each
static void preset_change(tP2Plugin * p, int number) {
    double values[kP2ParamCount];
    char   name[P2_SOUND_NAME_LENGTH + 1];

    // notes §20: the sound already loaded is left as it is, edits and all
    if (number == atomic_load(&p->programShown)) {
        return;
    }
    memcpy(values, p->param, sizeof(values));

    if (!p2_patch_cached(number, values, name)) {
        return;
    }

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        if (  (id == kP2ParamOversample) || (id == kP2ParamAnomalies) || (id == kP2ParamHiss) || (id == kP2ParamHwLink)
           || (id == kP2ParamHwChannel) || (id == kP2ParamMaster)
           || (id == kP2ParamProgram) || (id == kP2ParamBank) || ((id >= kP2ParamPitchBend) && (id <= kP2ParamSustain)) || (values[id] == p->param[id])) {
            continue;
        }
        p->param[id] = values[id];
        synthlib_plugin_param_edited(p, id, values[id]);
    }

    update_dcos(p);
    atomic_store(&p->programShown, number);
}

static void program_change(tP2Plugin * p, int program) {
    preset_change(p, p2_patch_program_number(plain_int(p, kP2ParamBank), program));
}

static void p2_set_param(void * inst, uint32_t id, double normalized) {
    tP2Plugin * p = (tP2Plugin *)inst;

    if ((id == kP2ParamSustain) && (normalized < 0.5) && (p->param[id] >= 0.5)) {
        p->param[id] = normalized;
        release_sustained(p);
        return;
    }

    if (id == kP2ParamOversample) {
        p->param[id] = normalized;
        apply_oversample(p);
        return;
    }

    if (id == kP2ParamProgram) {
        p->param[id] = normalized;
        program_change(p, (int)lround(normalized * 127.0));
        return;
    }

    if (id < kP2ParamCount) {
        p->param[id] = normalized;
        update_dcos(p);
    }
}

static double p2_get_param(void * inst, uint32_t id) {
    return (id < kP2ParamCount) ? ((tP2Plugin *)inst)->param[id] : 0.0;
}

static bool p2_param_text(const tSynthLibPluginDesc * desc, void * inst, uint32_t id, double value, char * out, size_t len) {
    const char *const * names = (id < kP2ParamCount) ? p2_param_names(id) : NULL;

    (void)desc;
    (void)inst;

    if (names == NULL) {
        return false;
    }
    long                entry = lround(value * (gP2Params[id].plainMax - gP2Params[id].plainMin));

    snprintf(out, len, "%s", names[(entry < 0) ? 0 : entry]);
    return true;
}

#ifndef P2_NO_EDITOR
// ---- the editor ---------------------------------------------------------------------------------

// A dump's name as the panel shows it: the trailing spaces that pad it to 14 dropped
static void set_sound_name(tP2Plugin * p, const char * name) {
    size_t n;

    snprintf(p->soundName, sizeof(p->soundName), "%s", name);
    n = strlen(p->soundName);

    while ((n > 0) && (p->soundName[n - 1] == ' ')) {
        p->soundName[--n] = '\0';
    }
}

static void p2_load_patch(void * user, const char * path);
static bool p2_save_patch(void * user, const char * path);
static void p2_init_patch(void * user);
static void p2_edit_param(void * user, uint32_t id, double normalized);
static const char * p2_sound_name(void * user);
static void p2_rename(void * user, const char * name);
static int p2_send_to_hardware(void * user);
static void p2_load_preset(void * user, int number);
static bool p2_store_preset(void * user, int number);
static int p2_preset_number(void * user);
static int p2_backup_start(void * user);
static void p2_backup_cancel(void * user);

static const tP2PatchHost gPatchHost = {
    .load       = p2_load_patch,   .save         = p2_save_patch,   .init   = p2_init_patch,       .edit = p2_edit_param,
    .name       = p2_sound_name,   .rename       = p2_rename,       .send   = p2_send_to_hardware,
    .loadPreset = p2_load_preset,  .storePreset  = p2_store_preset, .preset = p2_preset_number,
    .backup     = p2_backup_start, .cancelBackup = p2_backup_cancel
};

static void p2_panel_init(void) {
    p2_draw_init();
    p2_draw_set_pages(gP2Pages, gP2PageCount);
    p2_draw_set_host(&gPatchHost);
}

// What a sound carries: everything but the plug-in's own settings and the program change's two
static bool sound_param(uint32_t id) {
    return (id != kP2ParamOversample) && (id != kP2ParamAnomalies) && (id != kP2ParamHiss) && (id != kP2ParamHwLink)
           && (id != kP2ParamHwChannel) && (id != kP2ParamMaster)
           && (id != kP2ParamProgram) && (id != kP2ParamBank) && ((id < kP2ParamPitchBend) || (id > kP2ParamSustain));
}

static void p2_edit_param(void * user, uint32_t id, double normalized) {
    synthlib_plugin_param_edited(user, id, normalized);
}

static const char * p2_sound_name(void * user) {
    return ((tP2Plugin *)user)->soundName;
}

// notes §24: the name only - written to a file at the next save
static void p2_rename(void * user, const char * name) {
    set_sound_name((tP2Plugin *)user, name);
}

// notes §23: every sound parameter to its default; the next save has no dump to keep bytes from
static void p2_init_patch(void * user) {
    tP2Plugin * p = (tP2Plugin *)user;

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        if (sound_param(id)) {
            synthlib_plugin_param_edited(user, id, gP2Params[id].defaultNormalized);
        }
    }

    p->haveBase     = false;
    p->patchPath[0] = '\0';
    p->preset       = 0;
    atomic_store(&p->programShown, 0);
    set_sound_name(p, "INIT");
}

// notes §23: the sound's name from the file's - without the folder, ".syx" and a leading "NNN ", in capitals
static void name_from_path(const char * path, char name[P2_SOUND_NAME_LENGTH + 1]) {
    const char * slash = strrchr(path, '/');
    const char * from  = (slash != NULL) ? slash + 1 : path;
    size_t       n     = strlen(from);
    int          k     = 0;

    if ((n > 4) && (strcmp(from + n - 4, ".syx") == 0)) {
        n -= 4;
    }

    if ((n > 4) && isdigit((unsigned char)from[0]) && isdigit((unsigned char)from[1]) && isdigit((unsigned char)from[2]) && (from[3] == ' ')) {
        from += 4;
        n    -= 4;
    }

    for (size_t i = 0; (i < n) && (k < P2_SOUND_NAME_LENGTH); i++) {
        char c = (char)toupper((unsigned char)from[i]);

        name[k++] = ((c >= 32) && (c < 127)) ? c : ' ';
    }

    name[k] = '\0';
}

// The sound as it is now, as a dump: the host's values over the dump it came from
static void current_dump(void * user, const char * path, uint8_t dump[P2_DUMP_LENGTH]) {
    tP2Plugin * p = (tP2Plugin *)user;
    double      values[kP2ParamCount];
    char        name[P2_SOUND_NAME_LENGTH + 1];

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        values[id] = synthlib_plugin_param_value(user, id);
    }

    if (p->soundName[0] != '\0') {
        snprintf(name, sizeof(name), "%s", p->soundName);
    } else {
        name_from_path(path, name);
    }
    p2_patch_to_syx(values, p->haveBase ? p->baseDump : NULL, name, dump);
}

static bool p2_save_patch(void * user, const char * path) {
    tP2Plugin * p = (tP2Plugin *)user;
    char        name[P2_SOUND_NAME_LENGTH + 1];
    uint8_t     dump[P2_DUMP_LENGTH];

    current_dump(user, path, dump);
    memcpy(name, dump + 7 + P2_SOUND_NAME_OFFSET, P2_SOUND_NAME_LENGTH);
    name[P2_SOUND_NAME_LENGTH] = '\0';

    if (!p2_patch_save_file(path, dump)) {
        synthlib_log_line("could not write %s", path);
        return false;
    }
    memcpy(p->baseDump, dump, sizeof(dump));
    p->haveBase                = true;
    snprintf(p->patchPath, sizeof(p->patchPath), "%s", path);
    synthlib_log_line("saved \"%s\" to %s", name, path);
    return true;
}

// notes §16: a .syx chosen on the panel - every parameter the sound carries, through the host, so the change
// is one the host sees (automation, undo) and the processor receives
static void p2_load_patch(void * user, const char * path) {
    double values[kP2ParamCount];
    char   name[P2_SOUND_NAME_LENGTH + 1];

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        values[id] = synthlib_plugin_param_value(user, id);
    }

    if (!p2_patch_load_file(path, values, name)) {
        synthlib_log_line("not a Pulse 2 sound dump: %s", path);
        return;
    }

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        if (sound_param(id)) {
            synthlib_plugin_param_edited(user, id, values[id]);
        }
    }

    ((tP2Plugin *)user)->haveBase = p2_patch_read_dump(path, ((tP2Plugin *)user)->baseDump);
    snprintf(((tP2Plugin *)user)->patchPath, sizeof(((tP2Plugin *)user)->patchPath), "%s", path);
    set_sound_name((tP2Plugin *)user, name);
    ((tP2Plugin *)user)->preset   = 0; // notes §48: a file is not a preset until it is stored to one
    atomic_store(&((tP2Plugin *)user)->programShown, 0);
    synthlib_log_line("loaded \"%s\" from %s", name, path);
}

// notes §48: a preset chosen on the panel (by number, Prev/Next, Revert) - as a file is, from the cache
static void p2_load_preset(void * user, int number) {
    tP2Plugin * p = (tP2Plugin *)user;
    double      values[kP2ParamCount];
    char        name[P2_SOUND_NAME_LENGTH + 1];
    uint8_t     dump[P2_DUMP_LENGTH];

    if ((number < 1) || (number > P2_PATCH_SLOTS)) {
        return;
    }
    p2_presets_refresh();
    p2_presets_dump(number, dump);

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        values[id] = synthlib_plugin_param_value(user, id);
    }

    if (!p2_patch_from_syx(dump, P2_DUMP_LENGTH, values, name)) {
        return;
    }

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        if (sound_param(id)) {
            synthlib_plugin_param_edited(user, id, values[id]);
        }
    }

    memcpy(p->baseDump, dump, P2_DUMP_LENGTH);
    p->haveBase     = true;
    p->patchPath[0] = '\0';
    p->preset       = number;
    atomic_store(&p->programShown, number);
    set_sound_name(p, name);
}

// notes §48: the sound as it is now into preset n - the file and the cache, which program changes then play
static bool p2_store_preset(void * user, int number) {
    tP2Plugin * p = (tP2Plugin *)user;
    uint8_t     dump[P2_DUMP_LENGTH];

    current_dump(user, "", dump);

    if (!p2_presets_store(number, dump)) {
        synthlib_log_line("could not store P%03d to %s", number, p2_presets_path());
        return false;
    }
    p2_presets_dump(number, p->baseDump);
    p->haveBase = true;
    p->preset   = number;
    atomic_store(&p->programShown, number);
    synthlib_log_line("stored \"%s\" to P%03d", p->soundName, number);
    return true;
}

static int p2_preset_number(void * user) {
    return ((tP2Plugin *)user)->preset;
}

// notes §48: edited since its recall or store - the sound's bytes against the preset's, address and checksum aside
static bool preset_edited(tP2Plugin * p, void * user) {
    uint8_t now[P2_DUMP_LENGTH], stored[P2_DUMP_LENGTH];

    if (p->preset == 0) {
        return false;
    }
    current_dump(user, "", now);
    p2_presets_dump(p->preset, stored);
    return memcmp(now + 7, stored + 7, P2_DUMP_LENGTH - 9) != 0;
}

// ---- the instrument, over USB MIDI (notes §41) --------------------------------------------------

#define HW_DUMP_INTERVAL       (0.1) // seconds between dumps while something without a CC is moving
#define HW_CHANGES_FOR_DUMP    (8)   // more changes than this at once (a new sound) go as one dump
#define HW_PROGRAM_SETTLE      (0.4) // seconds the instrument takes to load a program before the dump may follow

// The parameters the instrument takes by CC, as the measurement rig drives it
static const struct {
    uint32_t id;
    uint8_t  cc;
} kHwCc[] = {
    {kP2ParamOsc1Shape,       34}, {kP2ParamOsc1Width,   35}, {kP2ParamOsc1Semitone,     70}, {kP2ParamOsc1Detune,      71},
    {kP2ParamOsc1Keytrack,    73}, {kP2ParamOsc1Level,   45}, {kP2ParamOsc2Shape,        38}, {kP2ParamOsc2Width,       39},
    {kP2ParamOsc2Semitone,    36}, {kP2ParamOsc2Detune,  37}, {kP2ParamOsc2Keytrack,     40}, {kP2ParamOsc2Level,       46},
    {kP2ParamOsc3Shape,       44}, {kP2ParamOsc3Routing, 72}, {kP2ParamOsc3Semitone,     42}, {kP2ParamOsc3Detune,      43},
    {kP2ParamOsc3Sync,        41}, {kP2ParamOsc3Level,   47}, {kP2ParamNoiseLevel,       48}, {kP2ParamCutoff,          50},
    {kP2ParamResonance,       56}, {kP2ParamFilterType,  49}, {kP2ParamFilterEnvAmount,  52}, {kP2ParamFilterKeytrack,  51},
    {kP2ParamFilterVelocity,  53}, {kP2ParamDrive,       76}, {kP2ParamDriveCurve,       77}, {kP2ParamPan,             59},
    {kP2ParamVolume,          57}, {kP2ParamAmpVelocity, 58}, {kP2ParamEnvFAttack,       14}, {kP2ParamEnvFDecay,       15},
    {kP2ParamEnvFSustain,     16}, {kP2ParamEnvFRelease, 17}, {kP2ParamEnvFLoop,         74}, {kP2ParamEnvFTrigger,     29},
    {kP2ParamEnvAAttack,      18}, {kP2ParamEnvADecay,   19}, {kP2ParamEnvASustain,      20}, {kP2ParamEnvARelease,     21},
    {kP2ParamEnvALoop,        75}, {kP2ParamEnvATrigger, 31}, {kP2ParamGlideRate,         5}, {kP2ParamGlideMode,       62},
    {kP2ParamArpActive,       79}, {kP2ParamArpRange,    80}, {kP2ParamArpTempo,         81}, {kP2ParamArpClock,        82},
    {kP2ParamArpMode,         83}, {kP2ParamArpStep,     84}, {kP2ParamArpSwing,         85}, {kP2ParamArpDelay,        86},
    {kP2ParamArpLength,       87}, {kP2ParamArpAccent,   88}, {kP2ParamUnisonDetune,    102}, {kP2ParamOsc1Fade,       103},
    {kP2ParamOsc2Fade,       104},
};

static int hw_cc(uint32_t id) {
    for (size_t i = 0; i < (sizeof(kHwCc) / sizeof(kHwCc[0])); i++) {
        if (kHwCc[i].id == id) {
            return kHwCc[i].cc;
        }
    }

    return -1;
}

static double now_seconds(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (t.tv_nsec * 1e-9);
}

static int p2_send_to_hardware(void * user) {
    tP2Plugin *       p         = (tP2Plugin *)user;
    bool              temporary = p->hwLock < 0;   // notes §49: a one-off send holds it only while it sends
    uint8_t           dump[P2_DUMP_LENGTH];
    tP2HardwareResult result;

    if (!p2_hardware_claim(&p->hwLock)) {
        return (int)eP2HardwareBusy;
    }
    current_dump(user, p->patchPath, dump);
    result = p2_hardware_send_to_edit_buffer(dump);

    if (temporary) {
        p2_hardware_release(&p->hwLock);
    }
    return (int)result;
}

// notes §41: run from the editor's frame - every sound parameter that moved since the last send goes by its CC,
// or, if it has none (or a whole new sound arrived), the sound goes as one edit-buffer dump, at most every 0.1 s
static void hardware_follow(tP2Plugin * p, void * user) {
    tP2HardwareResult result  = eP2HardwareSent;
    uint8_t           ccs[kP2ParamCount][3];
    int               ccCount = 0;
    int               changes = 0;
    int               channel = (int)lround(synthlib_plugin_param_value(user, kP2ParamHwChannel) * 15.0);

    if (p->backingUp) {          // notes §50: the instrument is busy answering; Link catches up afterwards
        p->hwValid = false;
        return;
    }

    if (synthlib_plugin_param_value(user, kP2ParamHwLink) < 0.5) {
        p->hwValid = false;
        p->hwBusy  = false;
        p2_hardware_release(&p->hwLock);
        return;
    }
    // notes §49: Link on but the instrument is another instance's - idle until it is free, then all of the sound
    p->hwBusy     = !p2_hardware_claim(&p->hwLock);

    if (p->hwBusy) {
        p->hwValid = false;
        return;
    }
    p->hwDumpDue |= !p->hwValid;

    // notes §41: a sound from the library goes as its program change too, so the instrument shows its number; the
    // dump follows once the instrument has loaded it, and carries any edits
    int number = p->preset;

    if (!p->hwValid) {
        p->hwProgram = 0;
    }

    if ((number > 0) && (number != p->hwProgram)) {
        uint8_t change[5] = {(uint8_t)(0xB0 | channel), 32, (uint8_t)((number - 1) / 100), (uint8_t)(0xC0 | channel), (uint8_t)((number - 1) % 100)};

        result             = p2_hardware_send(change, sizeof(change));
        synthlib_log_line("Link: P%03d as bank %d program %d on channel %d (result %d)", number, change[2], change[4], channel + 1, (int)result);
        p->hwProgram       = number;
        p->hwDumpDue       = true;
        p->hwDumpNotBefore = now_seconds() + HW_PROGRAM_SETTLE;
    }

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        double v    = synthlib_plugin_param_value(user, id);

        if (!sound_param(id) || (p->hwValid && (v == p->hwSent[id]))) {
            continue;
        }
        int    cc   = hw_cc(id);
        int    byte = p2_patch_param_byte(id, v);

        p->hwSent[id] = v;
        changes++;

        if ((cc < 0) || (byte < 0)) {
            p->hwDumpDue = true;
        } else {
            ccs[ccCount][0] = (uint8_t)(0xB0 | channel);
            ccs[ccCount][1] = (uint8_t)cc;
            ccs[ccCount][2] = (uint8_t)byte;
            ccCount++;
        }
    }

    p->hwValid    = true;
    p->hwDumpDue |= (changes > HW_CHANGES_FOR_DUMP);

    if (p->hwDumpDue) {
        if (((now_seconds() - p->hwLastDump) >= HW_DUMP_INTERVAL) && (now_seconds() >= p->hwDumpNotBefore)) {
            result        = (tP2HardwareResult)p2_send_to_hardware(user);
            synthlib_log_line("Link: sound dump sent (result %d), sound from %s, file number %d, last program sent %d",
                              (int)result, p->patchPath, number, p->hwProgram);
            p->hwDumpDue  = false;
            p->hwLastDump = now_seconds();
        }
    } else if (result == eP2HardwareSent) {
        for (int i = 0; (i < ccCount) && (result == eP2HardwareSent); i++) {
            result = p2_hardware_send(ccs[i], 3);
        }
    }

    if (result != eP2HardwareSent) {
        synthlib_plugin_param_edited(user, kP2ParamHwLink, 0.0);
        p->hwValid = false;
        p2_draw_hardware_problem((int)result);
    }
}

// notes §46: the instrument's channel is the rig's, not the project's - kept in the prefs, given to each instance
static void hardware_channel_pref(tP2Plugin * p, void * user) {
    double value = synthlib_plugin_param_value(user, kP2ParamHwChannel);

    if (p->hwChannelKnown < 0.0) {
        long channel = prefs_get_int(P2_PREF_HW_CHANNEL, 0);

        if ((channel >= 1) && (channel <= 16) && (value != (channel - 1) / 15.0)) {
            value = (channel - 1) / 15.0;
            synthlib_plugin_param_edited(user, kP2ParamHwChannel, value);
        }
        p->hwChannelKnown = value;
    } else if (value != p->hwChannelKnown) {
        p->hwChannelKnown = value;
        prefs_set_int(P2_PREF_HW_CHANNEL, lround(value * 15.0) + 1);
    }
}

// notes §50: Backup Pulse 2 to Presets - the lock for its whole run, then all 500 stored at once
static int p2_backup_start(void * user) {
    tP2Plugin *       p    = (tP2Plugin *)user;
    bool              held = p->hwLock >= 0;
    tP2HardwareResult result;

    if (p->backingUp) {
        return (int)eP2HardwareBusy;
    }

    if (!p2_hardware_claim(&p->hwLock)) {
        return (int)eP2HardwareBusy;
    }
    result        = p2_hardware_backup_start();

    if (result != eP2HardwareSent) {
        if (!held) {
            p2_hardware_release(&p->hwLock);
        }
        return (int)result;
    }
    p->backingUp  = true;
    p->backupLock = !held;
    synthlib_log_line("backup started");
    return (int)eP2HardwareSent;
}

static void p2_backup_cancel(void * user) {
    if (((tP2Plugin *)user)->backingUp) {
        p2_hardware_backup_cancel();
    }
}

static void backup_follow(tP2Plugin * p) {
    static uint8_t dumps[P2_PATCH_SLOTS][P2_DUMP_LENGTH];
    int            asked, found;

    if (!p->backingUp) {
        return;
    }

    if (!p2_hardware_backup_poll(&asked, &found)) {
        p2_draw_set_backup(true, asked, found);
        return;
    }
    found        = p2_hardware_backup_take(dumps);
    p->backingUp = false;

    if (p->backupLock) {
        p2_hardware_release(&p->hwLock);
    }
    p2_draw_set_backup(false, asked, found);
    synthlib_log_line("backup finished: %d asked, %d sounds", asked, found);
    p2_draw_backup_done(found, (found > 0) && p2_presets_store_all(dumps));
}

static void p2_panel_sync(void * user) {
    tP2Plugin * p      = (tP2Plugin *)user;
    int         number = atomic_load(&p->programShown);

    p2_draw_set_user(user);

    if (atomic_exchange(&p->startPreset, false)) {   // notes §48: no audio block yet - the editor loads it
        p2_load_preset(user, 1);
    }

    // notes §48: another process's store shows within a second
    if ((now_seconds() - p->presetsChecked) >= 1.0) {
        p->presetsChecked = now_seconds();
        (void)p2_presets_refresh();
    }

    // a program change: the panel names it, and a save keeps its dump's bytes
    if ((number != 0) && (number != p->preset)) {
        set_sound_name(p, p2_presets_name(number));
        p2_presets_dump(number, p->baseDump);
        p->haveBase     = true;
        p->patchPath[0] = '\0';
        p->preset       = number;
    }
    p2_draw_set_patch(p->patchPath);      // notes §24: this instance's, every sync
    p2_draw_set_preset(p->preset, preset_edited(p, user));
    hardware_channel_pref(p, user);
    backup_follow(p);
    hardware_follow(p, user);
    p2_draw_set_link_busy(p->hwBusy);

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        p2_draw_set_value(id, synthlib_plugin_param_value(user, id));
    }
}

static void p2_panel_frame(void * user, int pixelWidth, int pixelHeight) {
    (void)user;
    p2_draw_frame(pixelWidth, pixelHeight);
}

static bool p2_panel_press(void * user, double x, double y) {
    tP2Edit edit;
    bool    grabbed = false;

    (void)p2_draw_press(x, y, &edit, &grabbed);

    if (edit.paramId != UINT32_MAX) {
        synthlib_plugin_param_edited(user, edit.paramId, edit.normalized);
    }
    return grabbed;
}

static bool p2_panel_keeps_pointer(void * user) {
    (void)user;
    return p2_dial_mode() == eDialModeRotary;
}

static void p2_panel_drag_to(void * user, double x, double y) {
    tP2Edit edit;

    if (p2_draw_drag_to(x, y, &edit)) {
        synthlib_plugin_param_edited(user, edit.paramId, edit.normalized);
    }
}

static void p2_panel_drag_by(void * user, double dx, double dy) {
    tP2Edit edit;

    if (p2_draw_drag_by(dx, dy, &edit)) {
        synthlib_plugin_param_edited(user, edit.paramId, edit.normalized);
    }
}

static void p2_panel_release(void * user, double x, double y) {
    tP2Edit edit;

    p2_draw_release(x, y, &edit);

    if (edit.paramId != UINT32_MAX) {
        synthlib_plugin_param_edited(user, edit.paramId, edit.normalized);
    }
}

static bool p2_panel_key(void * user, int glfwKey, unsigned int codepoint, bool isRepeat) {
    (void)user;
    return p2_draw_key(glfwKey, codepoint, isRepeat);
}

static void p2_panel_scroll(void * user, double x, double y, double dy) {
    (void)user;
    (void)x;
    (void)y;
    p2_draw_scroll(dy);
}

static const tSynthLibPanel      gPanel = {
    .canvasWidth  = P2_CANVAS_W,
    .init         = p2_panel_init,
    .sync         = p2_panel_sync,
    .frame        = p2_panel_frame,
    .press        = p2_panel_press,
    .dragBy       = p2_panel_drag_by,
    .keepsPointer = p2_panel_keeps_pointer,
    .dragTo       = p2_panel_drag_to,
    .release      = p2_panel_release,
    .scroll       = p2_panel_scroll,
    .key          = p2_panel_key,
    .pointer      = p2_draw_set_mouse,
    .menuActive   = p2_draw_menu_active,
};

static void * p2_create_view(const tSynthLibPluginDesc * desc, void * inst, double width, double height) {
    (void)desc;
    return synthlib_panel_view_create(&gPanel, inst, width, height);
}

#endif // P2_NO_EDITOR

static const tSynthLibBus        gOutputs[1] = {{"Out", 2, false, true}};

static const tSynthLibPluginDesc gDescriptor = {
    .name               = "Pulse2Alike",
    .vendor             = "Chris Purusha",
    .url                = "https://github.com/chrispurusha/Pulse2Alike",
    .email              = "",
    .version            = P2A_VERSION_STRING,

    .isInstrument       = true,
    .vst3SubCategory    = "Instrument|Synth",
    .outputs            = gOutputs,
    .numOutputs         =                                             1,
    .wantsMidiIn        = true,

    .vst3ProcessorUid   = gProcessorUid,
    .vst3ControllerUid  = gControllerUid,
    .auType             = P2A_AU_TYPE,
    .auSubType          = P2A_AU_SUBTYPE,
    .auManufacturer     = P2A_AU_MANUFACTURER,
    .auVersion          = P2A_AU_VERSION,
    .auBundleId         = P2A_AU_BUNDLE_ID,

    .params             = gP2Params,
    .numParams          = kP2ParamCount,
#ifndef P2_NO_EDITOR
    .editorDefaultWidth = P2_CANVAS_W,
    .editorMinWidth     = P2_CANVAS_W * 0.75,
    .editorMaxWidth     = P2_CANVAS_W * 2.0,
    .editorAspect       = P2_CANVAS_W / P2_CANVAS_H,
#endif

    .cb                 = {
        .create     = p2_create,
        .destroy    = p2_destroy,
        .prepare    = p2_prepare,
        .setActive  = p2_set_active,
        .reset      = p2_reset,
        .process    = p2_process,
        .noteOn     = p2_note_on,
        .noteOff    = p2_note_off,
        .setParam   = p2_set_param,
        .getParam   = p2_get_param,
        .getState   = p2_get_state,
        .setState   = p2_set_state,
        .paramText  = p2_param_text,
#ifndef P2_NO_EDITOR
        .createView = p2_create_view,
#endif
    }
};

static const tSynthLibPluginSet  gSet = {
    .variants = &gDescriptor,
    .count    = 1
};

const tSynthLibPluginSet * synthlib_plugin_variants(void) {
    return &gSet;
}
