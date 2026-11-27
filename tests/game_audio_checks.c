/* Actual command queue, WAV decoder and 24-voice mixer; no OS audio output. */
#include "game_audio.h"
#include "game_events.h"
#include "audio_cache.h"
#include <stdlib.h>
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>
extern void game_audio_test_step(int16_t out[2048]);
extern int game_audio_test_prepare(const AudioAsset *assets, unsigned count, int complete);
typedef struct {
    int active, id;
    unsigned position;
} FakeFile;
static FakeFile files[32];
static int open_id;
static unsigned opens, closes, reads, seeks;
static unsigned threads_started;
static int fail_loader;
static unsigned char heap[3][12332];
static int allocated[3];
void *malloc(size_t n) {
    if (n > 12332)
        return NULL;
    for (int i = 0; i < 3; ++i)
        if (!allocated[i]) {
            allocated[i] = 1;
            return heap[i];
        }
    return NULL;
}
void free(void *p) {
    for (int i = 0; i < 3; ++i)
        if (p == heap[i])
            allocated[i] = 0;
}
static int16_t output[2048];
int snprintf(char *out, size_t size, const char *format, ...) {
    (void)format;
    va_list args;
    va_start(args, format);
    open_id = va_arg(args, int);
    va_end(args);
    if (size)
        out[0] = 0;
    return 0;
}
FILE *fopen(const char *p, const char *m) {
    (void)p;
    (void)m;
    ++opens;
    for (int i = 0; i < 32; ++i)
        if (!files[i].active) {
            files[i] = (FakeFile){1, open_id, 0};
            return (FILE *)&files[i];
        }
    return NULL;
}
int fclose(FILE *f) {
    ((FakeFile *)f)->active = 0;
    ++closes;
    return 0;
}
long ftell(FILE *f) {
    return (long)((FakeFile *)f)->position;
}
int fseek(FILE *fp, long offset, int origin) {
    FakeFile *f = (FakeFile *)fp;
    ++seeks;
    f->position = origin == SEEK_SET   ? (unsigned)offset
                  : origin == SEEK_END ? 12332u + (unsigned)offset
                                       : f->position + (unsigned)offset;
    return 0;
}
size_t fread(void *destination, size_t size, size_t count, FILE *fp) {
    FakeFile *f = (FakeFile *)fp;
    ++reads;
    unsigned char h[44] = {'R', 'I', 'F', 'F', 36, 48, 0,   0,   'W', 'A', 'V', 'E', 'f', 'm', 't',
                           ' ', 16,  0,   0,   0,  1,  0,   2,   0,   68,  172, 0,   0,   16,  177,
                           2,   0,   4,   0,   16, 0,  'd', 'a', 't', 'a', 0,   48,  0,   0};
    unsigned length = 44 + 12288, n = (unsigned)(size * count);
    if (f->position >= length)
        return 0;
    if (n > length - f->position)
        n = length - f->position;
    unsigned char *out = destination;
    for (unsigned i = 0; i < n; ++i) {
        unsigned pos = f->position++;
        if (pos < 44)
            out[i] = h[pos];
        else {
            unsigned block = (pos - 44) / 4096;
            int value = (int)(block + 1) * (f->id == 2 ? 2000 : 1000);
            out[i] = (unsigned char)((unsigned)value >> (((pos - 44) & 1) * 8));
        }
    }
    return n / size;
}
int ov_open_callbacks(void *source, OggVorbis_File *v, const char *initial, long bytes,
                      ov_callbacks callbacks) {
    (void)source;
    (void)v;
    (void)initial;
    (void)bytes;
    (void)callbacks;
    return -1;
}
int ov_clear(OggVorbis_File *v) {
    (void)v;
    return 0;
}
vorbis_info *ov_info(OggVorbis_File *v, int link) {
    (void)v;
    (void)link;
    static vorbis_info info;
    info.channels = 2;
    info.rate = 44100;
    return &info;
}
long ov_read(OggVorbis_File *v, char *out, int bytes, int endian, int word, int sign, int *stream) {
    (void)v;
    (void)out;
    (void)bytes;
    (void)endian;
    (void)word;
    (void)sign;
    (void)stream;
    return 0;
}
int ov_pcm_seek(OggVorbis_File *v, ogg_int64_t n) {
    (void)v;
    (void)n;
    return 0;
}
SceUID sceKernelCreateThread(const char *name, SceKernelThreadEntry entry, int priority,
                             SceSize stack, SceUInt attr, int affinity,
                             const SceKernelThreadOptParam *opt) {
    (void)entry;
    (void)priority;
    (void)stack;
    (void)attr;
    (void)affinity;
    (void)opt;
    return fail_loader && !strcmp(name, "FnafAudioPreload") ? -1 : 1;
}
int sceKernelStartThread(SceUID id, SceSize n, void *p) {
    (void)id;
    (void)n;
    (void)p;
    ++threads_started;
    return 0;
}
int sceKernelWaitThreadEnd(SceUID id, int *status, SceUInt *timeout) {
    (void)id;
    (void)status;
    (void)timeout;
    return 0;
}
int sceKernelDeleteThread(SceUID id) {
    (void)id;
    return 0;
}
int sceAudioOutOpenPort(SceAudioOutPortType type, int length, int frequency, SceAudioOutMode mode) {
    (void)type;
    (void)length;
    (void)frequency;
    (void)mode;
    return 1;
}
int sceAudioOutOutput(int port, const void *data) {
    (void)port;
    (void)data;
    return 0;
}
int sceAudioOutReleasePort(int port) {
    (void)port;
    return 0;
}
#define CHECK(c, n)                                                                                \
    do {                                                                                           \
        if (!(c))                                                                                  \
            return 900 + (n);                                                                      \
    } while (0)
int run_checks(void) {
    CHECK(!game_audio_start() && !game_audio_ready() && !game_audio_title_ready(), 1);
    game_audio_preload_gameplay(0);
    CHECK(threads_started == 1 && !opens, 26);
    const AudioAsset title = {35, 0, 12332};
    CHECK(!game_audio_test_prepare(&title, 1, 0) && game_audio_title_ready() && !game_audio_ready(),
          21);
    game_audio_preload_gameplay(0);
    game_audio_preload_gameplay(0);
    CHECK(threads_started == 2 && opens == 1 && !game_audio_ready(), 27);
    game_audio_settings(100, 1, 1);
    game_audio_play(35, 0, 1, 100);
    game_audio_test_step(output);
    CHECK(!output[0] && !game_audio_started(0), 28);
    game_audio_settings(100, 0, 1);
    game_audio_test_step(output);
    CHECK(output[0] == 1000 && opens == 1 && game_audio_started(0) && !game_audio_errors(), 22);
    game_audio_play(-1, 0, 0, 0);
    CHECK(!game_audio_started(0), 29);
    const AudioAsset assets[] = {{1, 0, 17}, {2, 0, 12332}};
    CHECK(!game_audio_test_prepare(assets, 2, 1) && game_audio_ready() && opens == 3 && reads == 3,
          12);
    game_audio_settings(100, 0, 1);
    game_audio_play(1, 23, 0, 100);
    game_audio_test_step(output);
    CHECK(output[0] == 1000 && output[2047] == 1000 && opens == 3, 2);
    game_audio_play(-2, 23, 0, 50);
    game_audio_test_step(output);
    CHECK(output[0] == 1000 && opens == 3 && game_audio_position(23) == 46, 3);
    game_audio_settings(100, 1, 1);
    game_audio_test_step(output);
    CHECK(!output[0] && game_audio_position(23) == 46, 4);
    game_audio_settings(100, 0, 1);
    game_audio_test_step(output);
    CHECK(output[0] == 1500 && opens == 3 && !game_audio_finished(23), 5);
    game_audio_test_step(output);
    CHECK(!output[0] && game_audio_finished(23), 23);
    game_audio_play(1, 23, 0, 100);
    CHECK(!game_audio_finished(23), 24);
    game_audio_play(-1, 23, 0, 0);
    game_audio_test_step(output);
    CHECK(!output[0] && closes == 3, 6);
    game_audio_play(1, 8, 0, 100);
    game_audio_play(2, 9, 0, 100);
    game_audio_settings(50, 0, 1);
    game_audio_test_step(output);
    CHECK(output[0] == 1500, 7);
    game_audio_play(-1, 8, 0, 0);
    game_audio_play(-1, 9, 0, 0);
    game_audio_play(35, 14, 1, 100);
    game_audio_settings(100, 0, 0);
    game_audio_test_step(output);
    CHECK(!output[0], 8);
    game_audio_settings(100, 0, 1);
    game_audio_test_step(output);
    CHECK(output[0] == 2000, 9);
    game_audio_test_step(output);
    game_audio_test_step(output);
    CHECK(output[0] == 1000, 10);
    game_audio_play(-1, 14, 0, 0);
    game_audio_test_step(output);
    CHECK(!game_audio_errors(), 11);
    CHECK(opens == 3 && reads == 3 && seeks == 6, 13);
    unsigned char letters[] = {1, 2, 3, 4, 5}, out[8] = {0};
    AudioMemory memory = {letters, 5, 0};
    CHECK(audio_memory_read(out, 2, 4, &memory) == 2 && memory.position == 4 && out[0] == 1 &&
              out[3] == 4,
          14);
    CHECK(!audio_memory_seek(&memory, -2, SEEK_END) && audio_memory_tell(&memory) == 3, 15);
    CHECK(audio_memory_seek(&memory, 3, SEEK_CUR) < 0 && memory.position == 3, 16);
    CHECK(!audio_memory_seek(&memory, 0, SEEK_SET) && audio_memory_read(out, 1, 8, &memory) == 5 &&
              out[4] == 5,
          17);
    CHECK(!audio_memory_read(out, 0, 1, &memory) && !audio_memory_read(out, 1, (size_t)-1, &memory),
          18);
    CHECK(audio_memory_seek(&memory, -1, SEEK_SET) < 0 && !audio_memory_close(&memory), 19);
    game_audio_stop();
    for (int i = 0; i < 3; ++i)
        CHECK(!allocated[i], 20);
    unsigned initial_opens = opens;
    fail_loader = 1;
    CHECK(!game_audio_start() && !game_audio_test_prepare(&title, 1, 0), 30);
    game_audio_preload_gameplay(0);
    CHECK(opens == initial_opens + 1 && !game_audio_ready(), 31);
    game_audio_settings(100, 0, 1);
    game_audio_play(35, 0, 1, 100);
    game_audio_test_step(output);
    CHECK(output[0] == 1000 && game_audio_started(0), 32);
    game_audio_preload_gameplay(1);
    CHECK(opens > initial_opens + 1 && game_audio_ready(), 33);
    game_audio_stop();
    return 0;
}
