/* One file-reader thread owns disk I/O. Render submission never calls fread.
 * A READY -> CLAIMED transition transfers ownership of the pixel buffer.
 * RAM staging has its own bound, independent of the GPU texture cache. */
#include "asset_stream.h"
#include "data_files.h"
#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "generated_subtitle_pack.h"
#include "scare_animation.h"
#include "camera_noise.h"
#include "caption_index.h"
enum { EMPTY, LOW, HIGH, FOREGROUND, READING, READY, CLAIMED, FAILED };
typedef struct {
    atomic_int state;
    atomic_uint order;
    AssetBuffer buffer;
} Slot;
static Slot slots[ASSET_SLOTS];
static atomic_int running;
static atomic_uint serial, ram_bytes, errors, read_us, jobs;
static SceUID thread = -1;
enum { CAPTION_COUNT = sizeof(packed_subtitles) / sizeof(*packed_subtitles) };
static CaptionEntry external_captions[CAPTION_COUNT];
static int caption_index_loaded;
static unsigned le16(const unsigned char *p) {
    return p[0] | (unsigned)p[1] << 8;
}
static int pick(int priority) {
    int chosen = -1;
    unsigned oldest = 0;
    for (int i = 0; i < ASSET_SLOTS; ++i)
        if (atomic_load_explicit(&slots[i].state, memory_order_acquire) == priority) {
            unsigned t = atomic_load(&slots[i].order);
            if (chosen < 0 || t < oldest) {
                chosen = i;
                oldest = t;
            }
        }
    return chosen;
}
static int reserve(unsigned bytes) {
    while (atomic_load(&ram_bytes) + bytes > ASSET_RAM_LIMIT) {
        int victim = pick(READY);
        if (victim < 0)
            return -1;
        int expected = READY;
        if (!atomic_compare_exchange_strong(&slots[victim].state, &expected, CLAIMED))
            continue;
        free(slots[victim].buffer.pixels);
        atomic_fetch_sub(&ram_bytes, slots[victim].buffer.bytes);
        memset(&slots[victim].buffer, 0, sizeof(AssetBuffer));
        atomic_store_explicit(&slots[victim].state, EMPTY, memory_order_release);
    }
    return 0;
}
static int load_caption_index(void) {
    if (caption_index_loaded)
        return 0;
    unsigned bytes = 12 + CAPTION_COUNT * 12;
    unsigned char *raw = malloc(bytes);
    if (!raw)
        return -2;
    FILE *f = fopen(FNAF_DATA_ROOT "ui/subtitles.idx", "rb");
    int ok = f && fread(raw, 1, bytes, f) == bytes &&
             !caption_index_decode(raw, bytes, external_captions, CAPTION_COUNT);
    if (f)
        fclose(f);
    free(raw);
    if (!ok)
        return -1;
    caption_index_loaded = 1;
    return 0;
}
static int read_caption(int index, AssetBuffer *b) {
    int loaded = load_caption_index();
    if (loaded)
        return loaded;
    const CaptionEntry *c = &external_captions[index];
    b->width = c->width;
    b->height = c->height;
    b->bytes = b->width * b->height * 4;
    if (!b->width || !b->height || b->bytes > ASSET_RAM_LIMIT)
        return -1;
    if (reserve(b->bytes))
        return -2;
    unsigned char *compressed = malloc(c->size), *mask = malloc(b->width * b->height);
    b->pixels = malloc(b->bytes);
    if (!compressed || !mask || !b->pixels) {
        free(compressed);
        free(mask);
        free(b->pixels);
        b->pixels = NULL;
        return -2;
    }
    FILE *f = fopen(FNAF_DATA_ROOT "ui/subtitles.pack", "rb");
    int result = -1;
    uLongf length = b->width * b->height;
    long packed_bytes = -1;
    if (f && !fseek(f, 0, SEEK_END))
        packed_bytes = ftell(f);
    if (packed_bytes >= 0 && c->offset <= (unsigned long)packed_bytes &&
        c->size <= (unsigned long)packed_bytes - c->offset &&
        !fseek(f, (long)c->offset, SEEK_SET) && fread(compressed, 1, c->size, f) == c->size &&
        uncompress(mask, &length, compressed, c->size) == Z_OK && length == b->width * b->height) {
        for (unsigned i = 0; i < length; ++i) {
            unsigned a = 223 + (32u * mask[i] + 127) / 255;
            /* Straight-alpha color for white glyph coverage on a black panel. */
            unsigned char value = (unsigned char)((mask[i] * 255u + a / 2) / a);
            b->pixels[i * 4] = b->pixels[i * 4 + 1] = b->pixels[i * 4 + 2] = value;
            b->pixels[i * 4 + 3] = (unsigned char)a;
        }
        result = 0;
    }
    if (f)
        fclose(f);
    free(compressed);
    free(mask);
    if (result) {
        free(b->pixels);
        b->pixels = NULL;
    }
    return result;
}
static int read_asset(int handle, AssetBuffer *b) {
    if (handle >= 1024 &&
        (unsigned)(handle - 1024) < sizeof(packed_subtitles) / sizeof(*packed_subtitles))
        return read_caption(handle - 1024, b);
    char path[96];
    if (handle >= 7000)
        snprintf(path, sizeof(path), FNAF_DATA_ROOT "ui/%04d.rgba", handle);
    else
        snprintf(path, sizeof(path), FNAF_DATA_ROOT "%s/%04d.rgba",
                 handle >= 1024 ? "subtitles" : "sprites", handle);
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    setvbuf(f, NULL, _IOFBF, 64 * 1024);
    unsigned char h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "FSPI", 4))
        goto failed;
    b->width = le16(h + 4);
    b->height = le16(h + 6);
    if (!b->width || b->width > 4096 || !b->height || b->height > 2048)
        goto failed;
    unsigned source_width = b->width, source_height = b->height,
             source_bytes = source_width * source_height * 4;
    if (source_bytes > ASSET_RAM_LIMIT)
        goto failed;
    scare_texture_size(handle, &b->width, &b->height);
    b->bytes = b->width * b->height * texture_pixel_bytes(handle);
    if (b->bytes > ASSET_RAM_LIMIT)
        goto failed;
    if (reserve(b->bytes)) {
        fclose(f);
        return -2;
    }
    b->pixels = malloc(source_bytes);
    if (!b->pixels) {
        fclose(f);
        return -2;
    }
    if (fread(b->pixels, 1, source_bytes, f) != source_bytes) {
        free(b->pixels);
        b->pixels = NULL;
        goto failed;
    }
    if (camera_noise_image(handle)) {
        unsigned char *grey = malloc(b->bytes);
        if (!grey) {
            free(b->pixels);
            b->pixels = NULL;
            fclose(f);
            return -2;
        }
        for (unsigned i = 0; i < b->bytes; ++i) {
            const unsigned char *p = b->pixels + i * 4;
            grey[i] = (unsigned char)((p[0] * 77u + p[1] * 150u + p[2] * 29u + 128) / 256);
        }
        free(b->pixels);
        b->pixels = grey;
    } else if (source_width != b->width || source_height != b->height) {
        unsigned char *smaller = malloc(b->bytes);
        if (!smaller) {
            free(b->pixels);
            b->pixels = NULL;
            fclose(f);
            return -2;
        }
        /* Bilinear reduction on the I/O worker; never on the render thread. */
        for (unsigned y = 0; y < b->height; ++y) {
            unsigned fy = (2 * y + 1) * source_height * 128 / b->height - 128, sy = fy >> 8,
                     wy = fy & 255;
            unsigned next_y = sy + 1 < source_height ? sy + 1 : sy;
            for (unsigned x = 0; x < b->width; ++x) {
                unsigned fx = (2 * x + 1) * source_width * 128 / b->width - 128, sx = fx >> 8,
                         wx = fx & 255;
                unsigned next_x = sx + 1 < source_width ? sx + 1 : sx;
                const unsigned char *a = b->pixels + (sy * source_width + sx) * 4,
                                    *c = b->pixels + (next_y * source_width + sx) * 4;
                const unsigned char *d = b->pixels + (sy * source_width + next_x) * 4,
                                    *e = b->pixels + (next_y * source_width + next_x) * 4;
                for (unsigned ch = 0; ch < 4; ++ch) {
                    unsigned top = a[ch] * (256 - wx) + d[ch] * wx,
                             bottom = c[ch] * (256 - wx) + e[ch] * wx;
                    smaller[(y * b->width + x) * 4 + ch] =
                        (unsigned char)((top * (256 - wy) + bottom * wy + 32768) >> 16);
                }
            }
        }
        free(b->pixels);
        b->pixels = smaller;
    }
    fclose(f);
    return 0;
failed:
    fclose(f);
    return -1;
}
static int process_one(void) {
    if (!atomic_load_explicit(&jobs, memory_order_acquire))
        return 0;
    int handle = pick(FOREGROUND);
    if (handle < 0)
        handle = pick(HIGH);
    if (handle < 0)
        handle = pick(LOW);
    if (handle < 0)
        return 0;
    int expected = atomic_load(&slots[handle].state);
    if ((expected != FOREGROUND && expected != HIGH && expected != LOW) ||
        !atomic_compare_exchange_strong(&slots[handle].state, &expected, READING))
        return 1;
    uint64_t start = sceKernelGetSystemTimeWide();
    AssetBuffer b = {0};
    int result = read_asset(handle, &b);
    atomic_store(&read_us, (unsigned)(sceKernelGetSystemTimeWide() - start));
    if (result == -2) {
        atomic_store_explicit(&slots[handle].state, expected, memory_order_release);
        sceKernelDelayThread(2000);
    } else if (result) {
        atomic_fetch_add(&errors, 1);
        atomic_store_explicit(&slots[handle].state, FAILED, memory_order_release);
    } else {
        slots[handle].buffer = b;
        atomic_fetch_add(&ram_bytes, b.bytes);
        atomic_store_explicit(&slots[handle].state, READY, memory_order_release);
    }
    if (result != -2)
        atomic_fetch_sub(&jobs, 1);
    return 1;
}
static int worker(SceSize n, void *p) {
    (void)n;
    (void)p;
    while (atomic_load(&running))
        if (!process_one())
            sceKernelDelayThread(2000);
    return 0;
}
#ifdef ASSET_STREAM_TEST
int asset_stream_test_step(void) {
    return process_one();
}
#endif
int asset_stream_start(void) {
    if (thread >= 0)
        return 0;
    atomic_store(&running, 1);
    thread = sceKernelCreateThread("FnafAssetReader", worker, 0x10000110, 128 * 1024, 0,
                                   SCE_KERNEL_CPU_MASK_USER_2, NULL);
    if (thread < 0)
        thread =
            sceKernelCreateThread("FnafAssetReader", worker, 0x10000110, 128 * 1024, 0, 0, NULL);
    if (thread < 0)
        return -1;
    if (sceKernelStartThread(thread, 0, NULL) < 0) {
        sceKernelDeleteThread(thread);
        thread = -1;
        return -1;
    }
    return 0;
}
void asset_stream_stop(void) {
    atomic_store(&running, 0);
    if (thread >= 0) {
        sceKernelWaitThreadEnd(thread, NULL, NULL);
        sceKernelDeleteThread(thread);
        thread = -1;
    }
    for (int i = 0; i < ASSET_SLOTS; ++i) {
        free(slots[i].buffer.pixels);
        memset(&slots[i].buffer, 0, sizeof(AssetBuffer));
        atomic_store(&slots[i].state, EMPTY);
    }
    atomic_store(&ram_bytes, 0);
    atomic_store(&jobs, 0);
    caption_index_loaded = 0;
}
void asset_stream_request(int handle, int urgent) {
    if (handle < 0 || handle >= ASSET_SLOTS)
        return;
    Slot *s = &slots[handle];
    int state = atomic_load_explicit(&s->state, memory_order_acquire);
    int target = urgent > 1 ? FOREGROUND : urgent ? HIGH : LOW;
    if (state == EMPTY) {
        atomic_store(&s->order, atomic_fetch_add(&serial, 1));
        int expected = EMPTY;
        atomic_fetch_add(&jobs, 1);
        if (!atomic_compare_exchange_strong_explicit(&s->state, &expected, target,
                                                     memory_order_release, memory_order_relaxed))
            atomic_fetch_sub(&jobs, 1);
    } else if ((state == LOW || state == HIGH) && target > state) {
        int expected = state;
        atomic_compare_exchange_strong(&s->state, &expected, target);
    } else if (urgent && state == READY)
        atomic_store(&s->order, atomic_fetch_add(&serial, 1));
}
void asset_stream_retain_requests(const unsigned char wanted[ASSET_SLOTS]) {
    for (int i = 0; i < ASSET_SLOTS; ++i) {
        int expected = atomic_load(&slots[i].state);
        if (expected != LOW && expected != HIGH && expected != FOREGROUND)
            continue;
        int target = !wanted[i] ? EMPTY : wanted[i] == 4 ? FOREGROUND : wanted[i] == 2 ? HIGH : LOW;
        if (target != expected &&
            atomic_compare_exchange_strong(&slots[i].state, &expected, target) && target == EMPTY)
            atomic_fetch_sub(&jobs, 1);
    }
}
int asset_stream_take(int handle, AssetBuffer *b) {
    if (handle < 0 || handle >= ASSET_SLOTS)
        return 0;
    Slot *s = &slots[handle];
    int expected = READY;
    if (!atomic_compare_exchange_strong_explicit(&s->state, &expected, CLAIMED,
                                                 memory_order_acquire, memory_order_relaxed))
        return 0;
    *b = s->buffer;
    memset(&s->buffer, 0, sizeof(AssetBuffer));
    atomic_fetch_sub(&ram_bytes, b->bytes);
    atomic_store_explicit(&s->state, EMPTY, memory_order_release);
    return 1;
}
void asset_stream_release(AssetBuffer *b) {
    free(b->pixels);
    memset(b, 0, sizeof(*b));
}
unsigned asset_stream_pending(void) {
    return atomic_load(&jobs);
}
unsigned asset_stream_errors(void) {
    return atomic_load(&errors);
}
unsigned asset_stream_read_us(void) {
    return atomic_load(&read_us);
}
