/* Original APK sprites and scene positions, connected to the native game.
 * Textures for String objects are prepared locally into ux0:data/fnaf1/ui. */
#include "android_menu.h"
#include "gpu_renderer.h"
#include "scene_engine.h"
#include "generated_android_ui.h"
#include <stddef.h>

#define COUNT(a) ((int)(sizeof(a) / sizeof(*(a))))
static const AndroidWidget *widget(int scene, int object) {
    for (int i = 0; i < COUNT(android_widgets); ++i)
        if (android_widgets[i].scene == scene && android_widgets[i].object == object)
            return &android_widgets[i];
    return NULL;
}
static const AndroidText *caption(int object, int paragraph) {
    for (int i = 0; i < COUNT(android_texts); ++i)
        if (android_texts[i].object == object && android_texts[i].paragraph == paragraph)
            return &android_texts[i];
    return NULL;
}
static void image_at(int handle, float x, float y, unsigned color) {
    const ImageDef *im = scene_image(handle);
    if (!im)
        return;
    gpu_queue_image(handle, (x - im->hotspot_x) * .75f, 2 + (y - im->hotspot_y) * .75f,
                    im->width * .75f, im->height * .75f, color, 0);
}
static void object_at(int object, int animation, int frame, float x, float y) {
    image_at(scene_art(object, animation, frame), x, y, 0xffffffffu);
}
static void original(int scene, int object, int animation, int frame) {
    const AndroidWidget *w = widget(scene, object);
    if (w)
        object_at(object, animation, frame, (float)w->x, (float)w->y);
}
static void string_at(int object, int paragraph, int x, int y) {
    const AndroidText *t = caption(object, paragraph);
    if (t)
        gpu_queue_image(t->handle, x * .75f, 2 + y * .75f, (float)t->width, (float)t->height,
                        0xffffffffu, 0);
}
static void string_original(int scene, int object, int paragraph) {
    const AndroidWidget *w = widget(scene, object);
    if (w)
        string_at(object, paragraph, w->x, w->y);
}
static void native_caption(int handle, int x, int y) {
    for (int i = 0; i < COUNT(android_texts); ++i)
        if (android_texts[i].handle == handle) {
            const AndroidText *t = &android_texts[i];
            gpu_queue_image(handle, x * .75f, 2 + y * .75f, (float)t->width, (float)t->height,
                            0xffffffffu, 0);
            return;
        }
}
static unsigned hash(unsigned x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}
static void background(unsigned ms) {
    unsigned roll = hash(ms / 80 + 17) % 100;
    int a = roll == 99 ? 14 : roll == 98 ? 13 : roll == 97 ? 12 : 0;
    image_at(scene_art(10, a, 0), 0, 0, 0x00ffffffu | (255 - hash(ms / 300 + 31) % 180) << 24);
    image_at(scene_art(9, 0, (int)(ms / 21)), 0, 0,
             0x00ffffffu | (155 - hash(ms / 80 + 71) % 100) << 24);
    /* Original stretched static bands remain below buttons and text. */
    gpu_queue_image(scene_art(18, 0, 0), 0, (float)((ms / 5) % 620) - 38, 960, 24, 0x16ffffffu, 0);
    if (hash(ms / 300 + 73) % 3 != 1) {
        int h = scene_art(17, 0, (int)(ms / 200));
        const ImageDef *im = scene_image(h);
        if (im)
            gpu_queue_image(h, 0, 2 - im->hotspot_y * .75f, 960, im->height * .75f, 0x30ffffffu, 0);
    }
    /* Eight static textures and four Freddy faces fit together in the cache. */
    for (int f = 0; f < 8; ++f)
        gpu_queue_prefetch(scene_art(9, 0, f));
    for (int f = 0; f < 4; ++f)
        gpu_queue_prefetch(scene_art(10, f ? f + 11 : 0, 0));
}
static int fade_alpha(unsigned ms, unsigned duration, unsigned in, unsigned out) {
    if (in && ms < in)
        return (int)(255 - (ms * 255 / in));
    if (out && ms > duration - out)
        return (int)((ms - (duration - out)) * 255 / out);
    return 0;
}
static void fade(int alpha) {
    if (alpha < 0)
        alpha = 0;
    if (alpha > 255)
        alpha = 255;
    if (alpha)
        gpu_queue_image(194, 0, 2, 960, 540, (unsigned)alpha << 24 | 0xffffffu, 0);
}
static void cursor(int left, int center_y) {
    object_at(14, 0, 0, (float)(left - 36), (float)center_y);
}
static void select_original(int scene, int object) {
    const AndroidWidget *w = widget(scene, object);
    const ImageDef *im = scene_image(scene_art(object, 0, 0));
    if (w && im)
        cursor(w->x - im->hotspot_x, w->y);
}
static int title_object(int action) {
    const int ids[] = {12, 13, 20, 24, 34, 33};
    return ids[action];
}
static void warm_title(const Game *g) {
    int n = 4 + (g->save.unlocked >= 6) + (g->save.unlocked >= 7);
    for (int i = 0; i < n; ++i)
        gpu_queue_warm(scene_art(title_object(game_title_action(g, i)), 0, 0));
    const int objects[] = {9, 10, 11, 14, 17, 18, 21, 22, 32};
    for (int i = 0; i < COUNT(objects); ++i)
        gpu_queue_warm(scene_art(objects[i], 0, 0));
    const AndroidText *texts[] = {caption(40, 0), caption(41, 0),
                                  caption(43, g->save.next_night - 1)};
    for (int i = 0; i < COUNT(texts); ++i)
        if (texts[i])
            gpu_queue_warm(texts[i]->handle);
    gpu_queue_warm(UI_PORT_CREDIT);
}
static void title(const Game *g, int confirm) {
    if (confirm) {
        string_original(3, 42, 0);
        original(3, 36, 0, 0);
        original(3, 37, 0, 0);
        select_original(3, g->selection ? 37 : 36);
    } else {
        int n = 4 + (g->save.unlocked >= 6) + (g->save.unlocked >= 7);
        for (int i = 0; i < n; ++i)
            original(3, title_object(game_title_action(g, i)), 0, 0);
        /* Original title event group 122 moves this String from its editor
         * position outside the frame to (176,488). */
        string_at(43, g->save.next_night - 1, 176, 488);
        select_original(3, title_object(game_title_action(g, g->selection)));
    }
    original(3, 11, 0, 0);
    const int stars[] = {21, 22, 32};
    for (int i = 0; i < 3; ++i)
        if (g->save.stars & (1 << i))
            original(3, stars[i], 0, 0);
    string_original(3, 40, 0);
    string_original(3, 41, 0);
}
static void options(const Game *g) {
    const GameOptions *o = &g->save.options;
    const int labels[] = {215, 216, 217, 219, 220, 218};
    for (int i = 0; i < 6; ++i)
        string_original(19, labels[i], 0);
    if (o->display == 2)
        native_caption(UI_4_3, 304, 136);
    else
        original(19, 211, 0, o->display == 1 ? 0 : 1);
    original(19, 212, 0, !o->perspective);
    original(19, 213, 0, !o->vibration);
    original(19, 221, 0, o->subtitles);
    original(19, 222, 0, o->forced);
    original(19, 223, 0, o->language);
    original(19, 202, 0, 0);
    native_caption(UI_AUDIO, 176, 622);
    const int xs[] = {176, 176, 176, 688, 688, 688}, ys[] = {166, 326, 486, 326, 486, 166};
    if (g->selection < 6)
        cursor(xs[g->selection], ys[g->selection]);
    else if (g->selection == 6)
        cursor(176, 646);
    else if (g->selection == OPTIONS_CONTROLS)
        cursor(688, 646);
    else
        select_original(19, 202);
}
enum {
    CONTROLS_LAYOUT_Y = 108, CONTROLS_ACTION_Y = 150, CONTROLS_ACTION_STEP = 35,
    CONTROLS_BACK_Y = 453, CONTROLS_VALUE_X = 610
};
void android_menu_draw_controls(const Game *g) {
    if (g->phase == G_OPTIONS) {
        gpu_text(516, 478, "Controls", 0xffffffffu);
        return;
    }
    const GameOptions *o = &g->save.options;
    static const char *layouts[] = {"Contextual", "Classic", "Custom"};
    int y = g->selection == 0 ? CONTROLS_LAYOUT_Y
            : g->selection == CONTROLS_BACK ? CONTROLS_BACK_Y
            : CONTROLS_ACTION_Y + (g->selection - CONTROLS_FIRST_ACTION) * CONTROLS_ACTION_STEP;
    gpu_rectangle(69, y - 5, 796, 28, 0x70494949u);
    gpu_text(84, 58, "Controls", 0xffffffffu);
    gpu_text(84, CONTROLS_LAYOUT_Y, "Layout", 0xffffffffu);
    gpu_text(CONTROLS_VALUE_X, CONTROLS_LAYOUT_Y, layouts[o->control_layout], 0xffffffffu);
    for (int i = 0; i < CONTROL_ACTION_COUNT; ++i) {
        int row = CONTROLS_ACTION_Y + i * CONTROLS_ACTION_STEP;
        uint32_t color = o->control_layout == CONTROL_CUSTOM ||
                         game_control_binding(o, i) != BUTTON_NONE ? 0xffffffffu : 0xff888888u;
        gpu_text(84, row, game_control_action_name(i), color);
        gpu_text(CONTROLS_VALUE_X, row,
                 g->binding_capture && g->selection == CONTROLS_FIRST_ACTION + i
                     ? "Press a button"
                     : game_control_button_name(o, i),
                 color);
    }
    gpu_text(84, CONTROLS_BACK_Y, "Back", 0xffffffffu);
    const char *hint = g->binding_capture ? "Press L, R, or a face button. Select cancels."
                       : o->control_layout == CONTROL_CUSTOM
                           ? "Cross to bind. Left / Right to cycle or unassign."
                           : "Left / Right to change layout. Select Custom to edit.";
    gpu_text(84, 503, hint, 0xffdddddd);
}
static void audio_options(const Game *g) {
    native_caption(UI_MUSIC, 176, 80);
    native_caption(UI_VOLUME, 688, 80);
    native_caption(UI_DIAGNOSTICS, 176, 240);
    object_at(221, 0, g->save.options.music, 384, 166);
    native_caption(UI_VOLUME_0 + g->save.options.volume / 10, 800, 136);
    object_at(221, 0, g->diagnostics, 384, 326);
    original(19, 202, 0, 0);
    const int xs[] = {176, 688, 176}, ys[] = {166, 166, 326};
    if (g->selection < 3)
        cursor(xs[g->selection], ys[g->selection]);
    else
        select_original(19, 202);
}
static void extras(const Game *g) {
    const GameOptions *o = &g->save.options;
    int tab = g->selection < 3 ? 0 : g->selection < 7 ? g->selection - 2 : 0;
    const int icons[] = {232, 233, 234, 235, 247};
    for (int i = 0; i < 5; ++i)
        original(20, icons[i], 0, 0);
    object_at(236, 0, 0, 160 + tab * 240, 104);
    string_original(20, 238, tab);
    string_original(20, 239, tab);
    native_caption(UI_LOCAL, 64, 360);
    if (!tab) {
        const int values[] = {o->unlimited, o->fast, o->radar};
        for (int i = 0; i < 3; ++i) {
            string_original(20, 240 + i, 0);
            original(20, 244 + i, 11, values[i]);
        }
        if (g->selection < 3)
            cursor(64 + g->selection * 240, 646);
    } else {
        object_at(244, 11, o->plush[tab - 1], 160, 646);
        if (g->selection != 7)
            cursor(64, 646);
    }
    original(20, 202, 0, 0);
    if (g->selection == 7)
        select_original(20, 202);
}
void android_menu_draw_extras_status(const Game *g) {
    const GameOptions *o = &g->save.options;
    for (int i = 0; i < 4; ++i)
        if (o->plush[i])
            gpu_check_badge(379 + i * 180, 57);
    if (g->selection < 3 || g->selection == 7) {
        const int enabled[] = {o->unlimited, o->fast, o->radar};
        for (int i = 0; i < 3; ++i)
            if (enabled[i])
                gpu_check_badge(162 + i * 180, 486);
    }
}
static void ai_value(int value, int center) {
    const int digits[] = {556, 557, 558, 559, 560, 561, 562, 563, 564, 565};
    int frames[2], n = value >= 10 ? 2 : 1;
    frames[0] = digits[n == 2 ? value / 10 : value % 10];
    frames[1] = digits[value % 10];
    float width = 0;
    for (int i = 0; i < n; ++i) {
        const ImageDef *im = scene_image(frames[i]);
        if (im)
            width += im->width * .75f;
    }
    float x = center * .75f - width / 2;
    for (int i = 0; i < n; ++i) {
        const ImageDef *im = scene_image(frames[i]);
        if (im) {
            gpu_queue_image(frames[i], x, 2 + (496 - im->height / 2) * .75f, im->width * .75f,
                            im->height * .75f, 0xffffffffu, 0);
            x += im->width * .75f;
        }
    }
}
static void custom(const Game *g) {
    for (int i = 0; i < COUNT(android_widgets); ++i) {
        const AndroidWidget *w = &android_widgets[i];
        if (w->scene != 14 || w->object < 180 || w->object == 202)
            continue;
        object_at(w->object, 0, 0, (float)w->x, (float)w->y);
    }
    for (int i = 0; i < 4; ++i)
        ai_value(g->save.custom[i], 196 + i * 296);
    original(14, 202, 0, 0);
    if (g->selection < 4)
        cursor(96 + g->selection * 296, 496);
    else
        select_original(14, g->selection == 4 ? 181 : 202);
}
int android_menu_phase(int p) {
    return p == G_WARNING || p == G_TITLE || p == G_CONFIRM_NEW || p == G_OPTIONS ||
           p == G_AUDIO_OPTIONS || p == G_EXTRAS || p == G_CUSTOM || p == G_INTRO ||
           p == G_CONTROL_OPTIONS;
}
void android_menu_night_card(int night) {
    original(4, 45, 0, 0);
    original(4, 46, night == 1 ? 0 : night + 10, 0);
}
void android_menu_prepare(const Game *g, unsigned ms) {
    int p = g->phase;
    if (p == G_WARNING) {
        original(0, 0, 0, 0);
        warm_title(g);
        fade(fade_alpha(g->phase_ms, WARNING_DURATION_MS, 400, 550));
        return;
    }
    if (p == G_INTRO) {
        android_menu_night_card(g->night);
        if (g->phase_ms < 300) {
            int frame = (int)(g->phase_ms / 27);
            int h = scene_art(44, 0, frame);
            const ImageDef *im = scene_image(h);
            if (im)
                gpu_queue_image(h, 0, 2, 960, im->height * .75f, 0xffffffffu, 0);
        }
        fade(fade_alpha(g->phase_ms, NIGHT_INTRO_DURATION_MS, 300, 0));
        return;
    }
    if (p != G_CUSTOM)
        background(ms);
    if (p == G_TITLE || p == G_CONFIRM_NEW)
        title(g, p == G_CONFIRM_NEW);
    else if (p == G_OPTIONS)
        options(g);
    else if (p == G_AUDIO_OPTIONS)
        audio_options(g);
    else if (p == G_CONTROL_OPTIONS)
        fade(180);
    else if (p == G_EXTRAS)
        extras(g);
    else if (p == G_CUSTOM)
        custom(g);
    if (p == G_TITLE || p == G_CONFIRM_NEW)
        native_caption(UI_PORT_CREDIT, 64, 655);
    if (p == G_TITLE)
        fade(fade_alpha(g->phase_ms, 0, 500, 0));
}
static int hit(int x, int y, int left, int top, int w, int h) {
    return x >= left && x < left + w && y >= top && y < top + h;
}
static int hit_original(int scene, int obj, int x, int y) {
    const AndroidWidget *w = widget(scene, obj);
    const ImageDef *im = scene_image(scene_art(obj, 0, 0));
    return w && im &&
           hit(x, y, w->x - im->hotspot_x - 18, w->y - im->hotspot_y - 8, im->width + 36,
               im->height + 16);
}
void android_menu_touch(Game *g, int x, int y) {
    if (g->phase == G_CONTROL_OPTIONS) {
        if (hit(x, y, 69, CONTROLS_BACK_Y - 5, 796, 28)) {
            if (g->binding_capture)
                game_input(g, IN_HELP, 0, 0);
            g->selection = CONTROLS_BACK;
            game_input(g, IN_OK, 0, 0);
        } else if (!g->binding_capture && hit(x, y, 69, CONTROLS_LAYOUT_Y - 5, 796, 28)) {
            g->selection = 0;
            game_input(g, IN_OK, 0, 0);
        } else if (!g->binding_capture && x >= 69 && x < 865 &&
                   y >= CONTROLS_ACTION_Y - 5 &&
                   y < CONTROLS_ACTION_Y - 5 + CONTROL_ACTION_COUNT * CONTROLS_ACTION_STEP) {
            g->selection = CONTROLS_FIRST_ACTION + (y - CONTROLS_ACTION_Y + 5) / CONTROLS_ACTION_STEP;
            game_input(g, x >= CONTROLS_VALUE_X ? IN_RIGHT : IN_OK, 0, 0);
        }
        return;
    }
    /* Coordinates enter in the 960x540 game viewport, matching display transforms. */
    x = x * 4 / 3;
    y = (y - 2) * 4 / 3;
    if (g->phase == G_WARNING || g->phase == G_INTRO) {
        game_input(g, IN_OK, 0, 0);
        return;
    }
    if (g->phase == G_TITLE) {
        int n = 4 + (g->save.unlocked >= 6) + (g->save.unlocked >= 7);
        for (int i = 0; i < n; ++i)
            if (hit_original(3, title_object(game_title_action(g, i)), x, y)) {
                g->selection = i;
                game_input(g, IN_OK, 0, 0);
                return;
            }
    } else if (g->phase == G_CONFIRM_NEW) {
        for (int i = 0; i < 2; ++i)
            if (hit_original(3, i ? 37 : 36, x, y)) {
                g->selection = i;
                game_input(g, IN_OK, 0, 0);
                return;
            }
    } else if (g->phase == G_OPTIONS) {
        const int indices[2][3] = {{0, 1, 2}, {5, 3, 4}};
        for (int col = 0; col < 2; ++col)
            for (int row = 0; row < 3; ++row)
                if (hit(x, y, 176 + col * 512, 80 + row * 160, 416, 144)) {
                    g->selection = indices[col][row];
                    game_input(g, IN_OK, 0, 0);
                    return;
                }
        if (hit(x, y, 150, 610, 230, 80)) {
            g->selection = 6;
            game_input(g, IN_OK, 0, 0);
        } else if (hit(x, y, 650, 610, 260, 80)) {
            g->selection = OPTIONS_CONTROLS;
            game_input(g, IN_OK, 0, 0);
        } else if (hit_original(19, 202, x, y)) {
            g->selection = 7;
            game_input(g, IN_OK, 0, 0);
        }
    } else if (g->phase == G_AUDIO_OPTIONS) {
        if (hit(x, y, 176, 80, 416, 144))
            g->selection = 0;
        else if (hit(x, y, 688, 80, 416, 144))
            g->selection = 1;
        else if (hit(x, y, 176, 240, 416, 144))
            g->selection = 2;
        else if (hit_original(19, 202, x, y))
            g->selection = 3;
        else
            return;
        game_input(g, IN_OK, 0, 0);
    } else if (g->phase == G_EXTRAS) {
        for (int i = 0; i < 5; ++i)
            if (hit(x, y, 64 + i * 240, 64, 192, 80)) {
                g->selection = i ? i + 2 : 0;
                return;
            }
        int tab = g->selection < 3 ? 0 : g->selection < 7 ? g->selection - 2 : 0;
        if (hit_original(20, 202, x, y)) {
            g->selection = 7;
            game_input(g, IN_OK, 0, 0);
            return;
        }
        if (!tab) {
            for (int i = 0; i < 3; ++i)
                if (hit(x, y, 64 + i * 240, 600, 192, 95)) {
                    g->selection = i;
                    game_input(g, IN_OK, 0, 0);
                    return;
                }
        } else if (hit(x, y, 64, 600, 192, 95))
            game_input(g, IN_OK, 0, 0);
    } else if (g->phase == G_CUSTOM) {
        for (int i = 0; i < 4; ++i) {
            if (hit_original(14, 191 + i, x, y)) {
                g->selection = i;
                game_input(g, IN_LEFT, 0, 0);
                return;
            }
            if (hit_original(14, 187 + i, x, y)) {
                g->selection = i;
                game_input(g, IN_RIGHT, 0, 0);
                return;
            }
        }
        if (hit_original(14, 181, x, y)) {
            g->selection = 4;
            game_input(g, IN_OK, 0, 0);
        } else if (hit_original(14, 202, x, y)) {
            g->selection = 5;
            game_input(g, IN_OK, 0, 0);
        }
    }
}
