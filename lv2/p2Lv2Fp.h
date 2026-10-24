/*
 * Pulse2Alike - flush-to-zero for the length of an LV2 run(), the host's own mode put back after.
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

#ifndef __P2_LV2_FP_H__
#define __P2_LV2_FP_H__

#include <stdint.h>

#if defined (__SSE__)
#include <xmmintrin.h>
#endif

// notes §9
#if defined (__aarch64__)
typedef uint64_t tP2FpMode;

static inline tP2FpMode p2_fp_enter(void) {
    uint64_t fpcr;

    __asm__ __volatile__ ("mrs %0, fpcr" : "=r" (fpcr));
    __asm__ __volatile__ ("msr fpcr, %0" : : "r" (fpcr | (1ull << 24)));   // FZ
    return fpcr;
}

static inline void p2_fp_leave(tP2FpMode mode) {
    __asm__ __volatile__ ("msr fpcr, %0" : : "r" (mode));
}

#elif defined (__SSE__)
typedef unsigned int tP2FpMode;

static inline tP2FpMode p2_fp_enter(void) {
    tP2FpMode mode = _mm_getcsr();

    _mm_setcsr(mode | 0x8040u);   // FTZ and DAZ
    return mode;
}

static inline void p2_fp_leave(tP2FpMode mode) {
    _mm_setcsr(mode);
}

#else
typedef int tP2FpMode;

static inline tP2FpMode p2_fp_enter(void) {
    return 0;
}

static inline void p2_fp_leave(tP2FpMode mode) {
    (void)mode;
}

#endif

#endif // __P2_LV2_FP_H__
