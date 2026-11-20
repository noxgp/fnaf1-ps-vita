#include "game_view.h"
#include "gpu_renderer.h"
#include "scene_engine.h"
#include "android_menu.h"
#include "scare_animation.h"
#include "generated_game_text.h"
#include "camera_map.h"
#include "game_events.h"
#include "generated_controls.h"
#include "monitor_animation.h"
#include "camera_noise.h"
#include <stdio.h>
#include <string.h>
static unsigned clock_time;
static void image(int handle, float x, float y, float w, float h, unsigned color, int perspective) {
    gpu_queue_image(handle, x, y, w, h, color, perspective);
}
static void screen(int handle, unsigned color) {
    image(handle, 0, 2, 960, 540, color, 0);
}
static void art(int object, int animation, int frame, float x, float y, float pan, float scale,
                int perspective) {
    int handle = scene_art(object, animation, frame);
    const ImageDef *im = scene_image(handle);
    if (!im)
        return;
    image(handle, (x - pan - im->hotspot_x * scale) * .75f, 2 + (y - im->hotspot_y * scale) * .75f,
          im->width * scale * .75f, im->height * scale * .75f, 0xffffffffu, perspective);
}
static void forced(const Game *g, const char *key, int step) {
    if (!g->save.options.forced)
        return;
    if (*key == '/')
        ++key;
    for (unsigned i = 0; i < sizeof(forced_cues) / sizeof(*forced_cues); ++i) {
        const ForcedCue *c = &forced_cues[i];
        const char *actual = c->key;
        if (*actual == '/')
            ++actual;
        if (c->enabled && c->language == g->save.options.language && c->step == step &&
            !strcmp(actual, key)) {
            float center = g->phase == G_NEWSPAPER ? 480 : g->phase == G_ENDING ? 480 : 415.5f;
            float top = g->phase == G_NEWSPAPER ? 452 : g->phase == G_ENDING ? 20 : 407;
            unsigned w = (unsigned)c->width, h = (unsigned)c->height;
            (void)gpu_texture_dimensions(c->handle, &w, &h);
            image(c->handle, center - w / 2.0f, top, (float)w, (float)h, 0xffffffffu, 0);
            return;
        }
    }
}
static void phone_subtitle(const Game *g, unsigned ms) {
    if (!g->save.options.subtitles || g->phone_muted || g->night > 4)
        return;
    for (unsigned i = 0; i < sizeof(subtitle_cues) / sizeof(*subtitle_cues); ++i) {
        const SubtitleCue *c = &subtitle_cues[i];
        if (c->language == g->save.options.language && c->track == g->night &&
            ms >= (unsigned)c->time && ms < (unsigned)c->end) {
            unsigned w = (unsigned)c->width, h = (unsigned)c->height;
            (void)gpu_texture_dimensions(c->handle, &w, &h);
            image(c->handle, (960 - w) / 2, 20, (float)w, (float)h, 0xffffffffu, 0);
            return;
        }
    }
}
static void sprite_at(int handle, float x, float y) {
    const ImageDef *im = scene_image(handle);
    if (im)
        image(handle, (x - im->hotspot_x) * .75f, 2 + (y - im->hotspot_y) * .75f, im->width * .75f,
              im->height * .75f, 0xffffffffu, 0);
}
static int number_width(int value, const int *digits) {
    int width = 0;
    do {
        const ImageDef *im = scene_image(digits[value % 10]);
        if (im)
            width += im->width;
        value /= 10;
    } while (value);
    return width;
}
static void number_at(int value, const int *digits, int right, int bottom) {
    do {
        int handle = digits[value % 10];
        const ImageDef *im = scene_image(handle);
        if (im) {
            right -= im->width;
            image(handle, right * .75f, 2 + (bottom - im->height) * .75f, im->width * .75f,
                  im->height * .75f, 0xffffffffu, 0);
        }
        value /= 10;
    } while (value);
}
static void hud(const Game *g) {
    static const int power_digits[] = {52, 81, 82, 84, 85, 86, 87, 123, 128, 146};
    static const int clock_digits[] = {249, 252, 253, 254, 255, 256, 257, 258, 259, 260};
    static const int night_digits[] = {589, 588, 595, 596, 597, 598, 599, 600, 601, 602};
    static const int usage_images[] = {212, 213, 214, 456, 455};
    int value = g->power / 10, right = 182 + number_width(value, power_digits);
    art(89, 0, 0, 106, 638, 0, 1, 0);
    number_at(value, power_digits, right, 646);
    sprite_at(204, right + 3, 645);
    art(88, 0, 0, 74, 674, 0, 1, 0);
    const ImageDef *usage = scene_image(usage_images[g->usage < 1   ? 0
                                                     : g->usage > 5 ? 4
                                                                    : g->usage - 1]);
    if (usage)
        image(usage_images[g->usage < 1   ? 0
                           : g->usage > 5 ? 4
                                          : g->usage - 1],
              90, 494.75f, usage->width * .75f, usage->height * .75f, 0xffffffffu, 0);
    number_at(g->hour ? g->hour : 12, clock_digits, 1185, 59);
    art(106, 0, 0, 1198, 31, 0, 1, 0);
    art(115, 0, 0, 754, 74, 0, 1, 0);
    number_at(g->night, night_digits, 1237, 89);
    art(113, 0, 0, 554, 668, 0, 1, 0);
    if (!g->phone_muted && g->night <= 5 && g->phone_ms < 180000)
        art(119, 0, 0, 87, 37, 0, 1, 0);
}
static void hint_animation(int object, int animation) {
    int n = scene_art_count(object, animation);
    for (int f = 0; f < n; ++f)
        gpu_queue_prefetch(scene_art(object, animation, f));
}
int game_view_loading(const Game *g) {
    if (g->phase == G_CREEPY_START)
        return !gpu_texture_ready(211) || !gpu_texture_ready(192);
    if (g->phase == G_CREEPY_END)
        return !gpu_texture_ready(548);
    if (g->phase == G_PLAY) {
        for (int i = 0; i < CONTROL_TEXTURE_COUNT; ++i)
            if (!gpu_texture_ready(control_textures[i]))
                return 1;
        for (unsigned i = 0; i < sizeof(camera_noise_images) / sizeof(*camera_noise_images); ++i)
            if (!gpu_texture_ready(camera_noise_images[i]))
                return 1;
    }
    if (g->phase == G_OUTAGE && g->outage_stage < 2)
        return !gpu_texture_ready(395) || !gpu_texture_ready(396);
    if (g->phase == G_SCARE) {
        const ScareAnimation *a = scare_animation(g->killer);
        if (!a)
            return !gpu_texture_ready(573);
        for (int f = 0; f < a->count; ++f)
            if (!gpu_texture_ready(a->frames[f]))
                return 1;
    }
    return 0;
}
static void warm_scares(const Game *g) {
    for (int k = 0; k < 4; ++k) {
        const ScareAnimation *a = scare_animation(k);
        for (int f = 0; f < 3; ++f)
            gpu_queue_prefetch(a->frames[f]);
    }
    int threat = -1;
    if (g->position[0] == 80 || g->position[0] == 81 || g->position[0] == 122)
        threat = 0;
    if (g->fox >= 2)
        threat = 3;
    if (g->position[1] == 99 || g->jam[0])
        threat = 1;
    if (g->position[2] == 100 || g->jam[1])
        threat = 2;
    const ScareAnimation *a = scare_animation(threat);
    if (a)
        for (int f = 0; f < a->count; ++f)
            gpu_queue_warm(a->frames[f]);
}
static void warm_night(const Game *g) {
    /* Warm current-night starting views during the advertisement/night intro.
     * These are low-priority hints; the renderer never evicts to satisfy one. */
    gpu_queue_prefetch(591);
    hint_animation(59, 0);
    hint_animation(123, 0);
    hint_animation(124, 0);
    gpu_queue_prefetch(548);
    if (g->halloween) {
        hint_animation(125, 0);
        hint_animation(125, 12);
        hint_animation(131, 0);
        hint_animation(131, 12);
        hint_animation(126, 0);
        hint_animation(126, 12);
    }
    for (int o = 60; o <= 61; ++o) {
        hint_animation(o, 0);
        hint_animation(o, 13);
        gpu_queue_prefetch(scene_art(o, 12, 1));
    }
    const int buttons[] = {62, 66};
    for (int i = 0; i < 2; ++i) {
        hint_animation(buttons[i], 0);
        for (int a = 12; a <= 14; ++a)
            hint_animation(buttons[i], a);
    }
    for (int a = 12; a <= 15; ++a)
        hint_animation(134, a);
    hint_animation(69, 0);
    hint_animation(9, 0);
    hint_animation(74, 0);
    Game probe = {0};
    probe.night = 1;
    probe.branch[0] = probe.branch[1] = 1;
    probe.position[0] = probe.position[1] = probe.position[2] = 75;
    for (int c = 0; c < 11; ++c) {
        probe.camera = c;
        hint_animation(camera_map[c].object, 0);
        hint_animation(camera_map[c].object, 12);
        if (c != 9) {
            int variants[3], count = game_camera_variants(&probe, variants);
            for (int v = 0; v < count; ++v)
                gpu_queue_prefetch(scene_art(51, variants[v], 0));
        }
    }
}
static void plushies(const Game *g, int dark) {
    for (int i = 0; i < 4; ++i)
        if (g->save.options.plush[i])
            art(plush_places[i].object, dark ? 12 : 0, 0, plush_places[i].x, plush_places[i].y,
                g->pan, 1, g->save.options.perspective);
}
static void art_animation(int object, int animation, unsigned ms, unsigned fps, float anchor_x,
                          float anchor_y, float pan, int perspective, int blend) {
    unsigned count = (unsigned)scene_art_count(object, animation);
    if (!count)
        return;
    SpriteTween f = sprite_tween(ms, count, fps);
    int first = scene_art(object, animation, f.frame), next = scene_art(object, animation, f.next);
    const ImageDef *a = scene_image(first), *b = scene_image(next);
    if (!a || !b)
        return;
    float t = f.fraction,
          x = (anchor_x - pan - (a->hotspot_x + (b->hotspot_x - a->hotspot_x) * t)) * .75f;
    float y = 2 + (anchor_y - (a->hotspot_y + (b->hotspot_y - a->hotspot_y) * t)) * .75f;
    float w = (a->width + (b->width - a->width) * t) * .75f,
          h = (a->height + (b->height - a->height) * t) * .75f;
    image(!blend && t >= .5f ? next : first, x, y, w, h, 0xffffffffu, perspective);
    if (blend && first != next && t > 0)
        image(next, x, y, w, h, ((unsigned)(t * 255) << 24) | 0xffffffu, perspective);
}
static int last_camera_noise = -1;
static void camera_noise(unsigned ms, unsigned color) {
    int wanted = scene_art(9, 0, ms / 80), next = scene_art(9, 0, ms / 80 + 1);
    (void)next;
    if (gpu_texture_ready(wanted))
        last_camera_noise = wanted;
    int visible =
        last_camera_noise >= 0 && gpu_texture_ready(last_camera_noise) ? last_camera_noise : wanted;
    if (visible != wanted && visible != next)
        gpu_queue_resident(visible);
    screen(visible, color);
}
static void room(const Game *g) {
    int p = g->save.options.perspective, pan = g->pan;
    art(51, 0, 0, 0, 0, pan, 1, p);
    art(59, 0, (int)(clock_time / 55), 0, 0, pan, 1, p);
    for (int side = 0; side < 2; ++side) {
        if (g->light[side])
            art(134, side ? (g->position[2] == 100 ? 15 : 13) : (g->position[1] == 99 ? 14 : 12), 0,
                0, 0, pan, 1, p);
        int anim = g->door_ms[side] ? (g->door[side] ? 12 : 14) : (g->door[side] ? 13 : 0);
        if (g->door_ms[side])
            art_animation(60 + side, anim, g->door_ms[side], 20, side ? 1270 : 72, -1, pan, p, 0);
        else
            art(60 + side, anim, 0, side ? 1270 : 72, -1, pan, 1, p);
        int button = g->door[side] ? (g->light[side] ? 14 : 12) : (g->light[side] ? 13 : 0);
        art(side ? 66 : 62, button, 0, side ? 1546 : 48, side ? 400 : 390, pan, 1, p);
    }
    plushies(g, 0);
    if (g->halloween) {
        art(125, 0, 0, 0, 0, pan, 1, p);
        art(131, 0, 0, 1599, 0, pan, 1, p);
        art(126, 0, (int)(g->elapsed * 15 / 1000), 734, 485, pan, 1, p);
    }
    if (g->golden == 2)
        art(124, 0, 0, 660, 478, pan, 1, p);
}
static void outage_room(const Game *g, unsigned ms) {
    const ScareAnimation *outage = scare_animation(SCARE_POWER_OUT);
    for (int f = 0; f < outage->count; ++f)
        gpu_queue_resident(outage->frames[f]);
    if (g->outage_stage >= 2)
        return;
    gpu_queue_resident(395);
    gpu_queue_resident(396);
    int anim = g->outage_stage == 1 && (ms / 150) % 2 ? 47 : 46;
    if (!gpu_texture_ready(scene_art(51, anim, 0))) {
        if (gpu_texture_ready(395))
            anim = 46;
        else if (gpu_texture_ready(396))
            anim = 47;
        else {
            const ImageDef *im = scene_image(591);
            gpu_queue_resident(591);
            if (im)
                image(591, -g->pan * .75f, 2, im->width * .75f, im->height * .75f, 0xff302018u, 0);
            return;
        }
    }
    art(51, anim, 0, 0, 0, g->pan, 1, 0);
    plushies(g, 1);
    if (g->halloween) {
        art(125, 12, 0, 0, 0, g->pan, 1, 0);
        art(131, 12, 0, 1599, 0, g->pan, 1, 0);
        art(126, 12, 0, 734, 485, g->pan, 1, 0);
    }
}
void game_view_prepare(const Game *g, unsigned ms, unsigned phone_ms) {
    clock_time = ms;
    gpu_queue_reset(g->save.options.display);
    int phase = g->phase;
    if (phase == G_PLAY || phase == G_INTRO || phase == G_PAUSE || phase == G_HELP ||
        ((phase == G_OPTIONS || phase == G_AUDIO_OPTIONS) && g->options_parent == G_PAUSE)) {
        for (int i = 0; i < CONTROL_TEXTURE_COUNT; ++i)
            gpu_queue_resident(control_textures[i]);
        if (g->return_phase != G_OUTAGE)
            for (unsigned i = 0; i < sizeof(camera_noise_images) / sizeof(*camera_noise_images);
                 ++i)
                gpu_queue_resident(camera_noise_images[i]);
    }
    if (phase == G_INTRO || (phase == G_PLAY && !g->monitor))
        warm_scares(g);
    if (phase == G_NEWSPAPER || phase == G_INTRO || (phase == G_PLAY && !gpu_texture_ready(591)))
        warm_night(g);
    if (android_menu_phase(phase))
        android_menu_prepare(g, ms);
    else if (phase == G_NEWSPAPER) {
        screen(574, 0xffffffffu);
        forced(g, "/13-ad", (g->phase_ms / 1000) % 8);
        unsigned alpha = g->phase_ms < 600    ? 255 - g->phase_ms * 255 / 600
                         : g->phase_ms > 7400 ? (g->phase_ms - 7400) * 255 / 600
                                              : 0;
        if (alpha > 255)
            alpha = 255;
        if (alpha)
            screen(194, (alpha << 24) | 0xffffffu);
    } else if (phase == G_PLAY || phase == G_PAUSE ||
               (phase == G_OPTIONS && g->options_parent == G_PAUSE) || phase == G_HELP) {
        if ((phase == G_PAUSE || phase == G_HELP) && g->return_phase == G_OUTAGE)
            outage_room(g, ms);
        else {
            if (g->monitor && !g->monitor_ms) {
                int a = game_camera_animation(g);
                int variants[3], variant_count = game_camera_variants(g, variants);
                if (game_camera_view(g) != 6)
                    for (int v = 0; v < variant_count; ++v) {
                        int handle = scene_art(51, variants[v], 0);
                        gpu_queue_resident(handle);
                    }
                int background = -1, displayed = -1;
                if (game_camera_view(g) != 6) {
                    int f = g->fox == 4 && game_camera_view(g) == 3 ? (int)(g->fox_ms / 50) : 0;
                    background = scene_art(51, a, f);
                    gpu_queue_focus(background);
                    if (gpu_texture_ready(background))
                        displayed = background;
                    else
                        for (int v = 0; v < variant_count && displayed < 0; ++v) {
                            int handle = scene_art(51, variants[v], 0);
                            if (gpu_texture_ready(handle))
                                displayed = handle;
                        }
                    if (displayed >= 0) {
                        const ImageDef *im = scene_image(displayed);
                        if (im)
                            image(displayed, (-game_camera_pan(g) - im->hotspot_x) * .75f,
                                  2 - im->hotspot_y * .75f, im->width * .75f, im->height * .75f,
                                  0xffffffffu, 0);
                    }
                }
                camera_noise(ms, game_camera_view(g) != 6 && displayed < 0 ? 0xffffffffu
                                 : g->static_ms                            ? 0xe0ffffffu
                                                                           : 0x24ffffffu);
                /* Keep the office and camera-static animation warm while watching. */
                gpu_queue_prefetch(591);
                Game probe = *g;
                const int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                for (int d = 0; d < 4; ++d) {
                    probe.camera = camera_map_move(g->camera, directions[d][0], directions[d][1]);
                    if (probe.camera != g->camera && probe.camera != 9)
                        gpu_queue_warm(scene_art(51, game_camera_animation(&probe), 0));
                }
                art(74, 0, 0, 848, 313, 0, 1, 0);
                for (int i = 0; i < 11; ++i)
                    art(camera_map[i].object, g->camera == i ? 12 : 0,
                        (int)((g->elapsed - g->camera_selected_ms) * 3u * 60 / 100000),
                        camera_map[i].x, camera_map[i].y, 0, 1, 0);
                art(87, camera_map[g->camera].label, 0, 832, 292, 0, 1, 0);
                art(50, 0, (int)(g->elapsed / 500), 92, 76, 0, 1, 0);
                if (g->camera == 9)
                    art(86, 0, 0, 384, 69, 0, 1, 0);
                art(130, 0, (int)(g->elapsed / 500), 1028, 657, 0, 1, 0);
                image(120, 12, 14, 935.25f, 1.5f, 0xffffffffu, 0);
                image(120, 12, 528.5f, 935.25f, 1.5f, 0xffffffffu, 0);
                image(120, 12, 14, 1.5f, 516, 0xffffffffu, 0);
                image(120, 945.75f, 14, 1.5f, 516, 0xffffffffu, 0);
                if (g->fox == 0 && g->camera == 2)
                    forced(g, "/piratecove", 0);
                if (a == 23)
                    forced(g, "23", (clock_time / 3000) % 5);
                if (a >= 69 && a <= 72) {
                    const int steps[] = {9, 8, 7, 8};
                    char key[4];
                    key[0] = '7';
                    key[1] = (char)('0' + a - 70);
                    key[2] = 0;
                    if (a == 69) {
                        key[0] = '6';
                        key[1] = '9';
                    }
                    forced(g, key, (clock_time / 3000) % (unsigned)steps[a - 69]);
                }
                if (a == 68)
                    forced(g, "68", 0);
                if (a == 73)
                    forced(g, "73", 0);
            } else
                room(g);
            if (g->monitor_ms)
                art_animation(g->monitor ? 69 : 73, 0, g->monitor_ms, 30, 0, 0, 0, 0, 1);
            hud(g);
            phone_subtitle(g, phone_ms);
            if (g->hallucination_left) {
                const int frames[] = {525, 5, 7, 5};
                for (int i = 0; i < 4; ++i)
                    gpu_queue_resident(frames[i]);
                gpu_queue_resident(194);
                if (g->hallucination_roll == 1 && gpu_texture_ready(525) && gpu_texture_ready(5) &&
                    gpu_texture_ready(7)) {
                    int f = (int)(g->elapsed * 45 / 1000) % 4;
                    screen(194, 0xffffffffu);
                    sprite_at(frames[f], f == 3 ? -257 : 0, f == 3 ? -246 : 0);
                }
            }
        }
        if (phase == G_PAUSE) {
            screen(194, 0xe0000000u);
            art(14, 0, 0, 100, 280 + g->selection * 64, 0, 1, 0);
        } else if (phase == G_HELP)
            screen(194, 0xf0000000u);
    } else if (phase == G_OUTAGE)
        outage_room(g, ms);
    else if (phase == G_CREEPY_START) {
        image(211, 207, 2, 569.25f, 540, 0xffffffffu, 0);
        gpu_queue_resident(192);
        if (g->phase_ms >= 9500) {
            sprite_at(192, 510, 192);
            sprite_at(192, 804, 196);
        }
    } else if (phase == G_CREEPY_END)
        screen(548, 0xffffffffu);
    else if (phase == G_SCARE) {
        const ScareAnimation *a = scare_animation(g->killer);
        if (g->killer == 4)
            screen(573, 0xffffffffu);
        else if (a) {
            int f = scare_frame(g->killer, g->phase_ms);
            const ImageDef *im = scene_image(a->frames[f]);
            /* Protect every frame from LRU eviction, not just the displayed one. */
            for (int j = 0; j < a->count; ++j)
                gpu_queue_resident(a->frames[j]);
            if (g->killer == SCARE_POWER_OUT) {
                /* Scene 08-freddy starts on black at (0,0). Keep black until
                 * the complete sequence is resident; never fall back to room. */
                if (!game_view_loading(g))
                    sprite_at(a->frames[f], 0, 0);
            } else {
                if (!gpu_texture_ready(a->frames[f]))
                    room(g);
                if (g->killer == 3) {
                    image(298, 0, 2, 1200, 540, 0xffffffffu, 0);
                    if (im)
                        image(a->frames[f], -im->hotspot_x * .75f, 2 - im->hotspot_y * .75f,
                              im->width * .75f, im->height * .75f, 0xffffffffu, 0);
                } else
                    image(a->frames[f], -120, 2, im ? im->width * .75f : 1200, 540, 0xffffffffu, 0);
            }
        }
    } else if (phase == G_DEAD) {
        screen(358, 0xffffffffu);
        image(471, 784.5f, 497, 154.5f, 20.25f, 0xffffffffu, 0);
    } else if (phase == G_ENDING) {
        screen(g->night == 5 ? 58 : g->night == 6 ? 39 : 126, 0xffffffffu);
        forced(g,
               g->night == 5   ? "/12-the end"
               : g->night == 6 ? "/14-the end 2"
                               : "/16-the end 3",
               (g->phase_ms / 3000) % (g->night == 7 ? 4 : 3));
    } else if (phase == G_WIN) {
        float scroll = g->phase_ms <= 3000   ? 0
                       : g->phase_ms >= 4000 ? 110
                                             : (g->phase_ms - 3000) * .11f;
        sprite_at(350, 544, 298 - scroll);
        sprite_at(351, 548, 408 - scroll);
        image(194, 0, 2, 960, 223.5f, 0xffffffffu, 0);
        image(194, 0, 279.5f, 960, 262.5f, 0xffffffffu, 0);
        sprite_at(352, 640, 296);
    } else
        screen(scene_art(9, 0, ms / 100), 0x65ffffffu);
    gpu_queue_prepare();
}
static void text(int x, int y, const char *s) {
    gpu_text(x, y, s, 0xffffffffu);
}
void game_view_draw(const Game *g, unsigned fps, int save_error) {
    gpu_queue_draw();
    char s[96];
    GameOptions const *o = &g->save.options;
    int p = g->phase;
    if (android_menu_phase(p)) {
        if (g->diagnostics) {
            GpuStats st;
            gpu_get_stats(&st);
            snprintf(s, sizeof(s), "FPS %u PREP %u UP %u DRAW %u WAIT %u PRES %u MS", fps,
                     st.prepare_us / 1000, st.upload_us / 1000, st.draw_us / 1000,
                     st.sync_us / 1000, st.present_us / 1000);
            text(12, 527, s);
        }
        if (save_error)
            text(150, 8, "Save failed - check free space");
        if (gpu_asset_errors())
            text(150, 30, "MENU FILE MISSING - REINSTALL VPK");
        return;
    }
    if (p == G_PAUSE) {
        text(150, 125, "Paused");
        const char *items[] = {"Resume", "Options", "Return to title", "Exit application"};
        for (int i = 0; i < 4; ++i)
            text(105, 200 + i * 48, items[i]);
    } else if (p == G_HELP) {
        const char *help[] = {"L / R: Left / right door",
                              "Hold Square: Left light",
                              "Hold Circle: Right light",
                              "Cross: Raise / lower monitor",
                              "D-pad: Camera direction / office left-right",
                              "Left stick: Look around office",
                              "Triangle: Mute phone call",
                              "Start: Pause    Select: Help",
                              "Touch: Doors, lights, monitor, camera map",
                              "Assists disable night progression."};
        for (int i = 0; i < 10; ++i)
            text(65, 65 + i * 39, help[i]);
    } else if (p == G_PLAY && g->monitor && o->radar) {
        const char *shortnames[] = {"F", "B", "C"};
        for (int a = 0; a < 3; ++a)
            for (int c = 0; c < 11; ++c)
                if (g->position[a] == camera_map[c].object)
                    text(camera_map[c].x * 3 / 4 + a * 9, camera_map[c].y * 3 / 4 - 12,
                         shortnames[a]);
        snprintf(s, sizeof(s), "Foxy: %d", g->fox);
        text(650, 190, s);
    }
    if (p == G_PLAY && game_view_loading(g)) {
        gpu_rectangle(0, 2, 960, 540, 0xff000000u);
        text(380, 260, "Loading night...");
    }
    if (g->diagnostics && p != G_SCARE) {
        GpuStats st;
        gpu_get_stats(&st);
        snprintf(s, sizeof(s), "FPS %u PREP %u UP %u DRAW %u WAIT %u PRES %u MS", fps,
                 st.prepare_us / 1000, st.upload_us / 1000, st.draw_us / 1000, st.sync_us / 1000,
                 st.present_us / 1000);
        text(12, 527, s);
        snprintf(s, sizeof(s), "CACHE %u MB  LOADS %u  PENDING %u  IO %u MS",
                 st.cache_bytes / 1048576, st.loads, st.pending, st.read_us / 1000);
        text(12, 8, s);
    }
    if (save_error)
        text(150, 8, "Save failed - check free space");
    if (gpu_asset_errors())
        text(150, 30, "Missing game file - install Game Files v2");
}
int game_touch(Game *g, int x, int y, int initial) {
    if (android_menu_phase(g->phase)) {
        if (initial)
            android_menu_touch(g, x, y);
        return 0;
    }
    if (g->phase == G_PLAY) {
        if (initial && x >= 20 && x < 112 && y >= 18 && y < 44 && !g->phone_muted &&
            g->night <= 5 && g->phone_ms < 180000) {
            game_input(g, IN_PHONE, 0, 0);
            return 0;
        }
        if (x >= 180 && x < 654 && y >= 480 && y < 542) {
            if (initial)
                game_input(g, IN_OK, 0, 0);
            return 0;
        }
        if (g->monitor) {
            game_select_camera(g, camera_map_hit(x, y));
        } else if (x < 85 || x > 875) {
            int side = x > 875;
            if (y >= 220 && y < 260 && initial)
                game_input(g, side ? IN_R : IN_L, 0, 0);
            else if (y >= 260 && y < 325)
                return side ? IN_LIGHT_R : IN_LIGHT_L;
        } else if (initial) {
            float world_x = x / 0.75f + g->pan, world_y = (y - 2) / 0.75f;
            if (g->save.options.perspective) {
                float offset = (x - 480) / 480.0f, height = 540 * (1 - .055f * offset * offset),
                      top = 2 + (540 - height) * .5f;
                world_y = (y - top) * 720 / height;
            }
            /* APK honk.Active (117): anchor (678,240), 16x16 scaled to 50x50. */
            if (world_x >= 653 && world_x < 703 && world_y >= 215 && world_y < 265)
                game_sound(g, 36, 7, 0, 100);
        }
        return 0;
    }
    if (!initial)
        return 0;
    if (g->phase == G_PAUSE && y >= 192 && y < 392) {
        g->selection = (y - 192) / 48;
        if (g->selection < 4)
            game_input(g, IN_OK, 0, 0);
    } else if (g->phase == G_NEWSPAPER || g->phase == G_DEAD || g->phase == G_ENDING ||
               g->phase == G_HELP)
        game_input(g, IN_OK, 0, 0);
    return 0;
}
