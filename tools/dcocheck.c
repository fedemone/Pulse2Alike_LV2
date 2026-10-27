// Offline check of the DCO law in src/p2Dco.c against the instrument. Build and run: tools/do-dcocheck
#include <math.h>
#include <stdio.h>

#include "p2Dco.h"

static int failures;

// Osc 1 saw, captured 2026-10-17 at 192 kHz (findings), divided by the interface clock offset measured in the
// same take (+1.5 ppm).
static const struct {
    int    note;
    double hz;
}          kCaptured[] = {
    {36, 65.4066}, {48, 130.8132}, {60, 261.6264}, {72, 523.2642}, {84, 1046.5740}, {96, 2093.1481},
};

#define CLOCK_OFFSET_PPM    (1.5)
#define TOLERANCE_CENTS     (0.02)

// p2Dco notes §2-3 as first written, with pow(): the base-period table must give exactly this at every pitch
static uint32_t law_period_ticks(int32_t pitch) {
    int32_t  semitone = pitch / P2_PITCH_PER_SEMITONE;
    int32_t  note     = semitone % 12;
    double   hz       = 440.0 * pow(2.0, ((double)note - 105.0) / 12.0);
    double   hzNext   = 440.0 * pow(2.0, ((double)note + 1.0 - 105.0) / 12.0);
    uint32_t base     = (uint32_t)floor((2.0 * P2_DCO_CLOCK_HZ) / hz);
    uint32_t slope    = base - (uint32_t)floor((2.0 * P2_DCO_CLOCK_HZ) / hzNext);

    return (base - ((slope * (uint32_t)(pitch % P2_PITCH_PER_SEMITONE)) >> 8)) >> ((semitone / 12) + 1);
}

int main(void) {
    for (int32_t pitch = 0; pitch <= P2_PITCH_MAX; pitch++) {
        if (p2_dco_period_ticks(pitch) != law_period_ticks(pitch)) {
            printf("pitch %d: %u ticks, the law %u\nFAIL\n", pitch, p2_dco_period_ticks(pitch), law_period_ticks(pitch));
            failures++;
            break;
        }
    }

    printf("base periods: %s at every pitch 0..%d\n", failures ? "differ from the law" : "the law's", P2_PITCH_MAX);

    for (unsigned i = 0; i < sizeof(kCaptured) / sizeof(kCaptured[0]); i++) {
        double law      = P2_DCO_CLOCK_HZ / p2_dco_period_ticks(p2_pitch_from_midi_note(kCaptured[i].note));
        double measured = kCaptured[i].hz / (1.0 + CLOCK_OFFSET_PPM * 1e-6);
        double cents    = 1200.0 * log2(measured / law);

        printf("MIDI %3d: law %10.4f Hz, captured %10.4f Hz, %+.3f cents\n", kCaptured[i].note, law, measured, cents);

        if (fabs(cents) > TOLERANCE_CENTS) {
            printf("FAIL\n");
            failures++;
        }
    }

    printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
