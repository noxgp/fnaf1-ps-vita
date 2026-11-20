#include "game.h"
#include "android_menu.h"
#include "gpu_renderer.h"
#include "game_view.h"
#include "scene_engine.h"
#include "scare_animation.h"
#include "touch_input.h"
#include "camera_map.h"
#include "game_events.h"
#include "generated_controls.h"
extern int capture_commands[96][7], capture_count;
static Game g;
static void fresh(void) {
    GameSave s;
    game_defaults(&s);
    game_init(&g, &s, 123);
    game_input(&g, IN_OK, 0, 0);
    g.sound_count = 0;
}
static void prepare(void) {
    g.phase_ms = 1200;
    gpu_queue_reset(0);
    android_menu_prepare(&g, 2000);
}
static int count(int handle) {
    int n = 0;
    for (int i = 0; i < capture_count; ++i)
        if (capture_commands[i][0] == handle)
            ++n;
    return n;
}
static int placed(int handle, int x, int y) {
    for (int i = 0; i < capture_count; ++i)
        if (capture_commands[i][0] == handle && capture_commands[i][1] == x &&
            capture_commands[i][2] == y)
            return 1;
    return 0;
}
#define CHECK(c, n)                                                                                \
    do {                                                                                           \
        if (!(c))                                                                                  \
            return 300 + (n);                                                                      \
    } while (0)
int run_android_checks(void) {
    fresh();
    prepare();
    CHECK(placed(341, 132, 284) && placed(444, 132, 344), 1);
    CHECK(count(172) == 1 && count(385) == 1 && count(401) == 1, 2);
    CHECK(!count(446) && !count(445) && !count(432), 3);
    android_menu_touch(&g, 160, 306);
    CHECK(g.phase == G_NEWSPAPER, 4);
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.phase == G_INTRO, 5);
    prepare();
    CHECK(placed(306, 409, 204) && placed(302, 399, 254), 6);
    android_menu_touch(&g, 480, 270);
    CHECK(g.phase == G_PLAY, 7);
    for (int i = 0; i < 50; ++i)
        game_tick(&g, 20);
    CHECK(g.power == 998, 8);
    fresh();
    android_menu_touch(&g, 840, 425);
    CHECK(g.phase == G_OPTIONS, 9);
    android_menu_touch(&g, 280, 245);
    CHECK(!g.save.options.perspective, 10);
    android_menu_touch(&g, 660, 125);
    CHECK(g.save.options.language == 1, 11);
    android_menu_touch(&g, 875, 487);
    CHECK(g.phase == G_TITLE, 12);
    android_menu_touch(&g, 840, 425);
    android_menu_touch(&g, 180, 487);
    CHECK(g.phase == G_AUDIO_OPTIONS, 13);
    android_menu_touch(&g, 280, 125);
    CHECK(!g.save.options.music, 14);
    android_menu_touch(&g, 660, 125);
    CHECK(g.save.options.volume == 90, 15);
    android_menu_touch(&g, 875, 487);
    CHECK(g.phase == G_OPTIONS, 16);
    game_input(&g, IN_BACK, 0, 0);
    android_menu_touch(&g, 840, 485);
    CHECK(g.phase == G_EXTRAS, 17);
    android_menu_touch(&g, 480, 80);
    CHECK(g.selection == 4 && !g.save.options.plush[1], 18);
    android_menu_touch(&g, 120, 486);
    CHECK(g.save.options.plush[1], 19);
    android_menu_touch(&g, 875, 487);
    CHECK(g.phase == G_TITLE, 20);
    g.save.unlocked = 7;
    g.save.stars = 7;
    prepare();
    CHECK(count(446) == 1 && count(445) == 1 && count(432) == 3, 21);
    android_menu_touch(&g, 217, 426);
    CHECK(g.phase == G_INTRO && g.night == 6, 22);
    prepare();
    CHECK(count(335) == 1 && !count(302), 23);
    fresh();
    g.save.unlocked = 7;
    android_menu_touch(&g, 248, 486);
    CHECK(g.phase == G_CUSTOM, 24);
    int ai = g.save.custom[0];
    android_menu_touch(&g, 223, 374);
    CHECK(g.save.custom[0] == ai + 1, 25);
    android_menu_touch(&g, 480, 486);
    CHECK(g.phase == G_INTRO && g.night == 7, 26);
    fresh();
    g.save.next_night = 5;
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.phase == G_CONFIRM_NEW, 27);
    android_menu_touch(&g, 149, 366);
    CHECK(g.phase == G_TITLE && g.save.next_night == 5 && g.selection == 1, 28);
    g.selection = 0;
    game_input(&g, IN_OK, 0, 0);
    android_menu_touch(&g, 147, 426);
    CHECK(g.phase == G_NEWSPAPER && g.save.next_night == 1, 29);
    fresh();
    g.phase = G_WARNING;
    prepare();
    CHECK(placed(24, 321, 189) && !count(172), 30);
    return 0;
}
static void night(void) {
    fresh();
    game_start(&g, 1);
    game_input(&g, IN_OK, 0, 0);
    g.sound_count = 0;
}
static void view(void) {
    game_view_prepare(&g, 2000, 22000);
}
static int captions(int y) {
    int n = 0;
    for (int i = 0; i < capture_count; ++i)
        if (capture_commands[i][0] >= 1024 && capture_commands[i][0] < 7000 &&
            capture_commands[i][2] == y)
            ++n;
    return n;
}
int run_presentation_checks(void) {
    night();
    view();
    CHECK(placed(201, 28, 475) && placed(146, 136, 470) && placed(146, 150, 470), 31);
    CHECK(placed(212, 90, 494) && placed(252, 852, 23) && placed(253, 870, 23), 32);
    CHECK(captions(20) == 1 && !captions(446), 33);
    g.monitor = 1;
    g.camera_scan = 0;
    view();
    int handle = scene_art(51, game_camera_animation(&g), 0);
    CHECK(placed(handle, 0, 2), 34);
    CHECK(placed(scene_art(87, 12, 0), 624, 221), 35);
    g.camera_scan = 320000;
    view();
    CHECK(placed(handle, -240, 2) && placed(scene_art(87, 12, 0), 624, 221), 36);
    g.camera = 5;
    view();
    CHECK(placed(scene_art(51, game_camera_animation(&g), 0), 0, 2), 37);
    g.camera = 0;
    game_touch(&g, 737, 469, 1);
    CHECK(g.camera == 3 && g.monitor && g.static_ms == 90, 38);
    game_touch(&g, 737, 470, 1);
    CHECK(g.camera == 4 && g.monitor, 39);
    g.sound_count = 0;
    game_touch(&g, 737, 484, 1);
    CHECK(g.camera == 4 && !g.sound_count, 40);
    game_touch(&g, 400, 500, 1);
    CHECK(!g.monitor, 41);
    g.monitor = 1;
    g.monitor_ms = 0;
    g.camera = 2;
    view();
    CHECK(!captions(407), 42);
    g.save.options.language = 8;
    view();
    CHECK(captions(407) == 1 && captions(20) == 1, 43);
    g.save.options.forced = 0;
    view();
    CHECK(!captions(407), 44);
    g.phase = G_WIN;
    g.phase_ms = 5000;
    view();
    CHECK(placed(351, 414, 225) && placed(352, 483, 224), 45);
    g.phase = G_DEAD;
    view();
    CHECK(placed(471, 784, 497), 46);
    night();
    g.monitor = 1;
    const int expected_labels[] = {54, 72, 73, 74, 76, 50, 79, 75, 71, 78, 77};
    for (int i = 0; i < 11; ++i) {
        g.camera = i;
        view();
        CHECK(placed(expected_labels[i], i == 2 ? 625 : 624, 221), 47);
    }
    game_touch(&g, 737, 454, 0);
    CHECK(g.camera == 3, 48);
    game_touch(&g, 737, 484, 0);
    CHECK(g.camera == 4, 49);
    game_touch(&g, 400, 532, 1);
    CHECK(!g.monitor, 50);
    game_touch(&g, 400, 532, 0);
    CHECK(!g.monitor, 51);
    game_touch(&g, 400, 532, 1);
    CHECK(g.monitor, 52);
    night();
    game_touch(&g, 388, 182, 1);
    CHECK(g.sound_count == 1 && g.sounds[0].id == 36 && g.sounds[0].channel == 7, 53);
    game_touch(&g, 388, 182, 0);
    CHECK(g.sound_count == 1, 54);
    game_touch(&g, 500, 182, 1);
    CHECK(g.sound_count == 1, 55);
    g.pan = 0;
    game_touch(&g, 508, 182, 1);
    CHECK(g.sound_count == 2, 56);
    g.pan = 320;
    game_touch(&g, 268, 183, 1);
    CHECK(g.sound_count == 3, 57);
    g.monitor = 1;
    game_touch(&g, 268, 183, 1);
    CHECK(g.sound_count == 3, 58);
    extern int capture_missing_handle, capture_resident_count;
    for (int k = 0; k < 4; ++k) {
        g.phase = G_SCARE;
        g.killer = k;
        g.phase_ms = 400;
        const ScareAnimation *a = scare_animation(k);
        capture_resident_count = 0;
        view();
        CHECK(capture_resident_count == a->count && !game_view_loading(&g), 59);
        capture_missing_handle = a->frames[a->count - 1];
        CHECK(game_view_loading(&g), 60);
        capture_missing_handle = -1;
    }
    night();
    TouchArea area = {20, 30, 1939, 1117};
    for (int display = 0; display < 3; ++display) {
        g.save.options.display = display;
        g.monitor = 1;
        for (int c = 0; c < 11; ++c) {
            int x = camera_map[c].x * 3 / 4, y = 2 + camera_map[c].y * 3 / 4;
            int px = display == 2 ? 117 + x * 726 / 960 : x,
                py = display == 1 ? (y - 2) * 544 / 540 : y, tx, ty;
            CHECK(touch_to_game(area, 20 + px * 2, 30 + py * 2, display, &tx, &ty), 61);
            game_touch(&g, tx, ty, 0);
            CHECK(g.camera == c && g.monitor, 62);
        }
        int px = display == 2 ? 117 + 400 * 726 / 960 : 400,
            py = display == 1 ? (532 - 2) * 544 / 540 : 532, tx, ty;
        CHECK(touch_to_game(area, 20 + px * 2, 30 + py * 2, display, &tx, &ty), 63);
        game_touch(&g, tx, ty, 1);
        CHECK(!g.monitor, 64);
        game_touch(&g, tx, ty, 0);
        CHECK(!g.monitor, 65);
    }
    night();
    g.monitor = 1;
    g.camera = 3;
    g.camera_flicker = 10;
    view();
    CHECK(count(37) == 1 && !count(19), 66);
    g.camera_flicker = 1;
    view();
    CHECK(count(19) == 1 && !count(37), 67);
    g.position[1] = 77;
    view();
    CHECK(count(41) == 1 && !count(19), 68);
    g.camera_flicker = 10;
    view();
    CHECK(count(37) == 1 && !count(41), 69);
    capture_resident_count = 0;
    view();
    CHECK(capture_resident_count == 10 + CONTROL_TEXTURE_COUNT, 70);
    capture_missing_handle = 41;
    view();
    int noise = scene_art(9, 0, 2000 / 80), opaque = 0;
    for (int i = 0; i < capture_count; ++i)
        if (capture_commands[i][0] == noise && capture_commands[i][5] == (int)0xffffffffu)
            opaque = 1;
    CHECK(!opaque && count(37) == 1 && !game_view_loading(&g), 71);
    capture_missing_handle = -1;
    g.phase = G_PLAY;
    g.camera = 4;
    g.night = 4;
    g.position[1] = 78;
    capture_resident_count = 0;
    view();
    CHECK(capture_resident_count == 11 + CONTROL_TEXTURE_COUNT, 72);
    night();
    view();
    CHECK(!count(109) && !count(47), 73);
    game_set_date(&g, 10, 31);
    view();
    CHECK(count(109) == 1 && count(106) == 1 && count(47) == 1, 74);
    g.phase = G_OUTAGE;
    view();
    CHECK(count(449) == 1 && count(454) == 1 && count(452) == 1, 75);
    night();
    g.golden = 1;
    view();
    CHECK(!count(573), 76);
    g.golden = 2;
    view();
    CHECK(placed(573, 172, 165), 77);
    g.monitor = 1;
    g.camera = 4;
    g.golden = 1;
    view();
    CHECK(count(scene_art(51, 74, 0)) == 1 && !count(573), 78);
    night();
    g.hallucination_left = 500;
    g.hallucination_roll = 1;
    view();
    CHECK(count(525) == 1, 79);
    g.elapsed = 24;
    view();
    CHECK(placed(5, 325, 290), 80);
    g.elapsed = 67;
    view();
    CHECK(placed(5, 132, 105), 81);
    g.hallucination_roll = 0;
    view();
    CHECK(!count(525) && !count(5) && !count(7), 82);
    g.phase = G_CREEPY_START;
    g.phase_ms = 9000;
    view();
    CHECK(placed(211, 207, 2) && !count(192), 83);
    g.phase_ms = 9500;
    view();
    CHECK(count(192) == 2 && placed(192, 378, 143), 84);
    g.phase = G_CREEPY_END;
    view();
    CHECK(count(548) == 1 && !count(591), 85);
    capture_missing_handle = 548;
    CHECK(game_view_loading(&g), 86);
    capture_missing_handle = -1;
    CHECK(!game_view_loading(&g), 87);
    night();
    g.monitor = 1;
    g.camera = 10;
    view();
    CHECK(capture_resident_count >= CONTROL_TEXTURE_COUNT, 88);
    capture_missing_handle = control_textures[0];
    CHECK(game_view_loading(&g), 89);
    capture_missing_handle = -1;
    CHECK(!game_view_loading(&g), 90);
    night();
    for (int i = 0; i < 4; ++i)
        g.save.options.plush[i] = 1;
    view();
    CHECK(placed(477, 330, 185) && placed(495, 281, 264) && placed(524, 518, 291) &&
              placed(496, 423, 285),
          91);
    g.pan = 0;
    view();
    CHECK(placed(477, 450, 185) && placed(495, 401, 264), 92);
    g.phase = G_OUTAGE;
    view();
    CHECK(count(583) == 1 && count(579) == 1 && count(584) == 1 && count(585) == 1, 93);
    night();
    g.phase = G_OUTAGE;
    g.outage_stage = 2;
    view();
    CHECK(!capture_count && !count(591), 94);
    const ScareAnimation *outage = scare_animation(SCARE_POWER_OUT);
    g.phase = G_SCARE;
    g.killer = SCARE_POWER_OUT;
    g.phase_ms = 0;
    capture_resident_count = 0;
    capture_missing_handle = 388;
    view();
    CHECK(!capture_count && game_view_loading(&g) && capture_resident_count == 21, 95);
    capture_missing_handle = 325;
    view();
    CHECK(!capture_count && game_view_loading(&g), 96);
    capture_missing_handle = -1;
    view();
    CHECK(placed(388, 398, 527) && capture_count == 1 && !count(591) && !count(344), 97);
    g.phase_ms = 560;
    view();
    CHECK(placed(outage->frames[20], 0, 2) && capture_count == 1, 98);
    g.phase = G_OUTAGE;
    g.outage_stage = 1;
    capture_resident_count = 0;
    view();
    CHECK(count(396) == 1 && !count(591) && capture_resident_count == 23, 99);
    extern unsigned char capture_demanded[8192];
    night();
    g.monitor = 1;
    view();
    for (int c = 0; c < 11; ++c)
        for (int f = 0; f < 2; ++f)
            CHECK(capture_demanded[scene_art(camera_map[c].object, 12, f)], 100);
    for (int f = 0; f < 11; ++f)
        CHECK(capture_demanded[scene_art(69, 0, f)], 101);
    g.elapsed = 600;
    view();
    CHECK(count(scene_art(75, 12, 1)) == 1 && count(scene_art(75, 12, 0)) == 0, 102);
    g.elapsed = 0;
    view();
    CHECK(count(scene_art(75, 12, 0)) == 1, 103);
    g.elapsed = 600;
    game_select_camera(&g, 10);
    view();
    CHECK(count(scene_art(84, 12, 0)) == 1 && g.camera_selected_ms == 600, 110);
    g.monitor_ms = 151;
    view();
    CHECK(count(591) == 1 && !count(90) && count(48) == 1 && count(49) == 1, 104);
    CHECK(placed(48, 64, 250) && placed(49, 64, 250), 105);
    g.monitor = 0;
    g.monitor_ms = 151;
    view();
    CHECK(count(591) == 1 && count(62) == 1 && count(49) == 1, 106);
    g.monitor = 1;
    g.monitor_ms = 0;
    game_view_prepare(&g, 1920, 22000);
    int previous = scene_art(9, 0, 24), wanted = scene_art(9, 0, 25);
    capture_missing_handle = wanted;
    view();
    CHECK(count(previous) == 1 && !count(wanted) && capture_demanded[wanted] &&
              capture_demanded[previous],
          107);
    capture_missing_handle = -1;
    night();
    game_input(&g, 0, game_touch(&g, 40, 280, 1), 0);
    CHECK(g.light[0] && g.sound_count == 1 && g.sounds[0].id == 1, 108);
    g.sound_count = 0;
    game_input(&g, 0, 0, 0);
    game_input(&g, 0, game_touch(&g, 920, 280, 1), 0);
    CHECK(g.light[1] && g.sound_count == 2 && g.sounds[1].id == 1, 109);
    night();
    g.monitor = 1;
    for (int i = 0; i < 4; ++i)
        g.save.options.plush[i] = 1;
    view();
    const int office_ready[] = {45,  46,  53,  55,  56,  68,  70,  132, 212, 213, 214, 455, 456,
                                201, 200, 204, 477, 495, 524, 496, 583, 579, 584, 585, 395, 396};
    for (unsigned i = 0; i < sizeof(office_ready) / sizeof(*office_ready); ++i)
        CHECK(capture_demanded[office_ready[i]], 111);
    capture_missing_handle = 212;
    CHECK(game_view_loading(&g), 112);
    capture_missing_handle = -1;
    g.monitor = 0;
    view();
    CHECK(count(201) == 1 && count(477) == 1 && count(495) == 1 && count(524) == 1 &&
              count(496) == 1 && count(45) == 1 && count(56) == 1,
          113);
    g.phase = G_OUTAGE;
    g.pan = 0;
    view();
    CHECK(count(395) == 1 && !count(591) && !count(201) && !game_view_loading(&g), 114);
    g.outage_stage = 1;
    capture_missing_handle = 396;
    view();
    CHECK(count(395) == 1 && !count(396) && game_view_loading(&g), 115);
    capture_missing_handle = -1;
    view();
    CHECK(count(396) == 1 && !count(395) && !game_view_loading(&g), 116);
    g.phase = G_PAUSE;
    g.return_phase = G_OUTAGE;
    g.outage_stage = 0;
    view();
    CHECK(count(395) == 1 && !count(591) && !count(201), 117);
    g.phase = G_OUTAGE;
    g.outage_stage = 2;
    view();
    CHECK(!capture_count && !game_view_loading(&g), 118);
    extern int capture_focus_handle;
    extern unsigned char capture_warmed[8192];
    night();
    g.monitor = 1;
    g.camera = 3;
    g.position[1] = 77;
    g.camera_flicker = 10;
    capture_missing_handle = 37;
    view();
    CHECK(count(41) == 1 && !count(37) && capture_focus_handle == 37, 119);
    capture_missing_handle = -1;
    g.camera = 10;
    int cold = scene_art(51, game_camera_animation(&g), 0);
    capture_missing_handle = cold;
    view();
    opaque = 0;
    for (int i = 0; i < capture_count; ++i)
        if (capture_commands[i][0] == noise && capture_commands[i][5] == (int)0xffffffffu)
            opaque = 1;
    CHECK(opaque && capture_focus_handle == cold, 120);
    capture_missing_handle = -1;
    g.camera = 0;
    g.position[0] = 122;
    view();
    Game probe = g;
    probe.camera = 10;
    CHECK(capture_warmed[scene_art(51, game_camera_animation(&probe), 0)] && !capture_warmed[344],
          121);
    night();
    g.door[0] = g.door[1] = 1;
    g.door_ms[0] = g.door_ms[1] = 76;
    view();
    CHECK(!count(88) && count(105) == 1 && !count(136) && count(137) == 1, 122);
    int opaque_doors = 0;
    for (int i = 0; i < capture_count; ++i)
        if ((capture_commands[i][0] == 105 || capture_commands[i][0] == 137) &&
            capture_commands[i][5] == (int)0xffffffffu)
            ++opaque_doors;
    CHECK(opaque_doors == 2, 123);
    for (int i = 0; i < 8; ++i) {
        const int ids[] = {18, 20, 12, 13, 14, 15, 16, 17};
        CHECK(capture_demanded[ids[i]], 124);
    }
    return 0;
}
