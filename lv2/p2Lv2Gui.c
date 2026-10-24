/*
 * Pulse2Alike - the LV2 plug-in's panel on a MOD device (its modgui): the instrument's matrix of rows and six knobs.
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
#include <string.h>

#include "p2Lv2Gui.h"
#include "p2Lv2Ports.h"

#define NONE             (UINT32_MAX)
#define KNOBS            (6)
#define ROWS             (6)
#define BUTTON_MOD       (6)
#define BUTTON_GLOBAL    (7)
#define DEFAULT_PAGE     "vcf"

// notes §16: the panel's geometry, in pixels - the matrix's six columns over the six knobs
#define MATRIX_X         (358)
#define MATRIX_Y         (30)
#define COLUMN_W         (64)
#define LINE_H           (13)
#define ROW_PAD          (12)
#define KNOB_SIZE        (36)
#define KNOB_GAP         (10)
#define BUTTON_X         (262)
#define LED_X            (298)
#define SHIFT_X          (216)
#define LCD_X            (16)
#define LCD_Y            (30)
#define LCD_COLUMNS      (2)
#define LCD_CELL_W       (94)
#define LCD_CELL_H       (32)
#define LCD_TITLE_H      (17)

typedef struct {
    const char * label;    // the matrix's text for it, as the instrument prints it
    const char * tag;      // a box before the label in the matrix (LFO 2, GLIDE), or NULL
    uint32_t     id;       // the parameter, NONE for an empty place
} tSlot;

typedef struct {
    const char * id;       // the page's name in the template and the script
    const char * title;    // what the LCD heads it with
    const char * led;      // the label beside its LED in the row
    int          button;   // the row button that selects it, 0..5, or BUTTON_MOD / BUTTON_GLOBAL
    bool         matrix;   // printed on the matrix
    tSlot        slot[KNOBS];
} tPage;

#define S(label, id)     {label, NULL, id}
#define EMPTY    {"", NULL, NONE}
#define KNOB_ROW(...)    {__VA_ARGS__}

// notes §16: the instrument's matrix, row by row, then what its Mod and Global buttons lead to - every control the
// plug-in shows, once
static const tPage kPages[] = {
    {
        "osc1", "OSC 1", "OSC 1", 0, true, KNOB_ROW(S("Shape", kP2ParamOsc1Shape), S("Pulsewidth", kP2ParamOsc1Width),
                                                    S("Semitone", kP2ParamOsc1Semitone), S("Detune", kP2ParamOsc1Detune),
                                                    S("Keytrack", kP2ParamOsc1Keytrack), S("Level", kP2ParamOsc1Level))
    },
    {
        "osc2", "OSC 2", "OSC 2", 0, true, KNOB_ROW(S("Shape", kP2ParamOsc2Shape), S("Pulsewidth", kP2ParamOsc2Width),
                                                    S("Semitone", kP2ParamOsc2Semitone), S("Detune", kP2ParamOsc2Detune),
                                                    S("Keytrack", kP2ParamOsc2Keytrack), S("Level", kP2ParamOsc2Level))
    },
    {
        "osc3", "OSC 3", "OSC 3", 0, true, KNOB_ROW(S("Shape", kP2ParamOsc3Shape), S("Routing", kP2ParamOsc3Routing),
                                                    S("Semitone", kP2ParamOsc3Semitone), S("Detune", kP2ParamOsc3Detune),
                                                    S("Sync", kP2ParamOsc3Sync), S("Level", kP2ParamOsc3Level))
    },
    {
        "lfo", "LFO 1 - LFO 2 - GLIDE", "LFO 1", 1, true, KNOB_ROW(S("Speed", kP2ParamLfo1Speed), S("Shape", kP2ParamLfo1Shape),
                                                                   {"Speed", "LFO 2", kP2ParamLfo2Speed}, S("Delay", kP2ParamLfo2Delay),
                                                                   {"Rate",  "GLIDE", kP2ParamGlideRate}, S("Mode",  kP2ParamGlideMode))
    },
    {
        "envf", "FILTER ENVELOPE", "ENV F", 2, true, KNOB_ROW(S("Attack", kP2ParamEnvFAttack), S("Decay", kP2ParamEnvFDecay),
                                                              S("Sustain", kP2ParamEnvFSustain), S("Release", kP2ParamEnvFRelease),
                                                              S("Loop", kP2ParamEnvFLoop), S("Trigger", kP2ParamEnvFTrigger))
    },
    {
        "enva", "AMP ENVELOPE", "ENV A", 2, true, KNOB_ROW(S("Attack", kP2ParamEnvAAttack), S("Decay", kP2ParamEnvADecay),
                                                           S("Sustain", kP2ParamEnvASustain), S("Release", kP2ParamEnvARelease),
                                                           S("Loop", kP2ParamEnvALoop), S("Trigger", kP2ParamEnvATrigger))
    },
    {
        "arp", "ARPEGGIATOR", "ARP", 3, true, KNOB_ROW(S("Active", kP2ParamArpActive), S("Range", kP2ParamArpRange),
                                                       S("Tempo", kP2ParamArpTempo), S("Clock", kP2ParamArpClock),
                                                       S("Pattern", kP2ParamArpPattern), S("Mode", kP2ParamArpMode))
    },
    {
        "arptime", "ARP TIMING", "TIME", 3, false, KNOB_ROW(S("Step", kP2ParamArpStep), S("Swing", kP2ParamArpSwing),
                                                            S("Delay", kP2ParamArpDelay), S("Length", kP2ParamArpLength),
                                                            S("Accent", kP2ParamArpAccent), EMPTY)
    },
    {
        "vcf", "VCF", "VCF", 4, true, KNOB_ROW(S("Cutoff", kP2ParamCutoff), S("Resonance", kP2ParamResonance),
                                               S("ENV F Amount", kP2ParamFilterEnvAmount), S("Type", kP2ParamFilterType),
                                               S("Keytrack", kP2ParamFilterKeytrack), S("Velocity", kP2ParamFilterVelocity))
    },
    {
        "vca", "VCA", "VCA", 5, true, KNOB_ROW(S("Drive", kP2ParamDrive), S("Drive Curve", kP2ParamDriveCurve),
                                               S("Panning", kP2ParamPan), S("Noise", kP2ParamNoiseLevel),
                                               S("Volume", kP2ParamVolume), S("Velocity", kP2ParamAmpVelocity))
    },
    {
        "mod12", "MOD 1 - MOD 2", NULL, BUTTON_MOD, false, KNOB_ROW(S("Source", kP2ParamMod1Source), S("Amount", kP2ParamMod1Amount),
                                                                    S("Target", kP2ParamMod1Target), S("Source", kP2ParamMod2Source),
                                                                    S("Amount", kP2ParamMod2Amount), S("Target", kP2ParamMod2Target))
    },
    {
        "mod34", "MOD 3 - MOD 4", NULL, BUTTON_MOD, false, KNOB_ROW(S("Source", kP2ParamMod3Source), S("Amount", kP2ParamMod3Amount),
                                                                    S("Target", kP2ParamMod3Target), S("Source", kP2ParamMod4Source),
                                                                    S("Amount", kP2ParamMod4Amount), S("Target", kP2ParamMod4Target))
    },
    {
        "mod56", "MOD 5 - MOD 6", NULL, BUTTON_MOD, false, KNOB_ROW(S("Source", kP2ParamMod5Source), S("Amount", kP2ParamMod5Amount),
                                                                    S("Target", kP2ParamMod5Target), S("Source", kP2ParamMod6Source),
                                                                    S("Amount", kP2ParamMod6Amount), S("Target", kP2ParamMod6Target))
    },
    {
        "mod78", "MOD 7 - MOD 8", NULL, BUTTON_MOD, false, KNOB_ROW(S("Source", kP2ParamMod7Source), S("Amount", kP2ParamMod7Amount),
                                                                    S("Target", kP2ParamMod7Target), S("Source", kP2ParamMod8Source),
                                                                    S("Amount", kP2ParamMod8Amount), S("Target", kP2ParamMod8Target))
    },
    {
        "voice", "VOICE", NULL, BUTTON_GLOBAL, false, KNOB_ROW(S("Unison", kP2ParamUnisonDetune), S("Fade 1", kP2ParamOsc1Fade),
                                                               S("Fade 2", kP2ParamOsc2Fade), S("LFO 1 Phase", kP2ParamLfo1Phase),
                                                               S("LFO 2 Phase", kP2ParamLfo2Phase), EMPTY)
    },
    {
        "global", "GLOBAL", NULL, BUTTON_GLOBAL, false, KNOB_ROW(S("Bend Up", kP2ParamBendUp), S("Bend Down", kP2ParamBendDown),
                                                                 S("Oversampling", kP2ParamOversample), S("Anomalies", kP2ParamAnomalies),
                                                                 S("Hiss", kP2ParamHiss), EMPTY)
    },
};

#define PAGES        ((int)(sizeof(kPages) / sizeof(kPages[0])))

// The one control beside the pages, where the instrument has its Volume knob
#define MASTER_ID    (kP2ParamMaster)

// ---- checks -------------------------------------------------------------------------------------------------------

int p2_lv2_gui_check(void) {
    int  failures     = 0;
    int  seen[kP2ParamCount];
    bool defaultFound = false;

    memset(seen, 0, sizeof(seen));
    seen[MASTER_ID]++;

    if (p2_lv2_port_of(MASTER_ID) == NULL) {
        fprintf(stderr, "p2Lv2Ttl: the panel's own Master knob is not a port\n");
        failures++;
    }

    for (int p = 0; p < PAGES; p++) {
        defaultFound |= strcmp(kPages[p].id, DEFAULT_PAGE) == 0;

        if ((kPages[p].button < ROWS) != (kPages[p].led != NULL)) {
            fprintf(stderr, "p2Lv2Ttl: page %s: a matrix row's page needs its LED's label, and only those\n", kPages[p].id);
            failures++;
        }

        for (int k = 0; k < KNOBS; k++) {
            uint32_t id = kPages[p].slot[k].id;

            if (id == NONE) {
                continue;
            }

            if ((id >= kP2ParamCount) || (p2_lv2_port_of(id) == NULL) || (p2_lv2_port_of(id)->flags & P2_LV2_NOT_SHOWN)) {
                fprintf(stderr, "p2Lv2Ttl: page %s, knob %d: not a port the panel can show\n", kPages[p].id, k + 1);
                failures++;
                continue;
            }
            seen[id]++;
        }
    }

    if (!defaultFound) {
        fprintf(stderr, "p2Lv2Ttl: no page " DEFAULT_PAGE "\n");
        failures++;
    }

    for (uint32_t i = 0; i < gP2Lv2PortCount; i++) {
        const tP2Lv2Port * port  = &gP2Lv2Ports[i];
        int                count = seen[port->id];

        if (!(port->flags & P2_LV2_NOT_SHOWN) && (count != 1)) {
            fprintf(stderr, "p2Lv2Ttl: %s is on the panel %d times - every control shown goes on it once (notes §16)\n",
                    port->symbol, count);
            failures++;
        }
    }

    return failures;
}

// ---- modgui.ttl ---------------------------------------------------------------------------------------------------

static void put_html(FILE * f, const char * s) {
    for (const char * c = s; *c != '\0'; c++) {
        switch (*c) {
            case '&':  fputs("&amp;", f);
                break;

            case '<':  fputs("&lt;", f);
                break;

            case '>':  fputs("&gt;", f);
                break;

            case '"':  fputs("&quot;", f);
                break;

            default:   fputc(*c, f);
                break;
        }
    }
}

static const tPage * page_named(const char * id) {
    for (int p = 0; p < PAGES; p++) {
        if (strcmp(kPages[p].id, id) == 0) {
            return &kPages[p];
        }
    }

    return NULL;
}

void p2_lv2_gui_write_ttl(FILE * f) {
    const tPage * first = page_named(DEFAULT_PAGE);

    fprintf(f, "# GENERATED by lv2/p2Lv2Ttl.c - make -C lv2 ttl, do not edit.\n\n"
            "@prefix lv2:    <http://lv2plug.in/ns/lv2core#> .\n"
            "@prefix modgui: <http://moddevices.com/ns/modgui#> .\n\n"
            "<" P2_LV2_URI ">\n"
            "    modgui:gui [\n"
            "        modgui:resourcesDirectory <modgui> ;\n"
            "        modgui:iconTemplate <modgui/icon-pulse2alike.html> ;\n"
            "        modgui:stylesheet <modgui/stylesheet-pulse2alike.css> ;\n"
            "        modgui:javascript <modgui/script-pulse2alike.js> ;\n"
            "        modgui:screenshot <modgui/screenshot-pulse2alike.png> ;\n"
            "        modgui:thumbnail <modgui/thumbnail-pulse2alike.png> ;\n"
            "        modgui:brand \"" P2_LV2_BRAND "\" ;\n"
            "        modgui:label \"Pulse2Alike\" ;\n");

    // the knobs the panel opens on
    for (int k = 0, n = 0; k < KNOBS; k++) {
        const tP2Lv2Port * port = p2_lv2_port_of(first->slot[k].id);

        if (port == NULL) {
            continue;
        }
        fprintf(f, "%s[ lv2:index %d ; lv2:symbol \"%s\" ; lv2:name \"%s\" ]", (n == 0) ? "        modgui:port " : " ,\n                    ",
                n, port->symbol, port->shortName);
        n++;
    }

    fprintf(f, " ;\n    ] .\n");
}

// ---- the template -------------------------------------------------------------------------------------------------

// notes §16: a row is as tall as its LED labels, or its matrix text where pages differ, stacked
static int row_lines(int row) {
    int pages = 0, lines = 1;

    for (int p = 0; p < PAGES; p++) {
        pages += (kPages[p].button == row) ? 1 : 0;
    }

    for (int k = 0; k < KNOBS; k++) {
        const char * label    = NULL;
        bool         differ   = false;
        int          onMatrix = 0;

        for (int p = 0; p < PAGES; p++) {
            if ((kPages[p].button != row) || !kPages[p].matrix) {
                continue;
            }
            onMatrix++;
            differ |= (label != NULL) && (strcmp(label, kPages[p].slot[k].label) != 0);
            label   = kPages[p].slot[k].label;
        }

        lines = (differ && (onMatrix > lines)) ? onMatrix : lines;
    }

    return (pages > lines) ? pages : lines;
}

static int row_top(int row) {
    int y = MATRIX_Y;

    for (int r = 0; r < row; r++) {
        y += (row_lines(r) * LINE_H) + ROW_PAD;
    }

    return y;
}

// The pages a row button or a shift label steps through, space-separated
static void put_cycle(FILE * f, int button) {
    bool first = true;

    for (int p = 0; p < PAGES; p++) {
        if (kPages[p].button == button) {
            fprintf(f, "%s%s", first ? "" : " ", kPages[p].id);
            first = false;
        }
    }
}

// The row's pages that are on the matrix: what a label shared by all of them lights for
static void put_matrix_pages(FILE * f, int row) {
    bool first = true;

    for (int p = 0; p < PAGES; p++) {
        if ((kPages[p].button == row) && kPages[p].matrix) {
            fprintf(f, "%s%s", first ? "" : " ", kPages[p].id);
            first = false;
        }
    }
}

static void put_shown(FILE * f, const char * id) {
    fprintf(f, "%s", (strcmp(id, DEFAULT_PAGE) == 0) ? " p2a-shown" : "");
}

static void write_matrix(FILE * f) {
    int bottom = row_top(ROWS);

    for (int r = 0; r < ROWS; r++) {
        int top = row_top(r), height = row_top(r + 1) - top, boxes = 0;

        fprintf(f, "        <div class=\"p2a-row\" style=\"top:%dpx;height:%dpx\"></div>\n", top, height);
        fprintf(f, "        <div class=\"p2a-button\" data-p2a-cycle=\"");
        put_cycle(f, r);
        fprintf(f, "\" style=\"left:%dpx;top:%dpx\"></div>\n", BUTTON_X, top + (height / 2) - 7);

        for (int p = 0; p < PAGES; p++) {
            boxes += (kPages[p].button == r) ? 1 : 0;
        }

        // the pages' LEDs and labels, stacked about the row's middle
        for (int p = 0, i = 0; p < PAGES; p++) {
            if (kPages[p].button != r) {
                continue;
            }
            int y = top + (height / 2) - ((boxes * LINE_H) / 2) + (i * LINE_H);

            fprintf(f, "        <div class=\"p2a-led%s\" data-p2a-page=\"%s\" style=\"left:%dpx;top:%dpx\"></div>\n",
                    (strcmp(kPages[p].id, DEFAULT_PAGE) == 0) ? " p2a-lit" : "", kPages[p].id, LED_X, y + 3);
            fprintf(f, "        <div class=\"p2a-box\" data-p2a-page=\"%s\" style=\"left:%dpx;top:%dpx\">", kPages[p].id,
                    LED_X + 10, y);
            put_html(f, kPages[p].led);
            fprintf(f, "</div>\n");
            i++;
        }

        // the text over each knob: once where the row's pages agree, a line each where they do not
        for (int k = 0; k < KNOBS; k++) {
            const char * label  = NULL;
            bool         differ = false;

            for (int p = 0; p < PAGES; p++) {
                if ((kPages[p].button == r) && kPages[p].matrix) {
                    differ |= (label != NULL) && (strcmp(label, kPages[p].slot[k].label) != 0);
                    label   = kPages[p].slot[k].label;
                }
            }

            fprintf(f, "        <div class=\"p2a-cell\" style=\"left:%dpx;top:%dpx;width:%dpx;height:%dpx\">", MATRIX_X + (k * COLUMN_W), top,
                    COLUMN_W, height);

            for (int p = 0, line = 0; p < PAGES; p++) {
                if ((kPages[p].button != r) || !kPages[p].matrix || (differ ? false : (line > 0))) {
                    continue;
                }
                fprintf(f, "<span data-p2a-page=\"");

                if (differ) {
                    fprintf(f, "%s", kPages[p].id);
                } else {
                    put_matrix_pages(f, r);
                }
                fprintf(f, "\">");

                if (kPages[p].slot[k].tag != NULL) {
                    fprintf(f, "<b>");
                    put_html(f, kPages[p].slot[k].tag);
                    fprintf(f, "</b> ");
                }
                put_html(f, kPages[p].slot[k].label);
                fprintf(f, "</span>");
                line++;
            }

            fprintf(f, "</div>\n");
        }
    }

    for (int k = 1; k < KNOBS; k++) {
        fprintf(f, "        <div class=\"p2a-column\" style=\"left:%dpx;top:%dpx;height:%dpx\"></div>\n", MATRIX_X + (k * COLUMN_W),
                MATRIX_Y, bottom - MATRIX_Y + KNOB_GAP + KNOB_SIZE);
    }

    // the instrument's shift labels for the two that reach further: Global over the envelopes, Mod over the arp
    fprintf(f, "        <div class=\"p2a-shift\" data-p2a-cycle=\"");
    put_cycle(f, BUTTON_GLOBAL);
    fprintf(f, "\" style=\"left:%dpx;top:%dpx\">Global</div>\n", SHIFT_X, row_top(2) + ((row_top(3) - row_top(2)) / 2) - 6);
    fprintf(f, "        <div class=\"p2a-shift\" data-p2a-cycle=\"");
    put_cycle(f, BUTTON_MOD);
    fprintf(f, "\" style=\"left:%dpx;top:%dpx\">Mod</div>\n", SHIFT_X, row_top(3) + ((row_top(4) - row_top(3)) / 2) - 6);
}

// A knob: a still cap, and the pointer in it that turns - the light on steel does not turn with a knob (notes §16)
static void write_knob(FILE * f, const tP2Lv2Port * port, const char * page, int x, int y, int size) {
    fprintf(f, "        <div class=\"p2a-knob");

    if (page != NULL) {
        fprintf(f, " p2a-paged");
        put_shown(f, page);
        fprintf(f, "\" data-p2a-page=\"%s", page);
    }
    fprintf(f, "\" style=\"left:%dpx;top:%dpx;width:%dpx;height:%dpx\">", x, y, size, size);
    fprintf(f, "<div class=\"p2a-pointer\" mod-role=\"input-control-port\" mod-port-symbol=\"%s\" mod-widget-rotation=\"270\" title=\"",
            port->symbol);
    put_html(f, gP2Params[port->id].title);
    fprintf(f, "\"></div></div>\n");
}

// Each page's title and its knobs' values, where MOD writes them (notes §16)
static void write_lcd(FILE * f) {
    for (int p = 0; p < PAGES; p++) {
        int cell = 0;

        fprintf(f, "            <div class=\"p2a-lcd-title p2a-paged");
        put_shown(f, kPages[p].id);
        fprintf(f, "\" data-p2a-page=\"%s\">", kPages[p].id);
        put_html(f, kPages[p].title);
        fprintf(f, "</div>\n");

        for (int k = 0; k < KNOBS; k++) {
            const tP2Lv2Port * port = (kPages[p].slot[k].id == NONE) ? NULL : p2_lv2_port_of(kPages[p].slot[k].id);

            if (port == NULL) {
                continue;
            }
            fprintf(f, "            <div class=\"p2a-lcd-cell p2a-paged");
            put_shown(f, kPages[p].id);
            fprintf(f, "\" data-p2a-page=\"%s\" style=\"left:%dpx;top:%dpx;width:%dpx\"><span class=\"p2a-lcd-label\">",
                    kPages[p].id, (cell % LCD_COLUMNS) * LCD_CELL_W, LCD_TITLE_H + ((cell / LCD_COLUMNS) * LCD_CELL_H), LCD_CELL_W);
            put_html(f, port->shortName);
            fprintf(f, "</span><span class=\"p2a-lcd-value\" mod-role=\"input-control-value\" mod-port-symbol=\"%s\"></span></div>\n",
                    port->symbol);
            cell++;
        }
    }
}

// Every page's six knobs, in the same six places; only the page shown can be seen and turned
static void write_knobs(FILE * f) {
    int knobY = row_top(ROWS) + KNOB_GAP;

    for (int p = 0; p < PAGES; p++) {
        for (int k = 0; k < KNOBS; k++) {
            if (kPages[p].slot[k].id != NONE) {
                write_knob(f, p2_lv2_port_of(kPages[p].slot[k].id), kPages[p].id, MATRIX_X + (k * COLUMN_W) + ((COLUMN_W - KNOB_SIZE) / 2),
                           knobY, KNOB_SIZE);
            }
        }
    }
}

void p2_lv2_gui_write_icon(FILE * f) {
    fprintf(f, "<!-- GENERATED by lv2/p2Lv2Ttl.c from lv2/p2Lv2Gui.c - make -C lv2 ttl, do not edit. Docs/code-notes/p2Lv2.md §16 -->\n"
            "<div class=\"mod-pedal mod-pulse2alike{{{cns}}}\">\n"
            "    <div mod-role=\"drag-handle\" class=\"mod-drag-handle\"></div>\n"
            "    <div class=\"p2a-face\">\n"
            "        <div class=\"p2a-print\" style=\"left:150px;top:7px\">&#x23FB;</div>\n"
            "        <div class=\"p2a-power\" mod-role=\"bypass\" title=\"On / Off\"></div>\n"
            "        <div class=\"p2a-power-light\" mod-role=\"bypass-light\"></div>\n"
            "        <div class=\"p2a-print\" style=\"left:376px;top:7px\">&#x25BE; MIDI In</div>\n"
            "        <div class=\"p2a-print\" style=\"left:624px;top:7px\">Left &#x25B4; OUT &#x25B4; Right</div>\n"
            "        <svg class=\"p2a-art\" width=\"760\" height=\"300\" viewBox=\"0 0 760 300\">\n"
            "            <path d=\"M 17 158 V 226 H 70 C 98 226 104 226 112 214 C 118 204 122 200 126 214 C 132 238 140 226 160 226"
            " H 215 V 158\" />\n"
            "        </svg>\n");
    fprintf(f, "        <div class=\"p2a-lcd\" style=\"left:%dpx;top:%dpx\">\n", LCD_X, LCD_Y);
    write_lcd(f);
    fprintf(f, "        </div>\n");
    write_matrix(f);
    write_knobs(f);
    fprintf(f, "        <div class=\"p2a-arrow\" data-p2a-step=\"-1\" style=\"left:30px;top:172px\" title=\"Previous page\">&#x25C2;</div>\n"
            "        <div class=\"p2a-arrow\" data-p2a-step=\"1\" style=\"left:66px;top:192px\" title=\"Next page\">&#x25B8;</div>\n");
    write_knob(f, p2_lv2_port_of(MASTER_ID), NULL, 160, 170, 34);
    fprintf(f, "        <div class=\"p2a-print\" style=\"left:157px;top:208px\">Master</div>\n"
            "        <div class=\"p2a-name\">pulse2alike</div>\n"
            "        <div class=\"p2a-subname\">ANALOG MODEL &#xB7; LV2</div>\n"
            "    </div>\n"
            "    <div class=\"mod-pedal-input\">\n"
            "        {{#effect.ports.midi.input}}\n"
            "        <div class=\"mod-input mod-input-disconnected\" title=\"{{name}}\" mod-role=\"input-midi-port\" mod-port-symbol=\"{{symbol}}\">\n"
            "            <div class=\"mod-pedal-input-image\"></div>\n"
            "        </div>\n"
            "        {{/effect.ports.midi.input}}\n"
            "    </div>\n"
            "    <div class=\"mod-pedal-output\">\n"
            "        {{#effect.ports.audio.output}}\n"
            "        <div class=\"mod-output mod-output-disconnected\" title=\"{{name}}\" mod-role=\"output-audio-port\" mod-port-symbol=\"{{symbol}}\">\n"
            "            <div class=\"mod-pedal-output-image\"></div>\n"
            "        </div>\n"
            "        {{/effect.ports.audio.output}}\n"
            "    </div>\n"
            "</div>\n");
}
