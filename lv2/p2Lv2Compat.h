/*
 * Pulse2Alike - what SynthLib's plug-in log needs from Linux that macOS has under another name. Force-included into
 * SynthLib/plugin/synthlibLog.c by lv2/Makefile, so SynthLib builds unchanged.
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

#ifndef __P2_LV2_COMPAT_H__
#define __P2_LV2_COMPAT_H__

#if defined (__linux__) && !defined (__ANDROID__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>

// glibc has no getprogname(); its program_invocation_short_name is the same thing
#define getprogname()    (program_invocation_short_name)
#endif

#endif // __P2_LV2_COMPAT_H__
