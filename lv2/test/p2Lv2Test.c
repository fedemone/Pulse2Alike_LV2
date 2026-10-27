/*
 * Pulse2Alike - the LV2 plug-in driven as a host drives it, against the same engine driven directly.
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
//
// usage: p2Lv2Test <Pulse2Alike.so>    (make -C lv2 test; make -C lv2 dwarf-check runs it under qemu-aarch64)
//
// notes §14: each case plays the same notes, controllers, port moves and transport into the plug-in, through LV2,
// and into the engine linked into this program from the plug-in's own objects, through its descriptor, the way an
// ideal host would: the block split at every event, the parameters set as the ports say. The two must agree to the
// bit. Then the plug-in's output is checked for sense - finite, both channels the same, sounding where it should.

#include <dlfcn.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include "synthlibPlugin.h"
#include "p2Lv2Fp.h"
#include "p2Lv2Ports.h"

#define RATE          (48000.0)
#define BLOCK         (128)            // a MOD device's
#define SECONDS       (2.5)
#define MAX_EVENTS    (64)
#define SEQ_BYTES     (8192)

void synthlib_plugin_param_edited(void * inst, uint32_t id, double normalized) {
    (void)inst;
    (void)id;
    (void)normalized;
}

// ---- a URID map ---------------------------------------------------------------------------------------------------

static char *   gUris[256];
static uint32_t gUriCount;

static LV2_URID map_uri(LV2_URID_Map_Handle handle, const char * uri) {
    (void)handle;

    for (uint32_t i = 0; i < gUriCount; i++) {
        if (strcmp(gUris[i], uri) == 0) {
            return i + 1;
        }
    }

    gUris[gUriCount] = strdup(uri);
    return ++gUriCount;
}

// ---- what a case plays --------------------------------------------------------------------------------------------

typedef enum {
    eMidi = 0,
    ePosition,       // a time:Position: speed, bpm, bar, barBeat (4/4)
    ePort            // a control port moves, from the start of the block holding `frame`
} tKind;

typedef struct {
    double       seconds;
    tKind        kind;
    uint8_t      midi[3];
    int          size;
    double       speed, bpm, bar, barBeat;
    const char * symbol;
    float        value;
} tEvent;

typedef struct {
    const char * name;
    const char * ports;      // symbol=value ..., in the ports' own units
    tEvent       events[MAX_EVENTS];
    bool         silent;     // expected to make no sound
} tCase;

#define NOTE_ON(t, n, v)                {t, eMidi, {0x90, n, v}, 3, 0, 0, 0, 0, NULL, 0}
#define NOTE_OFF(t, n)                  {t, eMidi, {0x80, n, 0}, 3, 0, 0, 0, 0, NULL, 0}
#define CC(t, c, v)                     {t, eMidi, {0xB0, c, v}, 3, 0, 0, 0, 0, NULL, 0}
#define BEND(t, lsb, msb)               {t, eMidi, {0xE0, lsb, msb}, 3, 0, 0, 0, 0, NULL, 0}
#define PRESSURE(t, v)                  {t, eMidi, {0xD0, v, 0}, 2, 0, 0, 0, 0, NULL, 0}
#define PROGRAM(t, p)                   {t, eMidi, {0xC0, p, 0}, 2, 0, 0, 0, 0, NULL, 0}
#define POSITION(t, s, b, bar, beat)    {t, ePosition, {0}, 0, s, b, bar, beat, NULL, 0}
#define PORT(t, sym, v)                 {t, ePort, {0}, 0, 0, 0, 0, 0, sym, v}
#define CHORD                                                                                                        \
   NOTE_ON(0.0, 48, 100), NOTE_ON(0.3013, 55, 64), NOTE_OFF(0.6007, 48), NOTE_ON(0.65, 60, 127), NOTE_OFF(0.85, 60), \
   NOTE_OFF(1.0, 55), NOTE_ON(1.2341, 67, 40), NOTE_ON(1.2341, 64, 90), NOTE_OFF(1.8, 67), NOTE_OFF(1.8, 64)

static const tCase kCases[] = {
    {"defaults",  "",                                                                                       {CHORD}, false},
    {"pwm-osc2",  "osc1_shape=0 osc1_width=30 osc2_level=100 osc2_shape=1 osc2_semitone=7 osc2_detune=-12", {CHORD}, false},
    {"unison",    "osc1_shape=4 unison_detune=40",                                                          {CHORD}, false},
    {"para-8",    "osc1_shape=8 osc1_fade=60",                                                              {CHORD}, false},
    {"osc3-vcf",  "osc3_level=110 osc3_shape=2 osc3_routing=2 resonance=60.5",                              {CHORD}, false},
    {"fuzz",      "drive_curve=2 drive=64 noise_level=60 hiss=1 osc2_level=127 osc3_level=127",             {CHORD}, false},
    {"lp12-reso", "filter_type=1 resonance=90 cutoff=40.25 filter_env_amount=40 filter_env_decay=50",       {CHORD}, false},
    {"4x",        "oversampling=2 resonance=80",                                                            {CHORD}, false},
    {"1x-ideal",  "oversampling=0 anomalies=0 mod1_source=1 mod1_target=10 mod1_amount=50",                 {CHORD}, false},
    {"fx-silent", "drive_curve=3 drive=64",                                                                 {CHORD}, true },
    // the performance controls arrive as MIDI: bend, the wheel on the cutoff, aftertouch on the pitch, the pedal
    {
        "performance", "mod1_source=12 mod1_target=10 mod1_amount=60 mod2_source=13 mod2_target=0 mod2_amount=20 bend_up=36 bend_down=36",
        {
            NOTE_ON(0.0, 60, 100), BEND(0.1, 0, 96), BEND(0.2, 127, 127), BEND(0.3, 0, 0), BEND(0.4, 0, 64), CC(0.5, 1, 127),
            CC(0.6, 1, 30), PRESSURE(0.7, 100), PRESSURE(0.8, 0), CC(0.9, 64, 127), NOTE_OFF(1.0, 60), NOTE_ON(1.1, 63, 80),
            NOTE_OFF(1.2, 63), CC(1.5, 64, 0), NOTE_ON(1.7, 55, 0), NOTE_ON(1.8, 57, 100), CC(2.0, 123, 0)
        }, false
    },
    // what the engine does not take reaches it not at all: other CCs, program changes, a CC on another channel
    {
        "ignored", "",
        {NOTE_ON(0.0,                                                                                       60,     100), CC(0.1, 7, 0), CC(0.15, 74, 127), PROGRAM(0.2, 5), CC(0.25, 32, 2), NOTE_OFF(1.0, 60)}, false
    },
    // ports moving while it plays, and past their ends
    {
        "port-moves", "cutoff=20",
        {
            NOTE_ON(0.0, 45, 100), PORT(0.1, "cutoff", 50.5), PORT(0.2, "cutoff", 90), PORT(0.3, "cutoff", 300), PORT(0.4, "resonance", -20),
            PORT(0.5, "osc1_shape", 3.6f), PORT(0.6, "filter_type", 2), PORT(0.8, "oversampling", 0), PORT(1.0, "oversampling", 2),
            PORT(1.2, "anomalies", 0), PORT(1.4, "arp_tempo", 47), NOTE_OFF(1.9, 45)
        }, false
    },
    // the arpeggiator on its own clock, then following a host's transport, as mod-host sends it (every block while
    // rolling, at the block's start)
    {
        "arp-own-clock", "arp_active=1 arp_clock=12 arp_mode=2 arp_range=2 arp_tempo=150",
        {NOTE_ON(0.0,                                                                                       48,     100), NOTE_ON(0.01, 52, 100), NOTE_ON(0.02, 55, 100), NOTE_OFF(2.0, 48), NOTE_OFF(2.0, 52), NOTE_OFF(2.0, 55)}, false
    },
    {
        "arp-host", "arp_active=2 arp_clock=15 arp_mode=0",
        {
            POSITION(0.0, 1, 100, 3, 1.5), NOTE_ON(0.0, 48, 100), NOTE_ON(0.0, 55, 100), NOTE_OFF(0.5, 48), NOTE_OFF(0.5, 55),
            POSITION(1.0, 1, 133, 7, 0.25), POSITION(1.6, 0, 133, 9, 2), POSITION(2.0, 1, 90, 0, 0)
        }, false
    },
};

// ---- the plug-in through LV2 --------------------------------------------------------------------------------------

typedef struct {
    LV2_URID midiEvent, position, speed, bpm, bar, barBeat, beatUnit, beatsPerBar, atomFloat, atomLong, atomInt;
} tTestUrids;

static int event_frame(const tEvent * e) {
    return (int)lround(e->seconds * RATE);
}

static void port_value(const char * spec, const char * symbol, float * value) {
    char buf[1024];

    snprintf(buf, sizeof(buf), "%s", spec);

    for (char * tok = strtok(buf, " "); tok != NULL; tok = strtok(NULL, " ")) {
        char * eq = strchr(tok, '=');

        if ((eq != NULL) && ((size_t)(eq - tok) == strlen(symbol)) && (strncmp(tok, symbol, strlen(symbol)) == 0)) {
            *value = (float)atof(eq + 1);
        }
    }
}

static bool known_symbols(const char * spec) {
    char buf[1024];

    snprintf(buf, sizeof(buf), "%s", spec);

    for (char * tok = strtok(buf, " "); tok != NULL; tok = strtok(NULL, " ")) {
        char * eq    = strchr(tok, '=');
        bool   found = false;

        for (uint32_t i = 0; (eq != NULL) && (i < gP2Lv2PortCount); i++) {
            found |= ((size_t)(eq - tok) == strlen(gP2Lv2Ports[i].symbol)) && (strncmp(tok, gP2Lv2Ports[i].symbol, (size_t)(eq - tok)) == 0);
        }

        if (!found) {
            fprintf(stderr, "no port %s\n", tok);
            return false;
        }
    }

    return true;
}

static void forge_position(LV2_Atom_Forge * forge, const tTestUrids * u, const tEvent * e) {
    LV2_Atom_Forge_Frame frame;

    lv2_atom_forge_object(forge, &frame, 0, u->position);
    lv2_atom_forge_key(forge, u->speed);
    lv2_atom_forge_float(forge, (float)e->speed);
    lv2_atom_forge_key(forge, u->bar);
    lv2_atom_forge_long(forge, (int64_t)e->bar);
    lv2_atom_forge_key(forge, u->barBeat);
    lv2_atom_forge_float(forge, (float)e->barBeat);
    lv2_atom_forge_key(forge, u->beatUnit);
    lv2_atom_forge_int(forge, 4);
    lv2_atom_forge_key(forge, u->beatsPerBar);
    lv2_atom_forge_float(forge, 4.0f);
    lv2_atom_forge_key(forge, u->bpm);
    lv2_atom_forge_float(forge, (float)e->bpm);
    lv2_atom_forge_pop(forge, &frame);
}

static void render_lv2(const LV2_Descriptor * d, const tCase * c, float * left, float * right, size_t total) {
    LV2_URID_Map   map        = {NULL, map_uri};
    LV2_Feature    mapFeature = {LV2_URID__map, &map};
    LV2_Feature *  features[] = {&mapFeature, NULL};
    LV2_Handle     h          = d->instantiate(d, RATE, "", (const LV2_Feature *const *)features);
    float          controls[kP2ParamCount];
    uint8_t        seq[SEQ_BYTES] __attribute__((aligned(8)));
    LV2_Atom_Forge forge;
    tTestUrids     u          = {
        map_uri(NULL, LV2_MIDI__MidiEvent),      map_uri(NULL, LV2_TIME__Position),    map_uri(NULL, LV2_TIME__speed),
        map_uri(NULL, LV2_TIME__beatsPerMinute), map_uri(NULL, LV2_TIME__bar),         map_uri(NULL, LV2_TIME__barBeat),
        map_uri(NULL, LV2_TIME__beatUnit),       map_uri(NULL, LV2_TIME__beatsPerBar), map_uri(NULL, LV2_ATOM__Float),
        map_uri(NULL, LV2_ATOM__Long),           map_uri(NULL, LV2_ATOM__Int)
    };

    if (h == NULL) {
        fprintf(stderr, "instantiate failed\n");
        exit(1);
    }
    lv2_atom_forge_init(&forge, &map);

    for (uint32_t i = 0; i < gP2Lv2PortCount; i++) {
        controls[i] = (float)p2_lv2_default(&gP2Lv2Ports[i]);
        port_value(c->ports, gP2Lv2Ports[i].symbol, &controls[i]);
        d->connect_port(h, kP2Lv2FirstControl + i, &controls[i]);
    }

    d->connect_port(h, kP2Lv2PortEvents, seq);
    d->activate(h);

    for (size_t at = 0; at < total; at += BLOCK) {
        uint32_t             frames = (uint32_t)(((total - at) < BLOCK) ? (total - at) : BLOCK);
        LV2_Atom_Forge_Frame frame;

        // a port moves for the whole of the block its event falls in, as a host's control port does
        for (int e = 0; e < MAX_EVENTS; e++) {
            const tEvent * ev = &c->events[e];

            if ((ev->kind == ePort) && (ev->symbol != NULL) && (event_frame(ev) >= (int)at) && (event_frame(ev) < (int)(at + frames))) {
                for (uint32_t i = 0; i < gP2Lv2PortCount; i++) {
                    if (strcmp(gP2Lv2Ports[i].symbol, ev->symbol) == 0) {
                        controls[i] = ev->value;
                    }
                }
            }
        }

        lv2_atom_forge_set_buffer(&forge, seq, sizeof(seq));
        lv2_atom_forge_sequence_head(&forge, &frame, 0);

        for (int e = 0; e < MAX_EVENTS; e++) {
            const tEvent * ev = &c->events[e];
            int            f  = event_frame(ev);

            if ((ev->size == 0) && (ev->kind == eMidi)) {
                continue;
            }

            if ((f < (int)at) || (f >= (int)(at + frames)) || (ev->kind == ePort)) {
                continue;
            }
            lv2_atom_forge_frame_time(&forge, f - (int)at);

            if (ev->kind == eMidi) {
                lv2_atom_forge_atom(&forge, (uint32_t)ev->size, u.midiEvent);
                lv2_atom_forge_write(&forge, ev->midi, (uint32_t)ev->size);
            } else {
                forge_position(&forge, &u, ev);
            }
        }

        lv2_atom_forge_pop(&forge, &frame);
        d->connect_port(h, kP2Lv2PortOutLeft, left + at);
        d->connect_port(h, kP2Lv2PortOutRight, right + at);
        d->run(h, frames);
    }

    d->deactivate(h);
    d->cleanup(h);
}

// ---- the same engine, directly: what an ideal host does with the same events (notes §14) -------------------------

typedef struct {
    bool   speedKnown, tempoKnown, positionKnown;
    double speed, tempo, quarters;
} tRefTransport;

static bool gRefFlushes = true;   // notes §9: the plug-in's floating-point mode, or the engine's by itself

static void ref_render(const tSynthLibPluginDesc * d, void * inst, tRefTransport * t, float * out, uint32_t frames) {
    tSynthLibTransport tr      = {0};
    float              spare[BLOCK];
    float *            outs[2] = {out, spare};
    tP2FpMode          mode    = gRefFlushes ? p2_fp_enter() : 0;

    tr.sampleRate = RATE;

    if (t->speedKnown || t->tempoKnown || t->positionKnown) {
        tr.valid            = true;
        tr.playing          = t->speedKnown && (t->speed != 0.0);
        tr.tempoValid       = t->tempoKnown;
        tr.tempo            = t->tempo;
        tr.musicTimeValid   = t->positionKnown;
        tr.projectTimeMusic = t->quarters;
    }
    d->cb.process(inst, NULL, 0, outs, 2, frames, &tr);

    if (gRefFlushes) {
        p2_fp_leave(mode);
    }

    if (t->speedKnown && (t->speed != 0.0) && t->tempoKnown && t->positionKnown) {
        t->quarters += t->speed * frames * t->tempo / (60.0 * RATE);
    }
}

static void ref_midi(const tSynthLibPluginDesc * d, void * inst, const uint8_t * m) {
    switch (m[0] & 0xF0) {
        case 0x90:

            if (m[2] > 0) {
                d->cb.noteOn(inst, 0, m[1], (float)m[2] / 127.0f, 0);
            } else {
                d->cb.noteOff(inst, 0, m[1], 0.0f, 0);
            }
            break;

        case 0x80: d->cb.noteOff(inst, 0, m[1], (float)m[2] / 127.0f, 0);
            break;

        case 0xB0:

            if ((m[1] == 120) || (m[1] == 123)) {
                d->cb.reset(inst);
            } else if (m[1] == 1) {
                d->cb.setParam(inst, kP2ParamModWheel, m[2] / 127.0);
            } else if (m[1] == 2) {
                d->cb.setParam(inst, kP2ParamBreath, m[2] / 127.0);
            } else if (m[1] == 64) {
                d->cb.setParam(inst, kP2ParamSustain, m[2] / 127.0);
            } else if (m[1] == 32) {
                d->cb.setParam(inst, kP2ParamBank, m[2] / 127.0);
            }
            break;

        case 0xD0: d->cb.setParam(inst, kP2ParamPressure, m[1] / 127.0);
            break;

        case 0xE0: d->cb.setParam(inst, kP2ParamPitchBend, (double)(m[1] | (m[2] << 7)) / 16383.0);
            break;

        default:
            break;
    }
}

static void render_reference(const tCase * c, float * out, size_t total) {
    const tSynthLibPluginDesc * d     = &synthlib_plugin_variants()->variants[0];
    void *                      inst  = d->cb.create(d);
    tSynthLibSetup              setup = {.sampleRate = RATE, .maxFrames = 0, .offline = false};
    float                       controls[kP2ParamCount], applied[kP2ParamCount];
    tRefTransport               t     = {0};
    size_t                      len;
    void *                      state;

    d->cb.prepare(inst, &setup);
    len   = d->cb.getState(inst, NULL, 0);
    state = malloc(len);
    d->cb.getState(inst, state, len);
    d->cb.setState(inst, state, len);
    free(state);
    d->cb.setActive(inst, true);

    for (uint32_t i = 0; i < gP2Lv2PortCount; i++) {
        controls[i] = (float)p2_lv2_default(&gP2Lv2Ports[i]);
        port_value(c->ports, gP2Lv2Ports[i].symbol, &controls[i]);
        applied[i]  = NAN;
    }

    for (size_t at = 0; at < total; at += BLOCK) {
        size_t frames = ((total - at) < BLOCK) ? (total - at) : BLOCK;
        size_t done   = 0;

        for (int e = 0; e < MAX_EVENTS; e++) {
            const tEvent * ev = &c->events[e];

            if ((ev->kind == ePort) && (ev->symbol != NULL) && (event_frame(ev) >= (int)at) && (event_frame(ev) < (int)(at + frames))) {
                for (uint32_t i = 0; i < gP2Lv2PortCount; i++) {
                    if (strcmp(gP2Lv2Ports[i].symbol, ev->symbol) == 0) {
                        controls[i] = ev->value;
                    }
                }
            }
        }

        for (uint32_t i = 0; i < gP2Lv2PortCount; i++) {
            if (!(controls[i] == applied[i])) {
                applied[i] = controls[i];
                d->cb.setParam(inst, gP2Lv2Ports[i].id, p2_lv2_normalized(&gP2Lv2Ports[i], controls[i]));
            }
        }

        // the events in time order (the table lists them so), each splitting the block - except those the
        // engine takes nothing from
        for (int e = 0; e < MAX_EVENTS; e++) {
            const tEvent * ev    = &c->events[e];
            int            f     = event_frame(ev);
            bool           takes = (ev->kind == ePosition)
                                   || (  (ev->kind == eMidi) && (ev->size > 0) && ((ev->midi[0] & 0xF0) != 0xC0)
                                      && !(  ((ev->midi[0] & 0xF0) == 0xB0) && (ev->midi[1] != 1) && (ev->midi[1] != 2)
                                          && (ev->midi[1] != 64) && (ev->midi[1] != 32) && (ev->midi[1] != 120) && (ev->midi[1] != 123)));

            if ((ev->kind == ePort) || (f < (int)at) || (f >= (int)(at + frames)) || !takes) {
                continue;
            }

            if ((size_t)(f - (int)at) > done) {
                ref_render(d, inst, &t, out + at + done, (uint32_t)(f - (int)at - done));
                done = (size_t)(f - (int)at);
            }

            if (ev->kind == eMidi) {
                ref_midi(d, inst, ev->midi);
            } else {
                t.speed         = ev->speed;
                t.speedKnown    = true;
                t.tempo         = ev->bpm;
                t.tempoKnown    = true;
                t.quarters      = (ev->bar * 4.0) + (double)(float)ev->barBeat;
                t.positionKnown = true;
            }
        }

        if (done < frames) {
            ref_render(d, inst, &t, out + at + done, (uint32_t)(frames - done));
        }
    }

    d->cb.destroy(inst);
}

// ---- the checks ---------------------------------------------------------------------------------------------------

int main(int argc, char ** argv) {
    void *                  so;
    LV2_Descriptor_Function get;
    const LV2_Descriptor *  d;
    size_t                  total    = (size_t)(SECONDS * RATE);
    int                     failures = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: p2Lv2Test <Pulse2Alike.so>\n");
        return 2;
    }
    so  = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);

    if (so == NULL) {
        fprintf(stderr, "FAIL: %s\n", dlerror());
        return 1;
    }
    get = (LV2_Descriptor_Function)dlsym(so, "lv2_descriptor");
    d   = (get != NULL) ? get(0) : NULL;

    if ((d == NULL) || (strcmp(d->URI, P2_LV2_URI) != 0) || (get(1) != NULL)) {
        fprintf(stderr, "FAIL: the bundle does not describe one plug-in, %s\n", P2_LV2_URI);
        return 1;
    }

    for (size_t k = 0; k < sizeof(kCases) / sizeof(kCases[0]); k++) {
        const tCase * c     = &kCases[k];
        float *       left  = calloc(total, sizeof(float));
        float *       right = calloc(total, sizeof(float));
        float *       ref   = calloc(total, sizeof(float));
        float *       again = calloc(total, sizeof(float));
        float *       plain = calloc(total, sizeof(float));
        double        diff = 0.0, peak = 0.0, energy = 0.0, flushed = 0.0;
        bool          finite = true, same = true;
        bool          ok;

        if (!known_symbols(c->ports)) {
            return 2;
        }
        // in pairs, one instance of each engine at a time: each instance's oscillators start where a count of the
        // instances made so far puts them (p2Plugin notes §35), and the two engines must count alike
        render_lv2(d, c, left, right, total);
        gRefFlushes = true;
        render_reference(c, ref, total);
        render_lv2(d, c, again, plain, total);
        gRefFlushes = false;
        render_reference(c, plain, total);

        for (size_t i = 0; i < total; i++) {
            finite &= isfinite(left[i]) && isfinite(right[i]);
            same   &= left[i] == right[i];
            diff    = fmax(diff, fabs((double)left[i] - ref[i]));
            flushed = fmax(flushed, fabs((double)again[i] - plain[i]));
            peak    = fmax(peak, fabs(left[i]));
            energy += (double)left[i] * left[i];
        }

        // notes §9: flushing to zero may change only outputs too small for a float to hold in full
        ok          = finite && same && (diff == 0.0) && (flushed < FLT_MIN) && (c->silent ? (peak == 0.0) : (sqrt(energy / total) > 1e-3)) && (peak < 8.0);
        printf("%s %-14s rms %.4f peak %.3f, against the engine %s\n", ok ? "ok  " : "FAIL", c->name, sqrt(energy / total),
               peak, (diff == 0.0) ? "bit-identical" : "DIFFERS");

        if (!ok) {
            printf("     finite %d, channels equal %d, largest difference %g (%g from the engine not flushing)\n", finite, same,
                   diff, flushed);
            failures++;
        }
        free(left);
        free(right);
        free(ref);
        free(again);
        free(plain);
    }

    printf("%s\n", (failures > 0) ? "FAILED" : "all passed");
    dlclose(so);
    return (failures > 0) ? 1 : 0;
}
