/*
 * Pulse2Alike - the instrument's sound dumps (.syx) read into parameter values.
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
// Notes: Docs/code-notes/p2Patch.md - "// notes §k" refers there.

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "p2Patch.h"

// notes §1
#define DATA_OFFSET    (7)

bool p2_patch_from_syx(const uint8_t * data, size_t len, double normalized[kP2ParamCount], char name[P2_SOUND_NAME_LENGTH + 1]) {
    static const uint8_t header[3] = {0xF0, 0x3E, 0x16};

    if ((len < P2_DUMP_LENGTH) || (memcmp(data, header, 3) != 0) || (data[4] != 0x10) || (data[P2_DUMP_LENGTH - 1] != 0xF7)) {
        return false;
    }
    const uint8_t *      sound     = data + DATA_OFFSET;

    for (int b = 0; b < P2_SOUND_BYTES; b++) {
        uint32_t id   = gP2SoundBytes[b].paramId;

        if (id == P2_SOUND_BYTE_UNUSED) {
            continue;
        }
        double   span = gP2Params[id].plainMax - gP2Params[id].plainMin;
        double   v    = (span > 0.0) ? ((double)(sound[b] - gP2SoundBytes[b].rawLow) / span) : 0.0;

        normalized[id] = (v < 0.0) ? 0.0 : ((v > 1.0) ? 1.0 : v);
    }

    for (int i = 0; i < P2_SOUND_NAME_LENGTH; i++) {
        uint8_t c = sound[P2_SOUND_NAME_OFFSET + i];

        name[i] = ((c >= 32) && (c < 127)) ? (char)c : ' ';
    }

    name[P2_SOUND_NAME_LENGTH] = '\0';
    return true;
}

bool p2_patch_load_file(const char * path, double normalized[kP2ParamCount], char name[P2_SOUND_NAME_LENGTH + 1]) {
    uint8_t buffer[512];
    FILE *  f   = fopen(path, "rb");

    if (f == NULL) {
        return false;
    }
    size_t  len = fread(buffer, 1, sizeof(buffer), f);

    fclose(f);
    return p2_patch_from_syx(buffer, len, normalized, name);
}

bool p2_patch_read_dump(const char * path, uint8_t dump[P2_DUMP_LENGTH]) {
    double values[kP2ParamCount] = {0};
    char   name[P2_SOUND_NAME_LENGTH + 1];
    FILE * f                     = fopen(path, "rb");

    if (f == NULL) {
        return false;
    }
    size_t len                   = fread(dump, 1, P2_DUMP_LENGTH, f);

    fclose(f);
    return (len == P2_DUMP_LENGTH) && p2_patch_from_syx(dump, len, values, name);
}

int p2_patch_param_byte(uint32_t paramId, double normalized) {
    for (int b = 0; b < P2_SOUND_BYTES; b++) {
        if (gP2SoundBytes[b].paramId == paramId) {
            long raw = lround(normalized * (gP2Params[paramId].plainMax - gP2Params[paramId].plainMin)) + gP2SoundBytes[b].rawLow;

            return (raw < 0) ? 0 : ((raw > 127) ? 127 : (int)raw);
        }
    }

    return -1;
}

// notes §4: the inverse of p2_patch_from_syx - every byte the plug-in models from its parameter, the rest from
// the dump the sound came from (or zero), and the name
void p2_patch_to_syx(const double normalized[kP2ParamCount], const uint8_t * base, const char * name, uint8_t out[P2_DUMP_LENGTH]) {
    static const uint8_t editBuffer[DATA_OFFSET] = {0xF0, 0x3E, 0x16, 0x7F, 0x10, 0x7F, 0x00};
    uint8_t *            sound                   = out + DATA_OFFSET;
    int                  sum                     = 0;

    if (base != NULL) {
        memcpy(out, base, P2_DUMP_LENGTH);
    } else {
        memset(out, 0, P2_DUMP_LENGTH);
        memcpy(out, editBuffer, DATA_OFFSET);
    }

    for (int b = 0; b < P2_SOUND_BYTES; b++) {
        uint32_t id   = gP2SoundBytes[b].paramId;

        if (id == P2_SOUND_BYTE_UNUSED) {
            continue;
        }
        double   span = gP2Params[id].plainMax - gP2Params[id].plainMin;
        long     raw  = lround(normalized[id] * span) + gP2SoundBytes[b].rawLow;

        sound[b] = (uint8_t)((raw < 0) ? 0 : ((raw > 127) ? 127 : raw));
    }

    for (int i = 0; i < P2_SOUND_NAME_LENGTH; i++) {
        char c = ((name != NULL) && (i < (int)strlen(name))) ? name[i] : ' ';

        sound[P2_SOUND_NAME_OFFSET + i] = (uint8_t)(((c >= 32) && (c < 127)) ? c : ' ');
    }

    for (int i = 1; i < P2_DUMP_LENGTH - 2; i++) {
        sum += out[i];
    }

    out[P2_DUMP_LENGTH - 2] = (uint8_t)(sum & 0x7F);
    out[P2_DUMP_LENGTH - 1] = 0xF7;
}

bool p2_patch_save_file(const char * path, const uint8_t dump[P2_DUMP_LENGTH]) {
    FILE * f = fopen(path, "wb");
    bool   ok;

    if (f == NULL) {
        return false;
    }
    ok = fwrite(dump, 1, P2_DUMP_LENGTH, f) == P2_DUMP_LENGTH;
    return (fclose(f) == 0) && ok;
}

// notes §2
#define SOUNDS_FOLDER     "Library/Mobile Documents/com~apple~CloudDocs/Patches/Pulse2"   // under $HOME
#define PRESETS_FOLDER    "Library/Application Support/Pulse2Alike"                       // beside prefs.txt
#define PRESETS_FILE      "presets.syx"

// notes §5: each slot is a seqlock - the main thread writes it, the audio thread copies it and keeps the copy only
// if the generation did not move (odd = being written)
static uint8_t          gCache[P2_PATCH_SLOTS][P2_DUMP_LENGTH];
static _Atomic uint32_t gGeneration[P2_PATCH_SLOTS];
static char             gCacheName[P2_PATCH_SLOTS][P2_SOUND_NAME_LENGTH + 1];
static struct timespec  gFileTime;   // the presets file's modification time when last read or written
static pthread_once_t   gCacheOnce = PTHREAD_ONCE_INIT;

const char * p2_patch_sounds_folder(void) {
    static char folder[1024];

    snprintf(folder, sizeof(folder), "%s/%s", getenv("HOME") ? getenv("HOME") : "", SOUNDS_FOLDER);
    return folder;
}

const char * p2_presets_path(void) {
    static char path[1024];

    snprintf(path, sizeof(path), "%s/%s/%s", getenv("HOME") ? getenv("HOME") : "", PRESETS_FOLDER, PRESETS_FILE);
    return path;
}

// "NNN Name.syx" in any folder -> NNN, otherwise 0: the instrument's number a file stands for
int p2_patch_file_number(const char * path) {
    const char * name = strrchr(path, '/');
    int          number;

    name   = (name != NULL) ? (name + 1) : path;
    number = atoi(name);
    return ((number >= 1) && (number <= P2_PATCH_SLOTS) && (name[0] >= '0') && (name[0] <= '9')) ? number : 0;
}

// notes §6: a dump readdressed to slot n, checksum recomputed
void p2_patch_address(uint8_t dump[P2_DUMP_LENGTH], int number) {
    int sum = 0;

    dump[5]                  = (uint8_t)(((number - 1) >> 7) & 0x7F);
    dump[6]                  = (uint8_t)((number - 1) & 0x7F);

    for (int i = 1; i < P2_DUMP_LENGTH - 2; i++) {
        sum += dump[i];
    }

    dump[P2_DUMP_LENGTH - 2] = (uint8_t)(sum & 0x7F);
}

// notes §6: every parameter at its default, named INIT - what an empty slot holds, as on the instrument
void p2_patch_init_dump(int number, uint8_t dump[P2_DUMP_LENGTH]) {
    double values[kP2ParamCount];

    for (uint32_t i = 0; i < kP2ParamCount; i++) {
        values[i] = gP2Params[i].defaultNormalized;
    }

    p2_patch_to_syx(values, NULL, "INIT", dump);
    dump[3] = 0x00;
    p2_patch_address(dump, number);
}

static void slot_write(int number, const uint8_t dump[P2_DUMP_LENGTH]) {
    int    i                     = number - 1;
    double values[kP2ParamCount] = {0};

    atomic_fetch_add(&gGeneration[i], 1);
    memcpy(gCache[i], dump, P2_DUMP_LENGTH);
    (void)p2_patch_from_syx(dump, P2_DUMP_LENGTH, values, gCacheName[i]);

    for (int k = P2_SOUND_NAME_LENGTH - 1; (k >= 0) && (gCacheName[i][k] == ' '); k--) {
        gCacheName[i][k] = '\0';           // the dump pads it to 14
    }

    atomic_fetch_add(&gGeneration[i], 1);
}

static bool file_time(struct timespec * t) {
    struct stat st;

    if (stat(p2_presets_path(), &st) != 0) {
        return false;
    }
#if defined (__APPLE__)
    *t = st.st_mtimespec;
#else
    *t = st.st_mtim;   // POSIX 2008's name for it; macOS has only its own
#endif
    return true;
}

// notes §6: slot n from the file's dump n, or INIT where the file is missing, short or not a sound there
static void cache_read(void) {
    FILE *  f = fopen(p2_presets_path(), "rb");
    uint8_t dump[P2_DUMP_LENGTH];

    for (int n = 1; n <= P2_PATCH_SLOTS; n++) {
        double values[kP2ParamCount];
        char   name[P2_SOUND_NAME_LENGTH + 1];

        if (  (f == NULL) || (fread(dump, 1, P2_DUMP_LENGTH, f) != P2_DUMP_LENGTH)
           || !p2_patch_from_syx(dump, P2_DUMP_LENGTH, values, name)) {
            p2_patch_init_dump(n, dump);
        }

        if (memcmp(dump, gCache[n - 1], P2_DUMP_LENGTH) != 0) {
            slot_write(n, dump);
        }
    }

    if (f != NULL) {
        fclose(f);
    }

    if (!file_time(&gFileTime)) {
        gFileTime = (struct timespec){
            0, 0
        };
    }
}

void p2_presets_load(void) {
    pthread_once(&gCacheOnce, cache_read);
}

bool p2_presets_refresh(void) {
    struct timespec now;

    p2_presets_load();

    if (!file_time(&now) || ((now.tv_sec == gFileTime.tv_sec) && (now.tv_nsec == gFileTime.tv_nsec))) {
        return false;
    }
    cache_read();
    return true;
}

// The presets file's folder, and each missing one on the way, as mkdir -p
void p2_presets_make_folder(void) {
    char dir[1024];

    snprintf(dir, sizeof(dir), "%s", p2_presets_path());
    *strrchr(dir, '/') = '\0';

    for (char * c = dir + 1; *c != '\0'; c++) {
        if (*c == '/') {
            *c = '\0';
            mkdir(dir, 0755);
            *c = '/';
        }
    }

    mkdir(dir, 0755);
}

// The whole cache to the file, through a temporary so a failed write never leaves half a file
static bool file_write_all(void) {
    char   temp[1100];
    FILE * f;
    bool   ok;

    p2_presets_make_folder();
    snprintf(temp, sizeof(temp), "%s.tmp", p2_presets_path());
    f  = fopen(temp, "wb");

    if (f == NULL) {
        return false;
    }
    ok = fwrite(gCache, 1, sizeof(gCache), f) == sizeof(gCache);
    ok = (fclose(f) == 0) && ok && (rename(temp, p2_presets_path()) == 0);
    return ok;
}

// notes §6: write-through - the slot in the cache, then its 137 bytes in place (the whole file if it is not whole)
bool p2_presets_store(int number, const uint8_t dump[P2_DUMP_LENGTH]) {
    uint8_t     slot[P2_DUMP_LENGTH];
    struct stat st;
    bool        ok = false;

    if ((number < 1) || (number > P2_PATCH_SLOTS)) {
        return false;
    }
    p2_presets_refresh();
    memcpy(slot, dump, P2_DUMP_LENGTH);
    p2_patch_address(slot, number);
    slot_write(number, slot);

    if ((stat(p2_presets_path(), &st) == 0) && (st.st_size == (off_t)sizeof(gCache))) {
        int fd = open(p2_presets_path(), O_WRONLY);

        ok = (fd >= 0) && (pwrite(fd, slot, P2_DUMP_LENGTH, (off_t)(number - 1) * P2_DUMP_LENGTH) == P2_DUMP_LENGTH);
        ok = (fd >= 0) && (close(fd) == 0) && ok;
    } else {
        ok = file_write_all();
    }
    (void)file_time(&gFileTime);
    return ok;
}

// notes §6: all 500 at once (an import), one file write
bool p2_presets_store_all(const uint8_t dumps[P2_PATCH_SLOTS][P2_DUMP_LENGTH]) {
    bool ok;

    p2_presets_load();

    for (int n = 1; n <= P2_PATCH_SLOTS; n++) {
        uint8_t slot[P2_DUMP_LENGTH];

        memcpy(slot, dumps[n - 1], P2_DUMP_LENGTH);
        p2_patch_address(slot, n);
        slot_write(n, slot);
    }

    ok = file_write_all();
    (void)file_time(&gFileTime);
    return ok;
}

// notes §7: a .syx of sound dumps - each to the slot it is addressed to, or in turn when it is addressed to the edit
// buffer; the rest INIT. The number of sounds read; 0 leaves dumps untouched
int p2_presets_read_file(const char * path, uint8_t dumps[P2_PATCH_SLOTS][P2_DUMP_LENGTH]) {
    static uint8_t data[P2_PATCH_SLOTS * P2_DUMP_LENGTH * 2];
    static uint8_t found[P2_PATCH_SLOTS][P2_DUMP_LENGTH];
    bool           have[P2_PATCH_SLOTS] = {false};
    FILE *         f                    = fopen(path, "rb");
    size_t         len;
    int            count = 0, next = 0;

    if (f == NULL) {
        return 0;
    }
    len = fread(data, 1, sizeof(data), f);
    fclose(f);

    for (size_t at = 0; at + P2_DUMP_LENGTH <= len;) {
        double values[kP2ParamCount];
        char   name[P2_SOUND_NAME_LENGTH + 1];

        if (!p2_patch_from_syx(data + at, P2_DUMP_LENGTH, values, name)) {
            at++;            // not a sound dump here: look further
            continue;
        }
        int    slot = (data[at + 5] << 7) | data[at + 6];

        slot = ((data[at + 5] == 0x7F) || (slot >= P2_PATCH_SLOTS)) ? next : slot;

        if ((slot < P2_PATCH_SLOTS) && !have[slot]) {
            memcpy(found[slot], data + at, P2_DUMP_LENGTH);
            have[slot] = true;
            count++;
        }
        next = slot + 1;
        at  += P2_DUMP_LENGTH;
    }

    for (int n = 1; (count > 0) && (n <= P2_PATCH_SLOTS); n++) {
        if (have[n - 1]) {
            memcpy(dumps[n - 1], found[n - 1], P2_DUMP_LENGTH);
        } else {
            p2_patch_init_dump(n, dumps[n - 1]);
        }
    }

    return count;
}

// notes §7: "NNN Name.syx" files, each to slot NNN; the rest INIT. The number read; 0 leaves dumps untouched
int p2_presets_read_folder(const char * folder, uint8_t dumps[P2_PATCH_SLOTS][P2_DUMP_LENGTH]) {
    static uint8_t  found[P2_PATCH_SLOTS][P2_DUMP_LENGTH];
    bool            have[P2_PATCH_SLOTS] = {false};
    DIR *           dir                  = opendir(folder);
    struct dirent * e;
    int             count                = 0;

    while ((dir != NULL) && ((e = readdir(dir)) != NULL)) {
        size_t n      = strlen(e->d_name);
        int    number = p2_patch_file_number(e->d_name);
        char   path[2048];

        if ((number == 0) || (n < 5) || (strcmp(e->d_name + n - 4, ".syx") != 0) || have[number - 1]) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", folder, e->d_name);

        if (p2_patch_read_dump(path, found[number - 1])) {
            have[number - 1] = true;
            count++;
        }
    }

    if (dir != NULL) {
        closedir(dir);
    }

    for (int n = 1; (count > 0) && (n <= P2_PATCH_SLOTS); n++) {
        if (have[n - 1]) {
            memcpy(dumps[n - 1], found[n - 1], P2_DUMP_LENGTH);
            p2_patch_address(dumps[n - 1], n);
        } else {
            p2_patch_init_dump(n, dumps[n - 1]);
        }
    }

    return count;
}

// notes §7: the 500 as they are, to any file
bool p2_presets_write_file(const char * path) {
    FILE * f;
    bool   ok;

    p2_presets_refresh();
    f  = fopen(path, "wb");

    if (f == NULL) {
        return false;
    }
    ok = fwrite(gCache, 1, sizeof(gCache), f) == sizeof(gCache);
    return (fclose(f) == 0) && ok;
}

// notes §7: one "NNN Name.syx" per preset, the name trimmed and kept to what a file name can hold
int p2_presets_write_folder(const char * folder) {
    int written = 0;

    p2_presets_refresh();

    for (int n = 1; n <= P2_PATCH_SLOTS; n++) {
        char name[P2_SOUND_NAME_LENGTH + 1], path[2048];
        int  k = (int)strlen(gCacheName[n - 1]);

        snprintf(name, sizeof(name), "%.*s", P2_SOUND_NAME_LENGTH, gCacheName[n - 1]);   // bounded, or gcc warns

        while ((k > 0) && (name[k - 1] == ' ')) {
            name[--k] = '\0';
        }

        for (int i = 0; i < k; i++) {
            name[i] = ((name[i] == '/') || (name[i] == ':')) ? '-' : name[i];
        }

        snprintf(path, sizeof(path), "%s/%03d %s.syx", folder, n, name);
        written += p2_patch_save_file(path, gCache[n - 1]) ? 1 : 0;
    }

    return written;
}

// The audio thread's read: no lock, no file - a copy that is kept only if no write overlapped it
bool p2_patch_cached(int number, double normalized[kP2ParamCount], char name[P2_SOUND_NAME_LENGTH + 1]) {
    uint8_t  copy[P2_DUMP_LENGTH];
    uint32_t before, after;

    if ((number < 1) || (number > P2_PATCH_SLOTS)) {
        return false;
    }
    before = atomic_load_explicit(&gGeneration[number - 1], memory_order_acquire);

    if (before & 1u) {
        return false;
    }
    memcpy(copy, gCache[number - 1], P2_DUMP_LENGTH);
    atomic_thread_fence(memory_order_acquire);
    after  = atomic_load_explicit(&gGeneration[number - 1], memory_order_relaxed);
    return (before == after) && p2_patch_from_syx(copy, P2_DUMP_LENGTH, normalized, name);
}

// Main thread only, as are the writes
void p2_presets_dump(int number, uint8_t dump[P2_DUMP_LENGTH]) {
    p2_presets_load();
    memcpy(dump, gCache[((number >= 1) && (number <= P2_PATCH_SLOTS)) ? number - 1 : 0], P2_DUMP_LENGTH);
}

const char * p2_presets_name(int number) {
    p2_presets_load();
    return gCacheName[((number >= 1) && (number <= P2_PATCH_SLOTS)) ? number - 1 : 0];
}

// notes §3: as the instrument picks a sound - 100 to a bank (bank select mod 5), programs above 99 clamped
int p2_patch_program_number(int bank, int program) {
    return (100 * (bank % 5)) + ((program > 99) ? 99 : program) + 1;
}
