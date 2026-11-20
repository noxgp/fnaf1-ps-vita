/* Execute the actual renderer against a deliberately asynchronous GPU model.
 * Checks pool lifetimes, upload fencing, cache protection and submission count.
 * This does not emulate GXM rasterization or measure device frame rate. */
#include "gpu_renderer.h"
#include "asset_stream.h"
#include "scene_engine.h"
#include "perspective_mesh.h"
#include "sprite_atlas.h"
#include "camera_noise.h"
#include <vita2d.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef struct {
    vita2d_texture t;
    unsigned w, h;
    int alive, luminance;
    unsigned char pixels[256];
} FakeTexture;
static FakeTexture textures[300];
static unsigned char font_pixels[512 * 72 * 4], pool[3 * 1024 * 1024], source[64];
static int text_parts, text_positions[8][4];
static unsigned char atlas_pixels[1024 * 512 * 4], atlas_source[1024 * 512 * 4];
static float part_draw[8];
static unsigned requested[ASSET_SLOTS], taken[ASSET_SLOTS], ready[ASSET_SLOTS];
static unsigned used, resets, waits, draws, arrays, bad, inflight, force_low;
static const unsigned char *protected_data;
static unsigned protected_size, protected_hash;
static unsigned checksum(const unsigned char *p, unsigned n) {
    unsigned h = 2166136261u;
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}
static FakeTexture *tex(const vita2d_texture *t) {
    return (FakeTexture *)t;
}
int vita2d_init_advanced(unsigned n) {
    return n == sizeof(pool) ? 1 : 0;
}
void vita2d_set_clear_color(unsigned c) {
    (void)c;
}
void vita2d_texture_set_alloc_memblock_type(SceKernelMemBlockType t) {
    (void)t;
}
vita2d_texture *vita2d_create_empty_texture(unsigned w, unsigned h) {
    if (inflight)
        ++bad;
    for (int i = 0; i < 300; ++i)
        if (!textures[i].alive) {
            textures[i].alive = 1;
            textures[i].luminance = 0;
            textures[i].w = w;
            textures[i].h = h;
            return &textures[i].t;
        }
    return NULL;
}
vita2d_texture *vita2d_create_empty_texture_format(unsigned w, unsigned h,
                                                   SceGxmTextureFormat format) {
    vita2d_texture *t = vita2d_create_empty_texture(w, h);
    if (t)
        tex(t)->luminance = format == SCE_GXM_TEXTURE_FORMAT_L8;
    return t;
}
void vita2d_free_texture(vita2d_texture *t) {
    if (inflight)
        ++bad;
    tex(t)->alive = 0;
}
void *vita2d_texture_get_datap(const vita2d_texture *t) {
    return tex(t)->w == 1024 ? atlas_pixels : tex(t)->w == 512 ? font_pixels : tex(t)->pixels;
}
unsigned vita2d_texture_get_stride(const vita2d_texture *t) {
    return ((tex(t)->w + 7) & ~7u) * (tex(t)->luminance ? 1 : 4);
}
unsigned vita2d_texture_get_width(const vita2d_texture *t) {
    return tex(t)->w;
}
unsigned vita2d_texture_get_height(const vita2d_texture *t) {
    return tex(t)->h;
}
void vita2d_texture_set_filters(vita2d_texture *t, SceGxmTextureFilter a, SceGxmTextureFilter b) {
    (void)t;
    (void)a;
    (void)b;
}
void vita2d_wait_rendering_done(void) {
    ++waits;
    inflight = 0;
    protected_data = NULL;
}
unsigned vita2d_pool_free_space(void) {
    return force_low ? 0 : (unsigned)sizeof(pool) - used;
}
void vita2d_pool_reset(void) {
    if (inflight)
        ++bad;
    used = 0;
    ++resets;
    force_low = 0;
}
void *vita2d_pool_memalign(unsigned n, unsigned align) {
    used = (used + align - 1) & ~(align - 1);
    if (used + n > sizeof(pool)) {
        ++bad;
        return NULL;
    }
    void *p = pool + used;
    used += n;
    return p;
}
void vita2d_start_drawing_advanced(vita2d_texture *target, unsigned flags) {
    (void)target;
    (void)flags;
    if (protected_data && checksum(protected_data, protected_size) != protected_hash)
        ++bad;
}
void vita2d_clear_screen(void) {
}
void vita2d_end_drawing(void) {
    inflight = 1;
}
void vita2d_swap_buffers(void) {
}
int vita2d_fini(void) {
    return 1;
}
void vita2d_set_clip_rectangle(int a, int b, int c, int d) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
}
void vita2d_enable_clipping(void) {
}
void vita2d_disable_clipping(void) {
}
void vita2d_draw_texture_tint_scale(const vita2d_texture *t, float x, float y, float sx, float sy,
                                    unsigned color) {
    (void)t;
    (void)x;
    (void)y;
    (void)sx;
    (void)sy;
    (void)color;
    ++draws;
}
void vita2d_draw_texture_tint_part(const vita2d_texture *t, float x, float y, float u, float v,
                                   float w, float h, unsigned color) {
    (void)t;
    (void)w;
    (void)h;
    (void)color;
    if (text_parts < 8) {
        int *p = text_positions[text_parts];
        p[0] = (int)x;
        p[1] = (int)y;
        p[2] = (int)u;
        p[3] = (int)v;
    }
    ++text_parts;
}
void vita2d_draw_texture_tint_part_scale(const vita2d_texture *t, float x, float y, float u,
                                         float v, float w, float h, float sx, float sy,
                                         unsigned color) {
    part_draw[0] = u;
    part_draw[1] = v;
    part_draw[2] = w;
    part_draw[3] = h;
    part_draw[4] = sx;
    part_draw[5] = sy;
    vita2d_draw_texture_tint_part(t, x, y, u, v, w, h, color);
}
void vita2d_draw_array_textured(const vita2d_texture *t, SceGxmPrimitiveType mode,
                                const vita2d_texture_vertex *v, size_t n, unsigned color) {
    (void)t;
    (void)color;
    ++arrays;
    if (mode != SCE_GXM_PRIMITIVE_TRIANGLE_STRIP || n != PERSPECTIVE_VERTICES)
        ++bad;
    if ((const unsigned char *)v < pool || (const unsigned char *)(v + n) > pool + sizeof(pool))
        ++bad;
    protected_data = (const unsigned char *)v;
    protected_size = (unsigned)(n * sizeof(*v));
    protected_hash = checksum(protected_data, protected_size);
}
SceInt64 sceKernelGetSystemTimeWide(void) {
    static SceInt64 clock;
    return ++clock;
}
const ImageDef *scene_image(int handle) {
    static ImageDef image;
    image = (ImageDef){handle, 5, 5, 0, 0};
    return &image;
}
int asset_stream_start(void) {
    for (int i = 0; i < 64; ++i)
        source[i] = (unsigned char)i;
    return 0;
}
void asset_stream_stop(void) {
}
void asset_stream_request(int h, int urgent) {
    requested[h] = (unsigned)urgent + 1;
}
int asset_stream_take(int h, AssetBuffer *b) {
    if (!ready[h])
        return 0;
    ready[h] = 0;
    ++taken[h];
    if (sprite_atlas_page(h))
        *b = (AssetBuffer){1024, 512, sizeof(atlas_source), atlas_source};
    else
        *b = (AssetBuffer){4, 4, 16 * texture_pixel_bytes(h), source};
    return 1;
}
void asset_stream_retain_requests(const unsigned char wanted[ASSET_SLOTS]) {
    (void)wanted;
}
void asset_stream_release(AssetBuffer *b) {
    memset(b, 0, sizeof(*b));
}
unsigned asset_stream_errors(void) {
    return 0;
}
unsigned asset_stream_pending(void) {
    return 0;
}
unsigned asset_stream_read_us(void) {
    return 0;
}
static void frame(int h, int perspective) {
    gpu_queue_reset(0);
    gpu_queue_image(h, 0, 2, 960, 540, 0xffffffffu, perspective);
    gpu_queue_prepare();
    gpu_begin();
    gpu_queue_draw();
    gpu_end();
}
#define CHECK(c, n)                                                                                \
    do {                                                                                           \
        if (!(c))                                                                                  \
            return 500 + (n);                                                                      \
    } while (0)
int run_checks(void) {
    CHECK(gpu_init() == 0, 1);
    frame(1, 1);
    CHECK(requested[1] == 2 && !gpu_texture_ready(1) && !waits && !arrays, 2);
    ready[1] = 1;
    frame(1, 1);
    CHECK(gpu_texture_ready(1) && waits == 1 && arrays == 1 && !draws, 3);
    unsigned old_waits = waits, old_resets = resets, old_used = used;
    frame(1, 1);
    CHECK(waits == old_waits && resets == old_resets && used > old_used && arrays == 2, 4);
    CHECK(!bad && taken[1] == 1, 5);
    force_low = 1;
    frame(1, 1);
    CHECK(waits == old_waits + 1 && resets == old_resets + 1 && !bad, 6);
    gpu_queue_reset(0);
    gpu_queue_image(2, 0, 2, 20, 20, 0xffffffffu, 0);
    gpu_queue_prefetch(2);
    gpu_queue_prepare();
    CHECK(requested[2] == 2, 7);
    /* At minimum allocation granularity, 256 textures fill 64 MiB. Keep the
     * office texture demanded on every frame while forcing LRU eviction. */
    for (int i = 4000; i < 4260; ++i) {
        ready[i] = 1;
        gpu_queue_reset(0);
        gpu_queue_image(1, 0, 2, 960, 540, 0xffffffffu, 1);
        gpu_queue_image(i, 10, 10, 20, 20, 0xffffffffu, 0);
        gpu_queue_prepare();
        gpu_begin();
        gpu_queue_draw();
        gpu_end();
        CHECK(gpu_texture_ready(1) && !bad, 8);
    }
    GpuStats s;
    gpu_get_stats(&s);
    CHECK(s.cache_bytes <= 64 * 1024 * 1024 && s.evictions > 0, 9);
    gpu_queue_reset(0);
    gpu_queue_image(1, 0, 2, 960, 540, 0xffffffffu, 1);
    gpu_queue_prefetch(700);
    ready[700] = 1;
    gpu_queue_prepare();
    CHECK(!requested[700] && !taken[700], 10);
    gpu_queue_reset(0);
    gpu_text(10, 20, "Aa\na", 0xffffffffu);
    CHECK(text_parts == 3 && text_positions[0][3] == 24 && text_positions[1][3] == 48, 12);
    CHECK(text_positions[1][0] == 20 && text_positions[2][0] == 10 && text_positions[2][1] == 44,
          13);
    unsigned alpha = 0;
    for (unsigned i = 0; i < sizeof(font_pixels) / 4; ++i)
        alpha += font_pixels[i * 4 + 3];
    CHECK(alpha > 0, 14);
    ready[2] = 1;
    for (int i = 4300; i < 4340; ++i) {
        ready[i] = 1;
        gpu_queue_reset(0);
        gpu_queue_resident(2);
        gpu_queue_image(i, 0, 2, 20, 20, 0xffffffffu, 0);
        gpu_queue_prepare();
        gpu_begin();
        gpu_queue_draw();
        gpu_end();
        CHECK(gpu_texture_ready(2) && !bad, 15);
    }
    gpu_reset_scene();
    gpu_queue_reset(0);
    for (int i = 4000; i <= 4255; ++i) {
        ready[i] = 1;
        gpu_queue_resident(i);
    }
    gpu_queue_prepare();
    CHECK(gpu_texture_ready(4255) && !gpu_asset_errors(), 16);
    gpu_queue_reset(0);
    for (int i = 4000; i <= 4255; ++i)
        gpu_queue_resident(i);
    gpu_queue_warm(900);
    ready[900] = 1;
    gpu_queue_prepare();
    CHECK(!requested[900] && !taken[900] && !gpu_asset_errors(), 17);
    gpu_queue_reset(0);
    for (int i = 4000; i < 4255; ++i)
        gpu_queue_resident(i);
    gpu_queue_warm(4255);
    gpu_queue_resident(901);
    ready[901] = 1;
    gpu_queue_prepare();
    CHECK(gpu_texture_ready(901) && !gpu_texture_ready(4255) && !gpu_asset_errors(), 18);
    for (int i = 4000; i < 4255; ++i)
        CHECK(gpu_texture_ready(i), 19);
    gpu_reset_scene();
    gpu_queue_reset(0);
    const SpriteAtlasItem *plush = sprite_atlas_item(477), *button = sprite_atlas_item(45);
    CHECK(plush && button && sprite_texture_handle(477) == plush->page &&
              sprite_texture_handle(591) == 591,
          20);
    ready[plush->page] = 1;
    gpu_queue_resident(477);
    gpu_queue_prepare();
    CHECK(gpu_texture_ready(477) && taken[plush->page] == 1 && !taken[477], 21);
    gpu_queue_reset(0);
    gpu_queue_image(477, 20, 30, 74, 102, 0xffffffffu, 1);
    gpu_queue_prepare();
    gpu_begin();
    gpu_queue_draw();
    gpu_end();
    const PerspectiveVertex *uv = (const PerspectiveVertex *)protected_data;
    CHECK(uv && uv[0].u == plush->x / 1024.0f && uv[0].v == plush->y / 512.0f, 22);
    CHECK(uv[PERSPECTIVE_VERTICES - 1].u == (plush->x + plush->width) / 1024.0f &&
              uv[PERSPECTIVE_VERTICES - 1].v == (plush->y + plush->height) / 512.0f,
          23);
    ready[button->page] = 1;
    gpu_queue_reset(0);
    gpu_queue_image(45, 20, 30, 100, 150, 0xffffffffu, 0);
    gpu_queue_prepare();
    gpu_begin();
    gpu_queue_draw();
    gpu_end();
    CHECK(part_draw[0] == button->x && part_draw[1] == button->y && part_draw[2] == button->width &&
              part_draw[3] == button->height,
          24);
    CHECK(!bad && !gpu_asset_errors(), 25);
    for (int i = 5000; i < 5270; ++i) {
        ready[i] = 1;
        gpu_queue_reset(0);
        gpu_queue_resident(477);
        gpu_queue_resident(45);
        gpu_queue_image(i, 0, 2, 20, 20, 0xffffffffu, 0);
        gpu_queue_prepare();
        gpu_begin();
        gpu_queue_draw();
        gpu_end();
        CHECK(gpu_texture_ready(477) && gpu_texture_ready(45) && !bad, 26);
    }
    CHECK(taken[plush->page] == 1 && !gpu_asset_errors(), 27);
    gpu_reset_scene();
    gpu_queue_reset(0);
    ready[12] = 1;
    gpu_queue_resident(12);
    gpu_queue_prepare();
    frame(12, 0);
    gpu_get_stats(&s);
    CHECK(gpu_texture_ready(12) && s.cache_bytes == 256 * 1024, 28);
    unsigned luma = 0;
    for (int i = 0; i < 300; ++i)
        if (textures[i].alive && textures[i].luminance)
            ++luma;
    CHECK(luma == 1 && !bad, 29);
    gpu_queue_reset(0);
    gpu_queue_focus(3500);
    gpu_queue_resident(3500);
    gpu_queue_image(3500, 0, 2, 20, 20, 0xffffffffu, 0);
    gpu_queue_prepare();
    CHECK(requested[3500] == 3, 30);
    unsigned dw = 0, dh = 0;
    CHECK(gpu_texture_dimensions(12, &dw, &dh) && dw == 4 && dh == 4 &&
              !gpu_texture_dimensions(3500, &dw, &dh),
          31);
    gpu_shutdown();
    CHECK(!bad && !gpu_asset_errors(), 11);
    return 0;
}
