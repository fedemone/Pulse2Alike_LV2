/*
 * Pulse2Alike - writes the LV2 bundle's Turtle from the engine's own parameter table, and checks the port table
 * against it.
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
// usage: p2Lv2Ttl <bundle directory>   writes manifest.ttl and Pulse2Alike.ttl there (lv2/Makefile: make ttl)

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "synthlibPlugin.h"
#include "p2Lv2Ports.h"

#define SHORT_NAME_MAX    (16)   // notes §5: what MOD keeps of lv2:shortName and mod:label
#define BRAND_MAX         (11)   // and what its schema allows mod:brand

static int gFailures;

static void fail(const char * format, ...) {
    va_list args;

    va_start(args, format);
    fprintf(stderr, "p2Lv2Ttl: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    gFailures++;
}

// The parameters that are deliberately not ports (notes §4): the program change and the performance controls arrive
// as MIDI, and the link to an instrument over CoreMIDI is the desktop plug-ins' alone
static bool deliberately_not_a_port(uint32_t id) {
    const tSynthLibParam * p = &gP2Params[id];

    return ((p->flags & SYNTHLIB_PARAM_HIDDEN) && (p->flags & SYNTHLIB_PARAM_NO_SAVE)) || (id == kP2ParamHwLink) || (id == kP2ParamHwChannel);
}

// ---- where a parameter sits on the panel, which is its port group ---------------------------------------------------

typedef struct {
    const tP2Section * section;
    const tP2Control * control;
    uint32_t           index;     // the section's, counted across the pages
} tPlace;

static bool place_of(uint32_t id, tPlace * out) {
    uint32_t index = 0;

    for (uint32_t pg = 0; pg < gP2PageCount; pg++) {
        for (uint32_t s = 0; s < gP2Pages[pg].count; s++, index++) {
            const tP2Section * section = &gP2Pages[pg].sections[s];

            for (uint32_t c = 0; c < section->count; c++) {
                if (section->controls[c].paramId == id) {
                    *out = (tPlace){
                        section, &section->controls[c], index
                    };
                    return true;
                }
            }
        }
    }

    return false;
}

static uint32_t section_count(void) {
    uint32_t n = 0;

    for (uint32_t pg = 0; pg < gP2PageCount; pg++) {
        n += gP2Pages[pg].count;
    }

    return n;
}

static const tP2Section * section_at(uint32_t index) {
    for (uint32_t pg = 0; pg < gP2PageCount; pg++) {
        if (index < gP2Pages[pg].count) {
            return &gP2Pages[pg].sections[index];
        }
        index -= gP2Pages[pg].count;
    }

    return NULL;
}

// "PLUG-IN" -> "plug_in": a group's lv2:symbol
static void symbol_from_title(const char * title, char * out, size_t len) {
    size_t k = 0;

    for (const char * c = title; (*c != '\0') && (k + 1 < len); c++) {
        out[k++] = isalnum((unsigned char)*c) ? (char)tolower((unsigned char)*c) : '_';
    }

    out[k] = '\0';
}

#define HIDDEN_GROUP_TITLE    "ARP STEPS"   // the ports no panel section shows

// ---- checks -----------------------------------------------------------------------------------------------------

static bool valid_symbol(const char * s) {
    if ((s == NULL) || !(isalpha((unsigned char)s[0]) || (s[0] == '_'))) {
        return false;
    }

    for (const char * c = s; *c != '\0'; c++) {
        if (!isalnum((unsigned char)*c) && (*c != '_')) {
            return false;
        }
    }

    return true;
}

// notes §2: a port's number, at every step, against the text the panel shows there
static void check_against_panel(const tP2Lv2Port * port) {
    const tSynthLibParam * p       = &gP2Params[port->id];
    const char *const *    names   = p2_param_names(port->id);
    tPlace                 place;
    bool                   onPanel = place_of(port->id, &place);
    int                    steps   = (int)lround(p->plainMax - p->plainMin);

    if (port->flags & (P2_LV2_ENUM | P2_LV2_TOGGLE)) {
        if ((names == NULL) || (port->offset != 0.0) || (port->scale != 1.0) || (p->plainMin != 0.0)) {
            fail("%s: a list or a switch needs the parameter's names, from 0, at offset 0 and scale 1", port->symbol);
        }

        if ((port->flags & P2_LV2_TOGGLE) && ((steps != 1) || (strcmp(names[0], "OFF") != 0) || (strcmp(names[1], "ON") != 0))) {
            fail("%s: a switch must be OFF/ON", port->symbol);
        }
        return;
    }

    if (!onPanel) {
        if (!(port->flags & P2_LV2_NOT_SHOWN) || (port->offset != 0.0) || (port->scale != 1.0)) {
            fail("%s: not on the panel, so it must be hidden, at offset 0 and scale 1", port->symbol);
        }
        return;
    }

    for (int step = 0; step <= steps; step++) {
        double value = port->offset + (port->scale * (p->plainMin + step));
        double shown;

        if (names != NULL) {
            char * end;

            shown = strtod(names[step], &end);

            if (end == names[step]) {
                fail("%s: the panel shows \"%s\", which is not a number - make it a list", port->symbol, names[step]);
                return;
            }
        } else {
            shown = place.control->plainMin + step;
        }

        if (value != shown) {
            fail("%s: step %d is %g as a port but %g on the panel", port->symbol, step, value, shown);
            return;
        }
    }
}

static void check_ports(void) {
    for (uint32_t i = 0; i < gP2Lv2PortCount; i++) {
        const tP2Lv2Port * port = &gP2Lv2Ports[i];

        if (port->id >= kP2ParamCount) {
            fail("port %u: no parameter %u", i, port->id);
            continue;
        }

        if ((i > 0) && (port->id <= gP2Lv2Ports[i - 1].id)) {
            fail("%s: the table must be in parameter id order, each once (notes §4)", port->symbol);
        }

        if (deliberately_not_a_port(port->id)) {
            fail("%s: \"%s\" is one of the parameters that are not ports", port->symbol, gP2Params[port->id].title);
        }

        if (  !valid_symbol(port->symbol) || (strcmp(port->symbol, "out_l") == 0) || (strcmp(port->symbol, "out_r") == 0)
           || (strcmp(port->symbol, "events_in") == 0)) {
            fail("%s: not a usable lv2:symbol", port->symbol);
        }

        if ((port->shortName == NULL) || (strlen(port->shortName) == 0) || (strlen(port->shortName) > SHORT_NAME_MAX)) {
            fail("%s: the short name must be 1-%d characters", port->symbol, SHORT_NAME_MAX);
        }

        if ((port->scale == 0.0) || ((port->defaultStep >= 0) && (port->defaultStep > lround(gP2Params[port->id].plainMax - gP2Params[port->id].plainMin)))) {
            fail("%s: a zero scale, or a default past the end", port->symbol);
        }

        for (uint32_t j = 0; j < i; j++) {
            if (strcmp(port->symbol, gP2Lv2Ports[j].symbol) == 0) {
                fail("%s: two ports have this symbol", port->symbol);
            }

            if (strcmp(port->shortName, gP2Lv2Ports[j].shortName) == 0) {
                fail("%s: \"%s\" is %s's short name too", port->symbol, port->shortName, gP2Lv2Ports[j].symbol);
            }
        }

        check_against_panel(port);

        // and the conversions undo each other at every step
        for (int step = 0; p2_lv2_stepped(port) && (step <= gP2Params[port->id].stepCount); step++) {
            double n = (double)step / gP2Params[port->id].stepCount;

            if (fabs(p2_lv2_normalized(port, p2_lv2_value(port, n)) - n) > 1e-12) {
                fail("%s: step %d does not survive the round trip", port->symbol, step);
            }
        }
    }

    for (uint32_t id = 0; id < kP2ParamCount; id++) {
        if ((p2_lv2_port_of(id) == NULL) && !deliberately_not_a_port(id)) {
            fail("\"%s\" (parameter %u) is new: give it a port in lv2/p2Lv2Ports.c, or say why not (notes §4)",
                 gP2Params[id].title, id);
        }
    }
}

// ---- writing --------------------------------------------------------------------------------------------------------

static void put_string(FILE * f, const char * s) {
    fputc('"', f);

    for (const char * c = s; *c != '\0'; c++) {
        if ((*c == '"') || (*c == '\\')) {
            fputc('\\', f);
        }
        fputc(*c, f);
    }

    fputc('"', f);
}

static void put_number(FILE * f, double v, bool integer) {
    if (integer) {
        fprintf(f, "%ld", lround(v));
    } else {
        fprintf(f, "%.1f", v);
    }
}

static void group_uri(FILE * f, const char * title) {
    char symbol[64];

    symbol_from_title(title, symbol, sizeof(symbol));
    fprintf(f, "<" P2_LV2_GROUP_URI "%s>", symbol);
}

static void write_manifest(FILE * f) {
    fprintf(f, "# GENERATED by lv2/p2Lv2Ttl.c from the engine's parameter table - make -C lv2 ttl, do not edit.\n\n"
            "@prefix lv2:  <http://lv2plug.in/ns/lv2core#> .\n"
            "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n\n"
            "<" P2_LV2_URI ">\n"
            "    a lv2:Plugin, lv2:InstrumentPlugin ;\n"
            "    lv2:binary <Pulse2Alike.so> ;\n"
            "    rdfs:seeAlso <Pulse2Alike.ttl> .\n");
}

static void write_groups(FILE * f) {
    uint32_t count = section_count();
    char     symbol[64];

    fprintf(f, "<" P2_LV2_GROUP_URI "out>\n    a pg:StereoGroup, pg:OutputGroup ;\n    lv2:name \"Output\" ;\n"
            "    lv2:symbol \"out\" .\n\n");

    // notes §6: one per panel section, numbered in the panel's order, which is the order MOD shows them in
    for (uint32_t i = 0; i <= count; i++) {
        const char * title = (i < count) ? section_at(i)->title : HIDDEN_GROUP_TITLE;

        symbol_from_title(title, symbol, sizeof(symbol));
        group_uri(f, title);
        fprintf(f, "\n    a pg:InputGroup ;\n    lv2:symbol \"%s\" ;\n    lv2:name ", symbol);
        put_string(f, title);
        fprintf(f, " ;\n    lv2:index %u .\n\n", i);
    }
}

static void write_control(FILE * f, const tP2Lv2Port * port, uint32_t index) {
    const tSynthLibParam * p         = &gP2Params[port->id];
    const char *const *    names     = p2_param_names(port->id);
    bool                   integer   = p2_lv2_stepped(port);
    bool                   list      = (port->flags & (P2_LV2_ENUM | P2_LV2_TOGGLE)) != 0;
    tPlace                 place;
    const char *           props[5];
    int                    propCount = 0;

    fprintf(f, ", [\n        a lv2:InputPort, lv2:ControlPort ;\n        lv2:index %u ;\n        lv2:symbol \"%s\" ;\n"
            "        lv2:name ", index, port->symbol);
    put_string(f, p->title);
    fprintf(f, " ;\n        lv2:shortName ");
    put_string(f, port->shortName);
    fprintf(f, " ;\n        lv2:default ");
    put_number(f, p2_lv2_default(port), integer);
    fprintf(f, " ;\n        lv2:minimum ");
    put_number(f, port->offset + (port->scale * p->plainMin), integer);
    fprintf(f, " ;\n        lv2:maximum ");
    put_number(f, port->offset + (port->scale * p->plainMax), integer);
    fprintf(f, " ;\n");

    if (integer) {
        props[propCount++] = "lv2:integer";
    }

    if (port->flags & P2_LV2_TOGGLE) {
        props[propCount++] = "lv2:toggled";
    } else if (port->flags & P2_LV2_ENUM) {
        props[propCount++] = "lv2:enumeration";
    }

    if (port->flags & P2_LV2_NOT_SHOWN) {
        props[propCount++] = "pprops:notOnGUI";
    }

    if (port->flags & P2_LV2_ARTIFACTS) {
        props[propCount++] = "pprops:causesArtifacts";
    }

    for (int k = 0; k < propCount; k++) {
        fprintf(f, "%s%s", (k == 0) ? "        lv2:portProperty " : ", ", props[k]);
    }

    if (propCount > 0) {
        fprintf(f, " ;\n");
    }

    if (port->unit != NULL) {
        fprintf(f, "        units:unit %s ;\n", port->unit);
    }

    if (list && !(port->flags & P2_LV2_TOGGLE)) {
        int steps = (int)lround(p->plainMax - p->plainMin);

        for (int k = 0; k <= steps; k++) {
            fprintf(f, "%s[ rdfs:label ", (k == 0) ? "        lv2:scalePoint " : ",\n                       ");
            put_string(f, names[k]);
            fprintf(f, " ; rdf:value %d ]", k);
        }

        fprintf(f, " ;\n");
    }

    if (port->comment != NULL) {
        fprintf(f, "        rdfs:comment ");
        put_string(f, port->comment);
        fprintf(f, " ;\n");
    }
    fprintf(f, "        pg:group ");
    group_uri(f, place_of(port->id, &place) ? place.section->title : HIDDEN_GROUP_TITLE);
    fprintf(f, " ;\n    ]");
}

static void write_plugin(FILE * f, const tSynthLibPluginDesc * d) {
    fprintf(f, "# GENERATED by lv2/p2Lv2Ttl.c from the engine's parameter table - make -C lv2 ttl, do not edit.\n\n"
            "@prefix atom:   <http://lv2plug.in/ns/ext/atom#> .\n"
            "@prefix doap:   <http://usefulinc.com/ns/doap#> .\n"
            "@prefix foaf:   <http://xmlns.com/foaf/0.1/> .\n"
            "@prefix lv2:    <http://lv2plug.in/ns/lv2core#> .\n"
            "@prefix midi:   <http://lv2plug.in/ns/ext/midi#> .\n"
            "@prefix mod:    <http://moddevices.com/ns/mod#> .\n"
            "@prefix pg:     <http://lv2plug.in/ns/ext/port-groups#> .\n"
            "@prefix pprops: <http://lv2plug.in/ns/ext/port-props#> .\n"
            "@prefix rdf:    <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n"
            "@prefix rdfs:   <http://www.w3.org/2000/01/rdf-schema#> .\n"
            "@prefix time:   <http://lv2plug.in/ns/ext/time#> .\n"
            "@prefix units:  <http://lv2plug.in/ns/extensions/units#> .\n"
            "@prefix urid:   <http://lv2plug.in/ns/ext/urid#> .\n\n");
    write_groups(f);

    fprintf(f, "<" P2_LV2_URI ">\n    a lv2:Plugin, lv2:InstrumentPlugin, doap:Project ;\n    doap:name ");
    put_string(f, d->name);
    fprintf(f, " ;\n    doap:license <http://spdx.org/licenses/GPL-3.0-or-later> ;\n"
            "    doap:maintainer [\n        foaf:name ");
    put_string(f, d->vendor);
    fprintf(f, " ;\n        foaf:homepage <%s> ;\n    ] ;\n    mod:brand ", d->url);
    put_string(f, P2_LV2_BRAND);
    fprintf(f, " ;\n    mod:label ");
    put_string(f, d->name);
    fprintf(f, " ;\n    lv2:minorVersion %d ;\n    lv2:microVersion %d ;\n", P2_LV2_MINOR_VERSION, P2_LV2_MICRO_VERSION);
    fprintf(f, "    rdfs:comment \"\"\"A reproduction inspired by the Waldorf Pulse 2, modelled from captures of a real "
            "instrument: three DCOs and noise, one resonant filter with four outputs (LP24, LP12, BP12, HP12), drive "
            "and VCA, two envelopes, two LFOs, an eight-slot modulation matrix and an arpeggiator. Monophonic, with "
            "unison and paraphonic modes.\n\n"
            "It plays MIDI notes, and takes pitch bend, the mod wheel, breath, aftertouch and the sustain pedal as "
            "the instrument does; every other control is a port, so map CCs to them with the host's MIDI learn. The "
            "arpeggiator follows the host's tempo while the transport rolls, and its own Arp Tempo otherwise. "
            "Program changes are left to the host: use presets.\"\"\" ;\n");
    fprintf(f, "    lv2:requiredFeature urid:map ;\n    lv2:optionalFeature lv2:hardRTCapable ;\n");

    fprintf(f, "    lv2:port [\n        a lv2:OutputPort, lv2:AudioPort ;\n        lv2:index %d ;\n"
            "        lv2:symbol \"out_l\" ;\n        lv2:name \"Left\" ;\n        lv2:designation pg:left ;\n"
            "        pg:group <" P2_LV2_GROUP_URI "out> ;\n    ], [\n"
            "        a lv2:OutputPort, lv2:AudioPort ;\n        lv2:index %d ;\n        lv2:symbol \"out_r\" ;\n"
            "        lv2:name \"Right\" ;\n        lv2:designation pg:right ;\n        pg:group <" P2_LV2_GROUP_URI "out> ;\n"
            "    ], [\n        a lv2:InputPort, atom:AtomPort ;\n        lv2:index %d ;\n        lv2:symbol \"events_in\" ;\n"
            "        lv2:name \"Events In\" ;\n        atom:bufferType atom:Sequence ;\n"
            "        atom:supports midi:MidiEvent, time:Position ;\n        lv2:designation lv2:control ;\n    ]",
            kP2Lv2PortOutLeft, kP2Lv2PortOutRight, kP2Lv2PortEvents);

    for (uint32_t i = 0; i < gP2Lv2PortCount; i++) {
        write_control(f, &gP2Lv2Ports[i], kP2Lv2FirstControl + i);
    }

    fprintf(f, " .\n");
}

static bool write_file(const char * dir, const char * name, void (*write)(FILE *, const tSynthLibPluginDesc *),
                       const tSynthLibPluginDesc * d) {
    char   path[2048];
    FILE * f;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    f = fopen(path, "w");

    if (f == NULL) {
        fail("cannot write %s", path);
        return false;
    }
    write(f, d);
    return fclose(f) == 0;
}

static void write_manifest_file(FILE * f, const tSynthLibPluginDesc * d) {
    (void)d;
    write_manifest(f);
}

// What the engine links against from a wrapper; the generator never runs the engine.
void synthlib_plugin_param_edited(void * inst, uint32_t id, double normalized) {
    (void)inst;
    (void)id;
    (void)normalized;
}

int main(int argc, char ** argv) {
    const tSynthLibPluginDesc * d = &synthlib_plugin_variants()->variants[0];

    if (argc != 2) {
        fprintf(stderr, "usage: p2Lv2Ttl <bundle directory>\n");
        return 2;
    }

    if ((strlen(P2_LV2_BRAND) > BRAND_MAX) || (strlen(d->name) > SHORT_NAME_MAX)) {
        fail("mod:brand must be at most %d characters and mod:label %d", BRAND_MAX, SHORT_NAME_MAX);
    }
    check_ports();

    if (gFailures > 0) {
        fprintf(stderr, "p2Lv2Ttl: %d problem%s, nothing written\n", gFailures, (gFailures == 1) ? "" : "s");
        return 1;
    }

    if (!write_file(argv[1], "manifest.ttl", write_manifest_file, d) || !write_file(argv[1], "Pulse2Alike.ttl", write_plugin, d)) {
        return 1;
    }
    printf("p2Lv2Ttl: %u control ports, %u groups, written to %s\n", gP2Lv2PortCount, section_count() + 1, argv[1]);
    return 0;
}
