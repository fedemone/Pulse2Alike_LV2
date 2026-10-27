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

#ifndef __P2_LV2_GUI_H__
#define __P2_LV2_GUI_H__

#include <stdio.h>

// notes §16: problems with the page table (each written to stderr); 0 when every control is on it once
int p2_lv2_gui_check(void);

// modgui.ttl, and modgui/icon-pulse2alike.html; the stylesheet and the script are written by hand
void p2_lv2_gui_write_ttl(FILE * f);
void p2_lv2_gui_write_icon(FILE * f);

#endif // __P2_LV2_GUI_H__
