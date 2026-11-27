/* GXM sprite rendering. Texture uploads happen before draw submission, and
 * textures are never freed while submitted GPU work still references them. */
#include "gpu_renderer.h"
#include "scene_engine.h"
#include "data_files.h"
#include "generated_native_font.h"
#include "asset_stream.h"
#include "perspective_mesh.h"
#include "scare_animation.h"
#include "sprite_atlas.h"
#include "camera_noise.h"
#include <vita2d.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SLOT_COUNT = ASSET_SLOTS,
    CACHE_LIMIT = 64 * 1024 * 1024,
    FRAME_POOL = 3 * 1024 * 1024,
    FRAME_RESERVE = 256 * 1024
};
typedef struct {
    vita2d_texture *texture;
    unsigned bytes, last;
} Cached;
static Cached cache[SLOT_COUNT];
static unsigned char needed[SLOT_COUNT];
static unsigned total_bytes, serial, errors;
static vita2d_texture *font;
typedef struct {
    int handle, perspective;
    float x, y, w, h;
    unsigned color;
} Draw;
static Draw commands[96];
static int command_count, display_mode;
static int gpu_idle = 1;
static GpuStats current_stats, last_stats;
_Static_assert(sizeof(PerspectiveVertex) == sizeof(vita2d_texture_vertex), "GPU vertex layout");
static void sync_gpu(void) {
    if (!gpu_idle) {
        uint64_t t = sceKernelGetSystemTimeWide();
        vita2d_wait_rendering_done();
        current_stats.sync_us += (unsigned)(sceKernelGetSystemTimeWide() - t);
        gpu_idle = 1;
    }
}

static void release(int handle) {
    total_bytes -= cache[handle].bytes;
    vita2d_free_texture(cache[handle].texture);
    memset(&cache[handle], 0, sizeof(cache[handle]));
}

static int oldest_unused(int allow_warm) {
    /* Future scare frames yield to live controls/camera frames. Warming may
     * evict old unneeded pictures, but cannot displace another warm frame. */
    int oldest = -1;
    for (int i = 0; i < SLOT_COUNT; ++i)
        if (cache[i].texture && needed[i] < 2 && (oldest < 0 || cache[i].last < cache[oldest].last))
            oldest = i;
    if (oldest < 0 && allow_warm)
        for (int i = 0; i < SLOT_COUNT; ++i)
            if (cache[i].texture && needed[i] == 3 &&
                (oldest < 0 || cache[i].last < cache[oldest].last))
                oldest = i;
    return oldest;
}
static unsigned estimate_bytes(int handle) {
    const SpriteAtlasPage *page = sprite_atlas_page(handle);
    unsigned w, h;
    if (page) {
        w = (unsigned)page->width;
        h = (unsigned)page->height;
    } else {
        const ImageDef *m = scene_image(handle);
        if (!m)
            return 0;
        w = (unsigned)(m->width * 3 + 2) / 4;
        h = (unsigned)(m->height * 3 + 2) / 4;
        animation_texture_size(handle, &w, &h);
    }
    return ((((w + 7) & ~7u) * texture_pixel_bytes(handle) * h) + 0x3ffffu) & ~0x3ffffu;
}
static int upload_ready(int handle, int priority) {
    AssetBuffer b = {0};
    if (!asset_stream_take(handle, &b))
        return 0;
    unsigned pixel_bytes = texture_pixel_bytes(handle);
    unsigned bytes = ((((b.width + 7) & ~7u) * pixel_bytes * b.height) + 0x3ffffu) & ~0x3ffffu;
    if (priority == 1 && total_bytes + bytes > CACHE_LIMIT) {
        asset_stream_release(&b);
        return 0;
    }
    sync_gpu();
    uint64_t start = sceKernelGetSystemTimeWide();
    int urgent = priority == 2 || priority == 4;
    while (total_bytes + bytes > CACHE_LIMIT) {
        int old = oldest_unused(urgent);
        if (old < 0) {
            asset_stream_release(&b);
            if (urgent)
                ++errors;
            return 0;
        }
        release(old);
        ++current_stats.evictions;
    }
    vita2d_texture *texture =
        camera_noise_image(handle)
            ? vita2d_create_empty_texture_format(b.width, b.height, SCE_GXM_TEXTURE_FORMAT_L8)
            : vita2d_create_empty_texture(b.width, b.height);
    /* A tighter device allocation limit may be below our logical budget. */
    while (!texture) {
        int old = oldest_unused(urgent);
        if (old < 0)
            break;
        release(old);
        ++current_stats.evictions;
        texture =
            camera_noise_image(handle)
                ? vita2d_create_empty_texture_format(b.width, b.height, SCE_GXM_TEXTURE_FORMAT_L8)
                : vita2d_create_empty_texture(b.width, b.height);
    }
    if (!texture) {
        asset_stream_release(&b);
        if (urgent)
            ++errors;
        return 0;
    }
    texture->gxm_rtgt = NULL;
    texture->depth_UID = 0;
    unsigned stride = vita2d_texture_get_stride(texture);
    unsigned char *pixels = vita2d_texture_get_datap(texture);
    if (stride == b.width * pixel_bytes)
        memcpy(pixels, b.pixels, b.bytes);
    else
        for (unsigned y = 0; y < b.height; ++y)
            memcpy(pixels + stride * y, b.pixels + b.width * pixel_bytes * y,
                   b.width * pixel_bytes);
    vita2d_texture_set_filters(texture, SCE_GXM_TEXTURE_FILTER_LINEAR,
                               SCE_GXM_TEXTURE_FILTER_LINEAR);
    cache[handle] = (Cached){texture, bytes, serial};
    total_bytes += bytes;
    ++current_stats.loads;
    current_stats.upload_bytes += b.bytes;
    asset_stream_release(&b);
    current_stats.upload_us += (unsigned)(sceKernelGetSystemTimeWide() - start);
    return 1;
}

int gpu_init(void) {
    if (vita2d_init_advanced(FRAME_POOL) != 1)
        return -1;
    vita2d_set_clear_color(0xff000000u);
    vita2d_texture_set_alloc_memblock_type(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW);
    font = vita2d_create_empty_texture(FONT_W, FONT_H);
    if (!font)
        return -1;
    font->gxm_rtgt = NULL;
    font->depth_UID = 0;
    uint32_t *pixels = vita2d_texture_get_datap(font);
    unsigned stride = vita2d_texture_get_stride(font) / 4;
    unsigned position = 0;
    for (unsigned r = 0; r < sizeof(font_runs) / sizeof(*font_runs); ++r)
        for (unsigned n = 0; n < font_runs[r].count; ++n, ++position)
            pixels[(position / FONT_W) * stride + position % FONT_W] =
                0x00ffffffu | ((unsigned)font_runs[r].alpha << 24);
    vita2d_texture_set_filters(font, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);
    return asset_stream_start();
}

void gpu_reset_scene(void) {
    sync_gpu();
    for (int i = 0; i < SLOT_COUNT; ++i)
        if (cache[i].texture)
            release(i);
    memset(cache, 0, sizeof(cache));
    memset(needed, 0, sizeof(needed));
    errors = 0;
}
void gpu_shutdown(void) {
    gpu_reset_scene();
    asset_stream_stop();
    vita2d_free_texture(font);
    font = NULL;
    vita2d_fini();
}
void gpu_prepare(void) {
    sync_gpu();
    ++serial;
    memset(needed, 0, sizeof(needed));
    for (int i = 0; i < runtime.count; ++i) {
        Instance *instance = &runtime.instances[i];
        if (!instance->create || !instance->visible || instance->layer < 0 ||
            instance->layer >= 32 || !runtime.layer_visible[instance->layer] ||
            instance->alpha == 255)
            continue;
        int handle = scene_instance_image(instance);
        const ImageDef *image = scene_image(handle);
        if (!image || handle < 0 || handle >= SLOT_COUNT)
            continue;
        int x, y, w, h;
        scene_geometry(instance, image, &x, &y, &w, &h);
        if (x < 960 && y < 544 && x + w > 0 && y + h > 0)
            needed[sprite_texture_handle(handle)] = 2;
    }
    gpu_queue_prepare();
}
void gpu_warm_title(void) {
    if (!runtime.scene || runtime.scene->index != 3)
        return;
    memset(needed, 0, sizeof(needed));
    /* Cache the full noise animation and Freddy variants before playback. */
    for (int i = 0; i < runtime.count; ++i) {
        const ObjectDef *object = runtime.instances[i].definition;
        if (object->handle != 9 && object->handle != 10)
            continue;
        for (int a = 0; a < object->animation_count; ++a)
            for (int f = 0; f < object->animations[a].count; ++f)
                gpu_queue_prefetch(object->animations[a].images[f]);
    }
}
void gpu_begin(void) {
    /* Append to the pool while old frames are in flight. Reuse only after a
     * fence; vita2d_start_drawing() would reset the shared pool every frame. */
    if (gpu_idle || vita2d_pool_free_space() < FRAME_RESERVE) {
        sync_gpu();
        vita2d_pool_reset();
    }
    vita2d_start_drawing_advanced(NULL, 0);
    vita2d_clear_screen();
}
void gpu_end(void) {
    uint64_t t = sceKernelGetSystemTimeWide();
    vita2d_end_drawing();
    vita2d_swap_buffers();
    gpu_idle = 0;
    current_stats.present_us = (unsigned)(sceKernelGetSystemTimeWide() - t);
    last_stats = current_stats;
}
void gpu_rectangle(int x, int y, int width, int height, uint32_t color) {
    float fx = x, fw = width;
    if (display_mode == 2) {
        fx = 117 + x * .75625f;
        fw *= .75625f;
    }
    vita2d_draw_rectangle(fx, (float)y, fw, (float)height, color);
}
void gpu_check_badge(int x, int y) {
    float scale = display_mode == 2 ? .75625f : 1.0f;
    float cx = display_mode == 2 ? 117 + x * scale : (float)x;
    vita2d_draw_fill_circle(cx, (float)y, 9 * scale, 0xffffffffu);
    for (int t = 0; t < 2; ++t) {
        float cy = (float)y + t;
        vita2d_draw_line(cx - 4 * scale, cy, cx - scale, cy + 3 * scale, 0xff171717u);
        vita2d_draw_line(cx - scale, cy + 3 * scale, cx + 5 * scale, cy - 4 * scale,
                         0xff171717u);
    }
}
void gpu_text(int x, int y, const char *value, uint32_t color) {
    int origin = x;
    while (*value) {
        unsigned char ch = (unsigned char)*value++;
        unsigned glyph;
        if (ch == '\n') {
            x = origin;
            y += FONT_CELL_H;
            continue;
        }
        if (ch == 0xc2 && (unsigned char)*value == 0xa9) {
            ++value;
            glyph = 95;
        } else if (ch >= 32 && ch <= 126)
            glyph = ch - 32;
        else {
            while (((unsigned char)*value & 0xc0) == 0x80)
                ++value;
            glyph = '?' - 32;
        }
        float left = (glyph % 32) * FONT_CELL_W, top = (glyph / 32) * FONT_CELL_H;
        if (display_mode == 2)
            vita2d_draw_texture_tint_part_scale(font, 117 + x * .75625f, (float)y, left, top,
                                                FONT_CELL_W, FONT_CELL_H, .75625f, 1, color);
        else
            vita2d_draw_texture_tint_part(font, (float)x, (float)y, left, top, FONT_CELL_W,
                                          FONT_CELL_H, color);
        x += FONT_ADVANCE;
    }
}
static void draw_sprite(int handle, float x, float y, float w, float h, unsigned color,
                        int perspective) {
    const SpriteAtlasItem *part = sprite_atlas_item(handle);
    vita2d_texture *t = cache[sprite_texture_handle(handle)].texture;
    if (!t)
        return;
    float tw = vita2d_texture_get_width(t), th = vita2d_texture_get_height(t);
    if (!perspective) {
        if (part)
            vita2d_draw_texture_tint_part_scale(t, x, y, (float)part->x, (float)part->y,
                                                (float)part->width, (float)part->height,
                                                w / part->width, h / part->height, color);
        else
            vita2d_draw_texture_tint_scale(t, x, y, w / tw, h / th, color);
    } else {
        PerspectiveVertex *v = vita2d_pool_memalign(sizeof(*v) * PERSPECTIVE_VERTICES, 4);
        if (!v) {
            ++errors;
            return;
        }
        unsigned count = perspective_mesh(v, x, y, w, h, display_mode == 2 ? 363.0f : 480.0f);
        if (!count)
            return;
        if (part)
            for (unsigned i = 0; i < count; ++i) {
                v[i].u = (part->x + v[i].u * part->width) / tw;
                v[i].v = (part->y + v[i].v * part->height) / th;
            }
        vita2d_draw_array_textured(t, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP,
                                   (const vita2d_texture_vertex *)v, count, color);
    }
    ++current_stats.draw_calls;
}
void gpu_draw_scene(void) {
    vita2d_set_clip_rectangle(0, 2, 960, 542);
    vita2d_enable_clipping();
    for (int layer = 0; layer < 32; ++layer) {
        if (!runtime.layer_visible[layer])
            continue;
        for (int i = 0; i < runtime.count; ++i) {
            const Instance *instance = &runtime.instances[i];
            if (!instance->create || !instance->visible || instance->layer != layer ||
                instance->alpha == 255)
                continue;
            int handle = scene_instance_image(instance);
            const ImageDef *image = scene_image(handle);
            if (!image || handle < 0 || handle >= SLOT_COUNT ||
                !cache[sprite_texture_handle(handle)].texture)
                continue;
            int x, y, w, h;
            scene_geometry(instance, image, &x, &y, &w, &h);
            unsigned color = 0x00ffffffu | (unsigned)(255 - instance->alpha) << 24;
            draw_sprite(handle, (float)x, (float)y, (float)w, (float)h, color, 0);
        }
    }
    vita2d_disable_clipping();
}
unsigned gpu_asset_errors(void) {
    return errors + asset_stream_errors();
}
int gpu_texture_ready(int handle) {
    return handle >= 0 && handle < SLOT_COUNT &&
           cache[sprite_texture_handle(handle)].texture != NULL;
}
int gpu_texture_dimensions(int handle, unsigned *width, unsigned *height) {
    if (!gpu_texture_ready(handle))
        return 0;
    const SpriteAtlasItem *part = sprite_atlas_item(handle);
    vita2d_texture *t = cache[sprite_texture_handle(handle)].texture;
    *width = part ? (unsigned)part->width : vita2d_texture_get_width(t);
    *height = part ? (unsigned)part->height : vita2d_texture_get_height(t);
    return 1;
}
void gpu_get_stats(GpuStats *s) {
    *s = last_stats;
    s->cache_bytes = total_bytes;
    s->pending = asset_stream_pending();
    s->read_us = asset_stream_read_us();
}
void gpu_queue_reset(int display) {
    command_count = 0;
    display_mode = display;
    ++serial;
    memset(needed, 0, sizeof(needed));
    memset(&current_stats, 0, sizeof(current_stats));
}
void gpu_queue_discard_draws(void) {
    /* Keep the texture requests so a hidden screen can be prepared ahead of time. */
    command_count = 0;
}
void gpu_queue_image(int handle, float x, float y, float w, float h, unsigned color,
                     int perspective) {
    if (handle < 0 || handle >= SLOT_COUNT || command_count >= 96)
        return;
    if (x >= 960 || y >= 544 || x + w <= 0 || y + h <= 0)
        return;
    commands[command_count++] = (Draw){handle, perspective, x, y, w, h, color};
    handle = sprite_texture_handle(handle);
    if (needed[handle] != 4)
        needed[handle] = 2;
}
void gpu_queue_prefetch(int handle) {
    if (handle >= 0 && handle < SLOT_COUNT) {
        handle = sprite_texture_handle(handle);
        if (!needed[handle])
            needed[handle] = 1;
    }
}
void gpu_queue_resident(int handle) {
    if (handle >= 0 && handle < SLOT_COUNT) {
        handle = sprite_texture_handle(handle);
        if (needed[handle] != 4)
            needed[handle] = 2;
    }
}
void gpu_queue_focus(int handle) {
    if (handle >= 0 && handle < SLOT_COUNT)
        needed[sprite_texture_handle(handle)] = 4;
}
void gpu_queue_warm(int handle) {
    if (handle >= 0 && handle < SLOT_COUNT) {
        handle = sprite_texture_handle(handle);
        if (needed[handle] != 2 && needed[handle] != 4)
            needed[handle] = 3;
    }
}
void gpu_queue_prepare(void) {
    uint64_t t = sceKernelGetSystemTimeWide();
    asset_stream_retain_requests(needed);
    for (int i = 0; i < SLOT_COUNT; ++i)
        if (needed[i]) {
            if (cache[i].texture) {
                cache[i].last = serial;
                continue;
            }
            if (needed[i] == 3 && total_bytes + estimate_bytes(i) > CACHE_LIMIT &&
                oldest_unused(0) < 0)
                continue;
            if (needed[i] >= 2 || total_bytes + estimate_bytes(i) <= CACHE_LIMIT)
                asset_stream_request(i, needed[i] == 4 ? 2 : needed[i] == 2 ? 1 : 0);
        }
    const int order[] = {4, 2, 3, 1};
    for (int pass = 0; pass < 4; ++pass) {
        int priority = order[pass];
        for (int i = 0; i < SLOT_COUNT; ++i)
            if (needed[i] == priority && !cache[i].texture) {
                if (priority == 1 &&
                    (current_stats.loads >= 2 || total_bytes + estimate_bytes(i) > CACHE_LIMIT))
                    continue;
                if (priority == 3 &&
                    (current_stats.loads >= 2 ||
                     (total_bytes + estimate_bytes(i) > CACHE_LIMIT && oldest_unused(0) < 0)))
                    continue;
                unsigned estimate = estimate_bytes(i);
                if (current_stats.upload_bytes &&
                    current_stats.upload_bytes + estimate > 4 * 1024 * 1024)
                    continue;
                (void)upload_ready(i, priority);
            }
    }
    current_stats.prepare_us = (unsigned)(sceKernelGetSystemTimeWide() - t);
}
int gpu_queue_draw_ready(void) {
    for (int i = 0; i < command_count; ++i)
        if (!gpu_texture_ready(commands[i].handle))
            return 0;
    return 1;
}
void gpu_queue_draw(void) {
    uint64_t start = sceKernelGetSystemTimeWide();
    int left = display_mode == 2 ? 117 : 0, right = display_mode == 2 ? 843 : 960;
    vita2d_set_clip_rectangle(left, 2, right, 542);
    vita2d_enable_clipping();
    for (int i = 0; i < command_count; ++i) {
        Draw *d = &commands[i];
        float x = d->x, y = d->y, w = d->w, h = d->h;
        if (display_mode == 2) {
            x = 117 + x * .75625f;
            w *= .75625f;
        } else if (display_mode == 1) {
            y = (y - 2) * 544 / 540;
            h *= 544.0f / 540;
        }
        draw_sprite(d->handle, x, y, w, h, d->color, d->perspective);
    }
    vita2d_disable_clipping();
    current_stats.draw_us = (unsigned)(sceKernelGetSystemTimeWide() - start);
}
