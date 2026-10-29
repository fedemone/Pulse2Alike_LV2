/*
 * Pulse2Alike - the plug-in as LV2, for MOD devices and any other LV2 host. Headless: a MOD device draws its own
 * controls from the ports.
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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include "synthlibPlugin.h"
#include "p2Lv2Fp.h"
#include "p2Lv2Ports.h"

#define NO_PARAM    (UINT32_MAX)

// notes §8: KXStudio's control input port change request, which mod-host offers: a plug-in asks the host to move one
// of its control ports (lv2/control-input-port-change-request.h of KXStudio's LV2 extensions, ISC)
#define P2_LV2_PORT_CHANGE_REQUEST_URI    "http://kx.studio/ns/lv2ext/control-input-port-change-request"

typedef struct {
    void * handle;
    int (*request_change)(void * handle, uint32_t index, float value);       // an LV2_ControlInputPort_Change_Status
} tP2Lv2Request;

typedef struct {
    LV2_URID midiEvent;
    LV2_URID object, blank;
    LV2_URID atomInt, atomLong, atomFloat, atomDouble;
    LV2_URID position, speed, frame, bar, barBeat, beatUnit, beatsPerBar, beatsPerMinute;
} tUrids;

// notes §7: the host's transport as the last time:Position left it, carried forward through the frames since
typedef struct {
    bool    speedKnown, tempoKnown, positionKnown;
    double  speed;              // 0 stopped, 1 rolling
    double  tempo;              // quarter notes a minute
    double  quarters;           // where the next frame is, in quarter notes from the start
    double  beatsPerBar, beatUnit;
    int64_t frame;
} tTransport;

typedef struct {
    const tSynthLibPluginDesc * desc;
    void *                      inst;
    double                      rate;
    tUrids                      urid;
    tTransport                  transport;

    float *                     out[2];
    const LV2_Atom_Sequence *   events;
    const float *               control[kP2ParamCount];            // by control port, gP2Lv2Ports' order
    float                       applied[kP2ParamCount];            // what each was when the engine was last given it
    bool                        primed;                            // every port has been given once

    uint32_t                    midiParam[SYNTHLIB_MIDI_CONTROLS]; // notes §8: a MIDI control's parameter
    uint32_t                    ccPort[128];                       // notes §8: a CC's port, NO_PARAM if none
    bool                        held[kP2ParamCount];               // a CC moved the port's parameter, the port not yet
    float                       heldValue[kP2ParamCount];          // what the port was when the CC came
    const tP2Lv2Request *       portChange;                        // the host's, or NULL
} tP2Lv2;

// ---- what the engine links against from a wrapper -----------------------------------------------------------------

// notes §10: only a program change moves a parameter from inside the engine, and LV2 delivers none - the ports are
// the host's, and there is nothing to tell it
void synthlib_plugin_param_edited(void * inst, uint32_t id, double normalized) {
    (void)inst;
    (void)id;
    (void)normalized;
}

// ---- the host's transport ---------------------------------------------------------------------------------------

static bool atom_number(const tUrids * u, const LV2_Atom * a, double * out) {
    if (a == NULL) {
        return false;
    }

    if (a->type == u->atomFloat) {
        *out = ((const LV2_Atom_Float *)a)->body;
    } else if (a->type == u->atomDouble) {
        *out = ((const LV2_Atom_Double *)a)->body;
    } else if (a->type == u->atomInt) {
        *out = ((const LV2_Atom_Int *)a)->body;
    } else if (a->type == u->atomLong) {
        *out = (double)((const LV2_Atom_Long *)a)->body;
    } else {
        return false;
    }
    return true;
}

// notes §7: what a time:Position says, in the engine's terms - quarter notes, whatever note the beat is
static void take_position(tP2Lv2 * w, const LV2_Atom_Object * obj) {
    const tUrids *   u = &w->urid;
    tTransport *     t = &w->transport;
    const LV2_Atom * speed = NULL, * frame = NULL, * bar = NULL, * barBeat = NULL, * beatUnit = NULL;
    const LV2_Atom * beatsPerBar = NULL, * bpm = NULL;
    double           v, barNumber, beat;

    lv2_atom_object_get(obj, u->speed, &speed, u->frame, &frame, u->bar, &bar, u->barBeat, &barBeat, u->beatUnit, &beatUnit,
                        u->beatsPerBar, &beatsPerBar, u->beatsPerMinute, &bpm, 0);

    if (atom_number(u, speed, &v)) {
        t->speed      = v;
        t->speedKnown = true;
    }

    if (atom_number(u, frame, &v)) {
        t->frame = (int64_t)v;
    }

    if (atom_number(u, beatUnit, &v) && (v > 0.0)) {
        t->beatUnit = v;
    }

    if (atom_number(u, beatsPerBar, &v) && (v > 0.0)) {
        t->beatsPerBar = v;
    }

    if (atom_number(u, bpm, &v) && (v > 0.0)) {
        t->tempo      = v * 4.0 / t->beatUnit;
        t->tempoKnown = true;
    }

    if (atom_number(u, bar, &barNumber) && atom_number(u, barBeat, &beat)) {
        t->quarters      = ((barNumber * t->beatsPerBar) + beat) * 4.0 / t->beatUnit;
        t->positionKnown = true;
    }
}

static void fill_transport(const tP2Lv2 * w, tSynthLibTransport * out) {
    const tTransport * t = &w->transport;

    memset(out, 0, sizeof(*out));
    out->sampleRate         = w->rate;

    if (!t->speedKnown && !t->tempoKnown && !t->positionKnown) {
        return;   // valid false: the host has said nothing, so the engine keeps its own clock
    }
    out->valid              = true;
    out->playing            = t->speedKnown && (t->speed != 0.0);
    out->tempoValid         = t->tempoKnown;
    out->tempo              = t->tempo;
    out->musicTimeValid     = t->positionKnown;
    out->projectTimeMusic   = t->quarters;
    out->timeSigValid       = true;
    out->timeSigNumerator   = (int32_t)t->beatsPerBar;
    out->timeSigDenominator = (int32_t)t->beatUnit;
    out->projectTimeSamples = t->frame;
}

static void advance_transport(tP2Lv2 * w, uint32_t frames) {
    tTransport * t = &w->transport;

    if (!t->speedKnown || (t->speed == 0.0)) {
        return;
    }
    t->frame += (int64_t)(t->speed * frames);

    if (t->tempoKnown && t->positionKnown) {
        t->quarters += t->speed * frames * t->tempo / (60.0 * w->rate);
    }
}

// ---- MIDI ---------------------------------------------------------------------------------------------------------

// notes §8: an instrument CC moves its parameter now, and asks the host to move the port to match, so a MOD device's
// web interface and saved pedalboard follow. Until the port moves, its old value is not given to the engine again: a
// host that moves it later, or not at all, does not undo the CC.
static void take_cc(tP2Lv2 * w, uint32_t i, uint8_t byte) {
    const tP2Lv2Port * port  = &gP2Lv2Ports[i];
    float              value = (float)p2_lv2_byte_value(port, byte);

    w->desc->cb.setParam(w->inst, port->id, p2_lv2_normalized(port, value));

    if (!w->held[i] && (w->control[i] != NULL)) {
        w->held[i]      = true;
        w->heldValue[i] = *w->control[i];
    }
    w->applied[i] = value;

    if (w->portChange != NULL) {
        (void)w->portChange->request_change(w->portChange->handle, kP2Lv2FirstControl + i, value);
    }
}

// notes §8: whether the message is one for the engine - and, with deliver, gives it the engine. Asked first without,
// so the block is split only at a message that reaches it.
static bool take_midi(tP2Lv2 * w, const uint8_t * m, uint32_t size, bool deliver) {
    const tSynthLibPluginCallbacks * cb      = &w->desc->cb;
    uint8_t                          channel = m[0] & 0x0Fu;
    uint32_t                         id      = NO_PARAM;
    double                           value   = 0.0;

    // the event lands on the first frame of the next part of the block, so it needs no offset of its own
    switch (m[0] & 0xF0u) {
        case LV2_MIDI_MSG_NOTE_ON:

            if (size < 3) {
                return false;
            }

            if (!deliver) {
                return true;
            }

            if (m[2] > 0) {    // velocity 0 is a note off, as it is on the wire
                cb->noteOn(w->inst, channel, m[1] & 0x7Fu, (float)(m[2] & 0x7Fu) / 127.0f, 0);
            } else {
                cb->noteOff(w->inst, channel, m[1] & 0x7Fu, 0.0f, 0);
            }
            return true;

        case LV2_MIDI_MSG_NOTE_OFF:

            if (size < 3) {
                return false;
            }

            if (deliver) {
                cb->noteOff(w->inst, channel, m[1] & 0x7Fu, (float)(m[2] & 0x7Fu) / 127.0f, 0);
            }
            return true;

        case LV2_MIDI_MSG_NOTE_PRESSURE:

            if ((size < 3) || (cb->polyPressure == NULL)) {
                return false;
            }

            if (deliver) {
                cb->polyPressure(w->inst, channel, m[1] & 0x7Fu, (float)(m[2] & 0x7Fu) / 127.0f, 0);
            }
            return true;

        case LV2_MIDI_MSG_CONTROLLER:

            if (size < 3) {
                return false;
            }

            // ALL SOUND OFF and ALL NOTES OFF, which a host sends on a transport stop
            if (((m[1] == LV2_MIDI_CTL_ALL_SOUNDS_OFF) || (m[1] == LV2_MIDI_CTL_ALL_NOTES_OFF)) && (cb->reset != NULL)) {
                if (deliver) {
                    cb->reset(w->inst);
                }
                return true;
            }

            if (w->ccPort[m[1] & 0x7Fu] != NO_PARAM) {
                if (deliver) {
                    take_cc(w, w->ccPort[m[1] & 0x7Fu], m[2] & 0x7Fu);
                }
                return true;
            }
            id    = w->midiParam[m[1] & 0x7Fu];
            value = (double)(m[2] & 0x7Fu) / 127.0;
            break;

        case LV2_MIDI_MSG_CHANNEL_PRESSURE:

            if (size < 2) {
                return false;
            }
            id    = w->midiParam[SYNTHLIB_MIDI_AFTERTOUCH];
            value = (double)(m[1] & 0x7Fu) / 127.0;
            break;

        case LV2_MIDI_MSG_BENDER:

            if (size < 3) {
                return false;
            }
            id    = w->midiParam[SYNTHLIB_MIDI_PITCH_BEND];
            value = (double)((uint32_t)(m[1] & 0x7Fu) | ((uint32_t)(m[2] & 0x7Fu) << 7)) / 16383.0;   // as the AU's
            break;

        default:           // notes §8: program change is the host's; clock and the rest are not the engine's
            return false;
    }

    if (id == NO_PARAM) {
        return false;
    }

    if (deliver) {
        cb->setParam(w->inst, id, value);
    }
    return true;
}

// ---- the ports --------------------------------------------------------------------------------------------------

// notes §2: a port the host moved goes to the engine, and only one it moved - setParam is not free
static void take_controls(tP2Lv2 * w) {
    for (uint32_t i = 0; i < gP2Lv2PortCount; i++) {
        float value;

        if (w->control[i] == NULL) {
            continue;      // not connected: the engine keeps what it has
        }
        value         = *w->control[i];

        // notes §8: after a CC, the port's old value is the CC's to replace, not the engine's to take back
        if (w->held[i]) {
            if (value == w->heldValue[i]) {
                continue;
            }
            w->held[i] = false;
        }

        if (w->primed && (value == w->applied[i])) {
            continue;
        }
        w->applied[i] = value;
        w->desc->cb.setParam(w->inst, gP2Lv2Ports[i].id, p2_lv2_normalized(&gP2Lv2Ports[i], value));
    }

    w->primed = true;
}

static void render(tP2Lv2 * w, uint32_t start, uint32_t frames) {
    tSynthLibTransport transport;
    float *            out[2] = {(w->out[0] != NULL) ? (w->out[0] + start) : NULL, (w->out[1] != NULL) ? (w->out[1] + start) : NULL};

    fill_transport(w, &transport);
    w->desc->cb.process(w->inst, NULL, 0, out, 2, frames, &transport);
    advance_transport(w, frames);
}

// ---- LV2 ----------------------------------------------------------------------------------------------------------

static LV2_Handle instantiate(const LV2_Descriptor * descriptor, double rate, const char * bundlePath,
                              const LV2_Feature *const * features) {
    const LV2_URID_Map * map = NULL;
    tP2Lv2 *             w;
    tSynthLibSetup       setup;

    (void)descriptor;
    (void)bundlePath;

    for (int i = 0; (features != NULL) && (features[i] != NULL); i++) {
        if (strcmp(features[i]->URI, LV2_URID__map) == 0) {
            map = (const LV2_URID_Map *)features[i]->data;
        }
    }

    if ((map == NULL) || (gP2Lv2PortCount > kP2ParamCount)) {
        return NULL;
    }
    w                        = (tP2Lv2 *)calloc(1, sizeof(tP2Lv2));

    if (w == NULL) {
        return NULL;
    }
    w->desc                  = &synthlib_plugin_variants()->variants[0];
    w->rate                  = rate;
    w->inst                  = w->desc->cb.create(w->desc);

    if (w->inst == NULL) {
        free(w);
        return NULL;
    }

    if (w->desc->cb.setSampleRate != NULL) {
        w->desc->cb.setSampleRate(w->inst, rate);
    }
    // notes §11: LV2 promises no block size without the options feature, and the engine does not need one
    setup                    = (tSynthLibSetup){
        .sampleRate = rate, .maxFrames = 0, .offline = false
    };

    if (w->desc->cb.prepare != NULL) {
        w->desc->cb.prepare(w->inst, &setup);
    }

    // notes §3: the instance's own fresh state, restored, which tells it its sound is the host's - not P001
    if ((w->desc->cb.getState != NULL) && (w->desc->cb.setState != NULL)) {
        size_t len   = w->desc->cb.getState(w->inst, NULL, 0);
        void * state = (len > 0) ? malloc(len) : NULL;

        if (state != NULL) {
            len = w->desc->cb.getState(w->inst, state, len);
            w->desc->cb.setState(w->inst, state, len);
            free(state);
        }
    }
    w->urid                  = (tUrids){
        .midiEvent      = map->map(map->handle, LV2_MIDI__MidiEvent),
        .object         = map->map(map->handle, LV2_ATOM__Object),
        .blank          = map->map(map->handle, LV2_ATOM__Blank),
        .atomInt        = map->map(map->handle, LV2_ATOM__Int),
        .atomLong       = map->map(map->handle, LV2_ATOM__Long),
        .atomFloat      = map->map(map->handle, LV2_ATOM__Float),
        .atomDouble     = map->map(map->handle, LV2_ATOM__Double),
        .position       = map->map(map->handle, LV2_TIME__Position),
        .speed          = map->map(map->handle, LV2_TIME__speed),
        .frame          = map->map(map->handle, LV2_TIME__frame),
        .bar            = map->map(map->handle, LV2_TIME__bar),
        .barBeat        = map->map(map->handle, LV2_TIME__barBeat),
        .beatUnit       = map->map(map->handle, LV2_TIME__beatUnit),
        .beatsPerBar    = map->map(map->handle, LV2_TIME__beatsPerBar),
        .beatsPerMinute = map->map(map->handle, LV2_TIME__beatsPerMinute),
    };
    w->transport.beatsPerBar = 4.0;
    w->transport.beatUnit    = 4.0;

    // notes §8: the MIDI controls the engine takes, onto the parameters that are not ports
    for (uint32_t c = 0; c < SYNTHLIB_MIDI_CONTROLS; c++) {
        w->midiParam[c] = NO_PARAM;
    }

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        int16_t control = gP2Params[id].midiControl;

        if ((control >= 0) && (control < SYNTHLIB_MIDI_CONTROLS) && (p2_lv2_port_of(id) == NULL)) {
            w->midiParam[control] = id;
        }
    }

    // notes §8: the instrument's CCs, onto the ports of the parameters they move
    for (uint32_t c = 0; c < 128; c++) {
        w->ccPort[c] = NO_PARAM;
    }

    for (uint32_t k = 0; k < gP2Lv2CcCount; k++) {
        const tP2Lv2Port * port = p2_lv2_port_of(gP2Lv2Ccs[k].id);

        if ((port != NULL) && (gP2Lv2Ccs[k].cc < 128)) {
            w->ccPort[gP2Lv2Ccs[k].cc] = (uint32_t)(port - gP2Lv2Ports);
        }
    }

    for (int i = 0; (features != NULL) && (features[i] != NULL); i++) {
        if (strcmp(features[i]->URI, P2_LV2_PORT_CHANGE_REQUEST_URI) == 0) {
            w->portChange = (const tP2Lv2Request *)features[i]->data;
        }
    }

    return (LV2_Handle)w;
}

static void connect_port(LV2_Handle handle, uint32_t port, void * data) {
    tP2Lv2 * w = (tP2Lv2 *)handle;

    if ((port == kP2Lv2PortOutLeft) || (port == kP2Lv2PortOutRight)) {
        w->out[port - kP2Lv2PortOutLeft] = (float *)data;
    } else if (port == kP2Lv2PortEvents) {
        w->events = (const LV2_Atom_Sequence *)data;
    } else if ((port >= kP2Lv2FirstControl) && (port - kP2Lv2FirstControl < gP2Lv2PortCount)) {
        w->control[port - kP2Lv2FirstControl] = (const float *)data;
    }
}

static void activate(LV2_Handle handle) {
    tP2Lv2 * w = (tP2Lv2 *)handle;

    if (w->desc->cb.setActive != NULL) {
        w->desc->cb.setActive(w->inst, true);
    }
}

// notes §12: each event splits the block, so it is heard on its own frame, as the engine cannot place one inside
static void run(LV2_Handle handle, uint32_t frames) {
    tP2Lv2 *           w    = (tP2Lv2 *)handle;
    tP2FpMode          mode = p2_fp_enter();   // notes §9
    uint32_t           done = 0;
    tSynthLibTransport transport;

    if (w->desc->cb.blockBegin != NULL) {
        fill_transport(w, &transport);
        w->desc->cb.blockBegin(w->inst, frames, &transport);
    }
    take_controls(w);

    if (w->events != NULL) {
        LV2_ATOM_SEQUENCE_FOREACH(w->events, ev) {
            uint32_t at     = (ev->time.frames <= 0) ? 0u : ((ev->time.frames >= frames) ? frames : (uint32_t)ev->time.frames);
            bool     object = (ev->body.type == w->urid.object) || (ev->body.type == w->urid.blank);

            at = (at < done) ? done : at;

            if (ev->body.type == w->urid.midiEvent) {
                const uint8_t * m = (const uint8_t *)LV2_ATOM_BODY_CONST(&ev->body);

                // running status and system messages are nothing to the engine
                if ((ev->body.size == 0) || (m[0] < 0x80u) || (m[0] >= 0xF0u) || !take_midi(w, m, ev->body.size, false)) {
                    continue;
                }

                if (at > done) {
                    render(w, done, at - done);
                    done = at;
                }
                (void)take_midi(w, m, ev->body.size, true);
            } else if (object && (((const LV2_Atom_Object *)&ev->body)->body.otype == w->urid.position)) {
                if (at > done) {
                    render(w, done, at - done);
                    done = at;
                }
                take_position(w, (const LV2_Atom_Object *)&ev->body);
            }
        }
    }

    if (done < frames) {
        render(w, done, frames - done);
    }
    p2_fp_leave(mode);
}

static void deactivate(LV2_Handle handle) {
    tP2Lv2 * w = (tP2Lv2 *)handle;

    if (w->desc->cb.setActive != NULL) {
        w->desc->cb.setActive(w->inst, false);
    }
}

static void cleanup(LV2_Handle handle) {
    tP2Lv2 * w = (tP2Lv2 *)handle;

    w->desc->cb.destroy(w->inst);
    free(w);
}

static const void * extension_data(const char * uri) {
    (void)uri;
    return NULL;
}

static const LV2_Descriptor gLv2Descriptor = {
    P2_LV2_URI, instantiate, connect_port, activate, run, deactivate, cleanup, extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor * lv2_descriptor(uint32_t index) {
    return (index == 0) ? &gLv2Descriptor : NULL;
}
