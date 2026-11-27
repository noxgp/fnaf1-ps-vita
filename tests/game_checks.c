#include "game.h"
#include "camera_map.h"
#include "scare_animation.h"
#include "touch_input.h"
#include "game_events.h"
#include "monitor_animation.h"
#include <string.h>
#include "perspective_mesh.h"
static Game g;
static void step(unsigned ms) {
    while (ms >= 20) {
        game_tick(&g, 20);
        g.sound_count = 0;
        ms -= 20;
    }
}
static void start(int n) {
    GameSave s;
    game_defaults(&s);
    game_init(&g, &s, 12345);
    game_start(&g, n);
    game_input(&g, IN_OK, 0, 0);
    g.sound_count = 0;
}
static int sound(int id, int channel, int volume) {
    for (int i = 0; i < g.sound_count; ++i)
        if (g.sounds[i].id == id && g.sounds[i].channel == channel && g.sounds[i].volume == volume)
            return 1;
    return 0;
}
#define CHECK(c, n)                                                                                \
    do {                                                                                           \
        if (!(c))                                                                                  \
            return 100 + (n);                                                                      \
    } while (0)
int run_game_checks(void) {
    GameSave s, t;
    unsigned char bytes[128];
    game_defaults(&s);
    game_encode_save(&s, bytes);
    CHECK(!game_decode_save(&t, bytes), 1);
    CHECK(t.next_night == 1 && t.options.volume == 80, 2);
    bytes[9] ^= 1;
    CHECK(game_decode_save(&t, bytes) < 0, 3);
    CHECK(t.next_night == 1, 4);
    s.options.language = 10;
    s.options.display = 2;
    s.options.plush[3] = 1;
    s.custom[3] = 20;
    s.unlocked = 7;
    s.next_night = 6;
    s.stars = 7;
    game_encode_save(&s, bytes);
    CHECK(!game_decode_save(&t, bytes) && t.options.language == 10 && t.custom[3] == 20 &&
              t.stars == 7 && t.next_night == 5 && t.unlocked == 7,
          5);
    start(2);
    CHECK(g.ai[0] == 0 && g.ai[1] == 3 && g.ai[2] == 1 && g.ai[3] == 1, 6);
    start(3);
    CHECK(g.ai[0] == 1 && g.ai[1] == 0 && g.ai[2] == 5 && g.ai[3] == 2, 7);
    start(4);
    CHECK((g.ai[0] == 1 || g.ai[0] == 2) && g.ai[3] == 6, 8);
    start(5);
    CHECK(g.ai[0] == 3 && g.ai[1] == 5 && g.ai[2] == 7 && g.ai[3] == 5, 9);
    start(6);
    CHECK(g.ai[0] == 4 && g.ai[1] == 10 && g.ai[2] == 12 && g.ai[3] == 6, 10);
    start(1);
    step(1000);
    CHECK(g.power == 998 && g.usage == 1, 11);
    game_input(&g, IN_R, IN_LIGHT, 0);
    CHECK(!g.door[0] && !g.door[1] && !g.light[0] && !g.light[1], 12);
    g.pan = 0;
    game_input(&g, IN_R, 0, 0);
    g.pan = OFFICE_PAN_MAX;
    game_input(&g, IN_R, 0, 0);
    g.pan = 0;
    game_input(&g, IN_LIGHT, IN_LIGHT, 0);
    CHECK(g.door[0] && g.door[1] && g.light[0] && !g.light[1], 185);
    step(1000);
    CHECK(g.power == 994 && g.usage == 4, 13);
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.monitor && !g.light[0], 14);
    step(1000);
    CHECK(g.power == 990 && g.usage == 4, 15);
    game_input(&g, IN_UP, 0, 0);
    CHECK(g.camera == 0 && game_camera_view(&g) == 1, 16);
    game_input(&g, IN_DOWN, 0, 0);
    CHECK(g.camera == 1 && game_camera_view(&g) == 2, 17);
    game_select_camera(&g, 0);
    CHECK(game_camera_animation(&g) == 17, 18);
    g.position[1] = 76;
    CHECK(game_camera_animation(&g) == 27, 19);
    g.position[2] = 76;
    g.position[0] = 76;
    CHECK(game_camera_animation(&g) != 17, 20);
    start(1);
    g.pan = 0;
    game_input(&g, IN_LIGHT, IN_LIGHT, 0);
    CHECK(g.light[0] && !g.light[1], 21);
    game_input(&g, IN_PAUSE, 0, 0);
    unsigned elapsed = g.elapsed;
    step(10000);
    CHECK(g.phase == G_PAUSE && g.elapsed == elapsed, 22);
    g.selection = 1;
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.phase == G_OPTIONS, 23);
    game_input(&g, IN_BACK, 0, 0);
    CHECK(g.phase == G_PAUSE, 24);
    game_input(&g, IN_BACK, 0, 0);
    CHECK(g.phase == G_PLAY, 25);
    start(1);
    g.power = 1;
    g.door[0] = 1;
    step(1000);
    CHECK(g.phase == G_OUTAGE && !g.door[0] && !g.monitor, 26);
    g.rng = 5;
    step(4980);
    CHECK(!g.outage_stage, 27);
    game_tick(&g, 20);
    CHECK(g.outage_stage == 1 && !g.phase_ms && sound(30, 0, 85), 198);
    step(45000);
    CHECK(g.phase == G_DEAD || g.phase == G_TITLE || g.phase == G_CREEPY_END, 28);
    start(1);
    g.save.options.unlimited = 1;
    g.power = 1;
    step(5000);
    CHECK(g.phase == G_PLAY && g.power == 1000, 29);
    for (int actor = 1; actor <= 2; ++actor) {
        start(1);
        g.position[actor] = actor == 1 ? 99 : 100;
        g.ai[actor] = 20;
        step(5000);
        CHECK(g.jam[actor - 1], 30 + actor);
        g.pan = actor == 1 ? 0 : OFFICE_PAN_MAX;
        game_input(&g, IN_R, 0, 0);
        CHECK(!g.door[actor - 1], 33 + actor);
        g.sound_count = 0;
        game_input(&g, IN_DIRECT_LIGHT, 0, 0);
        CHECK(!g.light[actor - 1] && sound(4, 3, 70), 237 + actor);
        step(5000);
        CHECK(g.phase == G_PLAY, 186 + actor);
        game_input(&g, IN_L, 0, 0);
        CHECK(g.monitor && (g.side_attack_armed & (1 << (actor - 1))), 188 + actor);
        step(400);
        game_input(&g, IN_L, 0, 0);
        game_input(&g, IN_PAUSE | IN_DIRECT_LIGHT | IN_DIRECT_MUTE, IN_LEFT, 0);
        CHECK(g.phase == G_PLAY && !g.monitor && !g.phone_muted, 240 + actor);
        step(360);
        CHECK(g.phase == G_PLAY && g.monitor_ms == 361, 243 + actor);
        step(20);
        CHECK(g.phase == G_SCARE && g.killer == actor && !g.phase_ms, 36 + actor);
        step(1800);
        CHECK(g.phase == G_DEAD, 39 + actor);
    }
    start(1);
    g.fox = 3;
    g.fox_ms = 24980;
    step(20);
    CHECK(g.phase == G_SCARE && g.killer == 3, 42);
    start(1);
    g.fox = 3;
    g.fox_ms = 24980;
    g.door[0] = 1;
    step(20);
    CHECK(g.phase == G_PLAY && g.fox < 2 && g.power == 989 && g.fox_hits == 1, 43);
    g.fox = 3;
    g.fox_ms = 24980;
    step(20);
    CHECK(g.power == 929 && g.fox_hits == 2, 44);
    start(1);
    g.monitor = 1;
    g.camera = 3;
    g.fox = 3;
    step(20);
    CHECK(g.fox == 4, 45);
    step(1680);
    CHECK(g.phase == G_SCARE, 46);
    start(1);
    g.golden = 2;
    step(1000);
    game_input(&g, IN_OK, 0, 0);
    CHECK(!g.golden, 47);
    start(1);
    g.golden = 2;
    step(5000);
    CHECK(g.phase == G_CREEPY_END && g.killer == 4, 48);
    start(1);
    g.position[0] = 81;
    g.monitor = 1;
    g.camera = 0;
    step(20);
    CHECK(g.position[0] == 122, 49);
    start(1);
    g.position[0] = 81;
    g.monitor = 1;
    g.camera = 0;
    g.door[1] = 1;
    step(20);
    CHECK(g.position[0] == 80, 50);
    start(1);
    g.position[0] = 81;
    g.monitor = 1;
    g.camera = 7;
    step(20);
    CHECK(g.position[0] == 81, 51);
    start(1);
    game_input(&g, IN_PHONE, 0, 0);
    CHECK(g.phone_muted, 52);
    static const unsigned hour_ends[] = {90000, 179000, 268000, 357000, 446000, 535000};
    static const unsigned move_intervals[] = {3020, 4970, 4980, 5010};
    for (int night = 1; night <= 7; ++night) {
        start(night);
        /* Isolate the clock and AI from power loss while leaving the AI active. */
        g.door[0] = g.door[1] = 1;
        int baseline_ai[4];
        memcpy(baseline_ai, g.ai, sizeof(baseline_ai));
        unsigned elapsed = 0;
        for (int hour = 1; hour <= 6; ++hour) {
            for (; elapsed < hour_ends[hour - 1]; elapsed += 20) {
                g.power = 1000;
                game_tick(&g, 20);
                g.sound_count = 0;
            }
            CHECK(g.elapsed == elapsed && g.hour == hour, 200 + hour);
            int shared_boost = (hour >= 3) + (hour >= 4);
            CHECK(g.ai[0] == baseline_ai[0] &&
                      g.ai[1] == baseline_ai[1] + (hour >= 2) + shared_boost &&
                      g.ai[2] == baseline_ai[2] + shared_boost &&
                      g.ai[3] == baseline_ai[3] + shared_boost,
                  207);
            for (int actor = 0; actor < 4; ++actor)
                CHECK(g.move_ms[actor] == (elapsed - (hour == 6 ? 20 : 0)) % move_intervals[actor],
                      208 + actor);
        }
        CHECK(g.phase == G_WIN && g.hour == 6, 53 + night);
        if (night == 6)
            CHECK(g.position[0] != 75 && g.position[1] != 75 && g.position[2] != 75 && g.fox_hits,
                  212);
        if (night == 5)
            CHECK(g.save.stars == 1 && g.save.unlocked == 6, 61);
        if (night == 6)
            CHECK(g.save.stars == 2 && g.save.unlocked == 7, 62);
    }
    start(1);
    g.save.options.fast = 1;
    g.save.options.unlimited = 1;
    game_start(&g, 1);
    game_input(&g, IN_OK, 0, 0);
    for (unsigned i = 0; i < 13375; ++i) {
        for (int a = 0; a < 4; ++a)
            g.ai[a] = 0;
        game_tick(&g, 20);
        g.sound_count = 0;
    }
    CHECK(g.phase == G_WIN, 63);
    CHECK(g.save.next_night == 1 && !g.save.stars, 64);
    start(1);
    g.phase = G_OPTIONS;
    g.options_parent = G_TITLE;
    g.selection = 5;
    game_input(&g, IN_LEFT, 0, 0);
    CHECK(g.save.options.language == 10, 65);
    g.phase = G_AUDIO_OPTIONS;
    g.selection = 1;
    game_input(&g, IN_RIGHT, 0, 0);
    CHECK(g.save.options.volume == 90, 66);
    start(1);
    g.phase = G_CUSTOM;
    g.selection = 0;
    g.save.custom[0] = 20;
    game_input(&g, IN_RIGHT, 0, 0);
    CHECK(g.save.custom[0] == 20, 67);
    g.save.custom[0] = 1;
    g.save.custom[1] = 9;
    g.save.custom[2] = 8;
    g.save.custom[3] = 7;
    game_start(&g, 7);
    CHECK(g.phase == G_CREEPY_END && g.killer == 4, 68);
    start(2);
    step(6000);
    CHECK(g.power == 992, 69);
    start(1);
    g.hour = 5;
    g.hour_ms = 88980;
    g.phase = G_OUTAGE;
    game_tick(&g, 20);
    CHECK(g.phase == G_WIN, 70);
    start(1);
    g.hour = 1;
    g.hour_ms = 88980;
    game_tick(&g, 20);
    CHECK(g.hour == 2 && g.ai[1] == 1, 71);
    g.hour_ms = 88980;
    game_tick(&g, 20);
    CHECK(g.hour == 3 && g.ai[1] == 2 && g.ai[2] == 1 && g.ai[3] == 1, 72);
    g.hour_ms = 88980;
    game_tick(&g, 20);
    CHECK(g.hour == 4 && g.ai[1] == 3 && g.ai[2] == 2 && g.ai[3] == 2, 73);
    start(1);
    g.position[1] = 99;
    g.ai[1] = 20;
    g.door[0] = 1;
    step(5000);
    CHECK(g.position[1] == 76 && !g.jam[0], 74);
    start(1);
    g.position[2] = 100;
    g.ai[2] = 20;
    g.door[1] = 1;
    step(5000);
    CHECK(g.position[2] == 80 && !g.jam[1], 75);
    start(1);
    g.ai[3] = 20;
    g.monitor = 1;
    step(10000);
    CHECK(g.fox == 0 && g.fox_cooldown > 0, 76);
    g.monitor = 0;
    g.fox_cooldown = 10000;
    step(5020);
    CHECK(g.fox == 0, 77);
    s.options.language = 11;
    game_encode_save(&s, bytes);
    CHECK(game_decode_save(&t, bytes) < 0 && t.options.language == 10, 78);
    start(1);
    g.dirty = 1;
    g.save.next_night = 4;
    game_start(&g, 4);
    CHECK(g.dirty && g.save.next_night == 4, 79);
    game_defaults(&s);
    game_init(&g, &s, 123);
    CHECK(g.phase == G_WARNING && !g.sound_count, 80);
    step(WARNING_DURATION_MS - 20);
    CHECK(g.phase == G_WARNING, 81);
    step(20);
    CHECK(g.phase == G_TITLE, 82);
    CHECK(game_title_action(&g, 0) == TITLE_NEW && game_title_action(&g, 2) == TITLE_OPTIONS &&
              game_title_action(&g, 3) == TITLE_UNLOCKS,
          83);
    g.save.unlocked = 6;
    CHECK(game_title_action(&g, 2) == TITLE_NIGHT6 && game_title_action(&g, 3) == TITLE_OPTIONS,
          84);
    g.save.unlocked = 7;
    CHECK(game_title_action(&g, 3) == TITLE_CUSTOM && game_title_action(&g, 4) == TITLE_OPTIONS &&
              game_title_action(&g, 5) == TITLE_UNLOCKS,
          85);
    g.save.next_night = 4;
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.phase == G_CONFIRM_NEW && g.save.next_night == 4, 86);
    game_input(&g, IN_BACK, 0, 0);
    CHECK(g.phase == G_TITLE && g.save.next_night == 4, 87);
    g.selection = 0;
    game_input(&g, IN_OK, 0, 0);
    game_input(&g, IN_DOWN, 0, 0);
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.phase == G_NEWSPAPER && g.save.next_night == 1 && g.save.unlocked == 7, 88);
    game_init(&g, &s, 123);
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.phase == G_TITLE, 89);
    g.selection = 1;
    g.phase_ms = 1200;
    g.sound_count = 0;
    game_input(&g, IN_BACK, 0, 0);
    CHECK(g.phase == G_TITLE && g.selection == 1 && g.phase_ms == 1200 && !g.sound_count, 236);
    game_input(&g, IN_HELP, 0, 0);
    CHECK(g.diagnostics, 90);
    game_start(&g, 1);
    CHECK(g.diagnostics, 91);
    g.phase = G_OPTIONS;
    g.options_parent = G_PAUSE;
    g.selection = 6;
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.phase == G_AUDIO_OPTIONS, 92);
    game_input(&g, IN_BACK, 0, 0);
    CHECK(g.phase == G_OPTIONS && g.options_parent == G_PAUSE, 93);
    game_input(&g, IN_BACK, 0, 0);
    CHECK(g.phase == G_PAUSE, 94);
    game_init(&g, &s, 123);
    game_input(&g, IN_PAUSE, 0, 0);
    CHECK(g.phase == G_TITLE && !g.quit, 95);
    game_input(&g, IN_PAUSE, 0, 0);
    CHECK(g.phase == G_CONFIRM_NEW && !g.quit, 96);
    start(1);
    game_input(&g, 0, IN_RIGHT, 0);
    step(200);
    int pan = g.pan;
    CHECK(pan == 312, 97);
    start(1);
    for (int i = 0; i < 10; ++i) {
        game_input(&g, 0, IN_RIGHT, 0);
        step(20);
    }
    CHECK(g.pan == pan, 98);
    start(1);
    game_input(&g, 0, 0, 127);
    step(200);
    CHECK(g.pan == pan, 253);
    start(1);
    game_input(&g, 0, IN_LEFT, 0);
    step(1000);
    CHECK(g.pan == 0, 99);
    PerspectiveVertex mesh[PERSPECTIVE_VERTICES];
    unsigned vertices = perspective_mesh(mesh, -120, 2, 1200, 540, 480);
    CHECK(vertices == PERSPECTIVE_VERTICES && mesh[0].x == 0 && mesh[vertices - 1].x == 960 &&
              mesh[0].u == .1f && mesh[vertices - 1].u == .9f,
          100);
    for (int i = 0; i < PERSPECTIVE_VERTICES; i += 2) {
        CHECK(mesh[i].x == mesh[i + 1].x && mesh[i].y < mesh[i + 1].y && mesh[i].v == 0 &&
                  mesh[i + 1].v == 1,
              101);
        CHECK(mesh[i].y <= 2 && mesh[i + 1].y >= 542, 102);
        if (i)
            CHECK(mesh[i].x > mesh[i - 2].x, 103);
    }
    CHECK(office_height_scale(0, 480) == office_height_scale(960, 480) &&
              office_height_scale(1080, 480) == office_height_scale(960, 480) &&
              office_height_scale(960, 480) < 1.2f,
          246);
    PerspectiveVertex patch[PERSPECTIVE_VERTICES];
    unsigned patch_vertices = perspective_mesh(patch, 72.75f, 2, 316.5f, 540, 480);
    CHECK(patch_vertices < vertices && patch_vertices > 4 && patch[0].u == 0 &&
              patch[patch_vertices - 1].u == 1,
          254);
    for (unsigned i = 2; i + 2 < patch_vertices; i += 2) {
        unsigned base = (unsigned)(patch[i].x / 30.0f) * 2;
        CHECK(patch[i].x == mesh[base].x && patch[i].y == mesh[base].y &&
                  patch[i + 1].y == mesh[base + 1].y,
              255);
    }
    CHECK(camera_map_move(0, 0, 1) == 1 && camera_map_move(1, 0, 1) == 2, 104);
    CHECK(camera_map_move(3, 0, 1) == 4 && camera_map_move(3, 1, 0) == 6, 105);
    CHECK(camera_map_move(6, 0, 1) == 7 && camera_map_move(10, 0, 1) == 9, 106);
    CHECK(camera_map_move(0, 0, -1) == 0 && camera_map_move(4, 0, 1) == 4, 107);
    CHECK(camera_map_hit(983 * 3 / 4, 2 + 603 * 3 / 4) == 3 &&
              camera_map_hit(983 * 3 / 4, 2 + 643 * 3 / 4) == 4,
          108);
    CHECK(camera_map_hit(0, 0) == -1 && camera_map_move(0, 0, 0) == 0, 109);
    start(1);
    step(1000);
    CHECK(game_camera_pan(&g) == 60, 110);
    g.camera_scan = 320000;
    CHECK(game_camera_pan(&g) == 320, 111);
    g.camera_scan = 419999;
    CHECK(game_camera_pan(&g) == 320, 112);
    g.camera_scan = 500000;
    CHECK(game_camera_pan(&g) == 240, 113);
    g.camera_scan = 800000;
    CHECK(game_camera_pan(&g) == 0, 114);
    g.camera_scan = 320000;
    game_select_camera(&g, 5);
    CHECK(game_camera_pan(&g) == 0 && g.camera_scan == 320000, 115);
    game_select_camera(&g, 0);
    CHECK(game_camera_pan(&g) == 320, 116);
    game_input(&g, IN_PAUSE, 0, 0);
    step(2000);
    CHECK(g.camera_scan == 320000, 117);
    start(1);
    g.monitor = 1;
    g.sound_count = 0;
    unsigned picture = g.picture;
    game_input(&g, IN_UP, 0, 0);
    CHECK(!g.sound_count && g.picture == (int)picture && !g.static_ms, 118);
    CHECK(scare_frame(0, 100) == 3 && scare_frame(1, 100) == 4 && scare_frame(2, 100) == 5 &&
              scare_frame(3, 100) == 3,
          119);
    CHECK(scare_frame(0, 1040) == 30 && scare_frame(0, 1680) == 30 && scare_frame(3, 620) == 17 &&
              scare_frame(3, 1680) == 17,
          120);
    CHECK(scare_frame(1, 250) == 0 && scare_frame(2, 270) == 0 && !scare_animation(4), 121);
    unsigned sw = 1200, sh = 540;
    animation_texture_size(344, &sw, &sh);
    CHECK(sw == 800 && sh == 360, 122);
    sw = 1200;
    sh = 540;
    animation_texture_size(591, &sw, &sh);
    CHECK(sw == 1200 && sh == 540, 123);
    animation_texture_size(266, &sw, &sh);
    CHECK(sw == 360 && sh == 162, 233);
    TouchArea area = {20, 30, 1939, 1117};
    int tx, ty;
    CHECK(touch_to_game(area, 980, 574, 0, &tx, &ty) && tx == 480 && ty == 272, 124);
    CHECK(touch_to_game(area, 980, 574, 1, &tx, &ty) && tx == 480 && ty == 272, 125);
    CHECK(touch_to_game(area, 980, 574, 2, &tx, &ty) && tx == 480 && ty == 272, 126);
    CHECK(!touch_to_game(area, 20, 30, 2, &tx, &ty) && !touch_to_game(area, 0, 0, 0, &tx, &ty),
          127);
    TouchContacts contacts = {0};
    touch_begin(&contacts);
    CHECK(touch_contact(&contacts, 4) && touch_contact(&contacts, 9), 128);
    touch_begin(&contacts);
    CHECK(!touch_contact(&contacts, 4) && touch_contact(&contacts, 10), 129);
    touch_begin(&contacts);
    CHECK(touch_contact(&contacts, 9) && !touch_contact(&contacts, 4), 130);
    touch_begin(&contacts);
    touch_begin(&contacts);
    CHECK(touch_contact(&contacts, 4), 131);
    start(1);
    g.monitor = 1;
    g.camera = 3;
    g.camera_flicker = 10;
    CHECK(game_camera_animation(&g) == 14, 132);
    g.camera_flicker = 3;
    CHECK(game_camera_animation(&g) == 15, 133);
    g.position[1] = 77;
    CHECK(game_camera_animation(&g) == 33, 134);
    g.camera_flicker = 4;
    CHECK(game_camera_animation(&g) == 14, 135);
    int variants[3];
    CHECK(game_camera_variants(&g, variants) == 2 && variants[0] == 14 && variants[1] == 33, 136);
    g.fox = 3;
    CHECK(game_camera_animation(&g) == 51 && game_camera_variants(&g, variants) == 1, 137);
    start(4);
    g.monitor = 1;
    g.camera = 4;
    g.position[1] = 78;
    g.camera_twitch = 24;
    CHECK(game_camera_animation(&g) == 30, 138);
    g.camera_twitch = 25;
    CHECK(game_camera_animation(&g) == 58, 139);
    g.camera_twitch = 29;
    CHECK(game_camera_animation(&g) == 59, 140);
    CHECK(game_camera_variants(&g, variants) == 3 && variants[0] == 30 && variants[1] == 58 &&
              variants[2] == 59,
          141);
    g.night = 3;
    CHECK(game_camera_animation(&g) == 30 && game_camera_variants(&g, variants) == 1, 142);
    g.night = 4;
    g.camera = 7;
    g.position[2] = 81;
    g.camera_twitch = 24;
    CHECK(game_camera_animation(&g) == 38, 143);
    g.camera_twitch = 25;
    CHECK(game_camera_animation(&g) == 56, 144);
    g.camera_twitch = 30;
    CHECK(game_camera_animation(&g) == 57, 145);
    CHECK(game_camera_variants(&g, variants) == 3 && variants[0] == 38 && variants[1] == 56 &&
              variants[2] == 57,
          146);
    /* Stable poster rolls are independent of per-frame lamp/head randoms. */
    g.camera = 6;
    g.position[0] = g.position[2] = 75;
    g.picture = 100;
    CHECK(game_camera_animation(&g) == 68, 147);
    g.camera = 10;
    CHECK(game_camera_animation(&g) == 21 && game_camera_variants(&g, variants) == 1, 148);
    start(1);
    g.monitor = 1;
    g.camera = 3;
    unsigned visual_before = g.visual_rng;
    game_tick(&g, 16);
    CHECK(g.visual_rng == visual_before && g.flicker_clock == 960, 149);
    game_tick(&g, 1);
    CHECK(g.visual_rng != visual_before && g.flicker_clock == 20 && g.twitch_ms == 17, 150);
    int stable = g.picture;
    int seen_dark = 0, seen_lit = 0;
    for (int i = 0; i < 100; ++i) {
        game_tick(&g, 20);
        seen_dark |= game_camera_animation(&g) == 14;
        seen_lit |= game_camera_animation(&g) == 15;
    }
    CHECK(seen_dark && seen_lit && g.picture == stable && g.camera == 3, 151);
    int lamp = g.camera_flicker, head = g.camera_twitch;
    visual_before = g.visual_rng;
    game_input(&g, IN_PAUSE, 0, 0);
    step(2000);
    CHECK(g.visual_rng == visual_before && g.camera_flicker == lamp && g.camera_twitch == head,
          152);
    game_input(&g, IN_BACK, 0, 0);
    game_select_camera(&g, 0);
    CHECK(g.visual_rng == visual_before && g.picture == stable, 153);
    game_select_camera(&g, 3);
    CHECK(g.camera_flicker == lamp && g.camera_twitch == head, 154);
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.picture >= 1 && g.picture <= 100 && g.visual_rng != visual_before, 155);
    start(1);
    g.monitor = 1;
    g.camera = 0;
    for (int i = 0; i < 50; ++i)
        game_tick(&g, 20);
    unsigned visual_end = g.visual_rng;
    int lamp_end = g.camera_flicker, head_end = g.camera_twitch;
    start(1);
    g.monitor = 1;
    g.camera = 10;
    for (int i = 0; i < 100; ++i)
        game_tick(&g, 10);
    CHECK(g.visual_rng == visual_end && g.camera_flicker == lamp_end && g.camera_twitch == head_end,
          156);
    start(1);
    g.phase = G_OUTAGE;
    g.power = 0;
    g.outage_stage = 1;
    g.phase_ms = 19980;
    game_tick(&g, 20);
    CHECK(g.phase == G_OUTAGE && g.outage_stage == 2 && !g.phase_ms, 157);
    CHECK(g.sound_count == 24, 158);
    for (int i = 0; i < g.sound_count; ++i)
        CHECK(g.sounds[i].id == -1, 158);
    g.sound_count = 0;
    g.phase_ms = 19980;
    game_tick(&g, 20);
    CHECK(g.phase == G_SCARE && g.killer == SCARE_POWER_OUT && g.sounds[g.sound_count - 1].id == 15,
          159);
    CHECK(scare_duration(SCARE_POWER_OUT) == 584 && scare_duration(0) == 1700, 160);
    const ScareAnimation *outage = scare_animation(SCARE_POWER_OUT);
    CHECK(outage && outage->count == 21 && outage->frames[0] == 388 && outage->frames[20] == 325 &&
              !outage->loop,
          161);
    CHECK(scare_frame(SCARE_POWER_OUT, 100) == 3 && scare_frame(SCARE_POWER_OUT, 560) == 20, 162);
    step(580);
    CHECK(g.phase == G_SCARE, 163);
    step(20);
    CHECK(g.phase == G_DEAD, 164);
    sw = 960;
    sh = 540;
    animation_texture_size(388, &sw, &sh);
    CHECK(sw == 640 && sh == 360, 165);
    start(1);
    g.phase = G_OUTAGE;
    g.power = 0;
    g.rng = 1;
    step(5000);
    CHECK(!g.outage_stage && !g.outage_roll, 213);
    g.phase_ms = 19980;
    game_tick(&g, 20);
    CHECK(g.outage_stage == 1 && !g.phase_ms && sound(30, 0, 85), 214);
    start(1);
    g.pan = 0;
    g.position[1] = 99;
    game_input(&g, IN_LIGHT, IN_LIGHT, 0);
    CHECK(game_light_visible(&g, 0) && sound(1, A_DOOR_LIGHT, 100) && sound(31, A_SIGHTING, 80), 166);
    g.sound_count = 0;
    game_input(&g, 0, IN_LIGHT, 0);
    game_input(&g, 0, 0, 0);
    CHECK(g.light[0] && !g.sound_count, 167);
    game_input(&g, IN_LIGHT, 0, 0);
    CHECK(!g.light[0] && g.sound_count == 1 && sound(-1, A_DOOR_LIGHT, 0), 168);
    g.sound_count = 0;
    game_input(&g, IN_LIGHT, 0, 0);
    CHECK(g.light[0] && g.sound_count == 1 && !sound(31, A_SIGHTING, 80), 169);
    g.sound_count = 0;
    g.visual_rng = 10;
    g.flicker_clock = 0;
    game_tick(&g, 20);
    CHECK(g.light[0] && !game_light_visible(&g, 0) && g.usage == 2 &&
              sound(-2, A_DOOR_LIGHT, 0) && !sound(-1, A_DOOR_LIGHT, 0),
          191);
    g.sound_count = 0;
    g.visual_rng = 1;
    game_tick(&g, 20);
    CHECK(game_light_visible(&g, 0) && sound(-2, A_DOOR_LIGHT, 100) &&
              !sound(1, A_DOOR_LIGHT, 100),
          192);
    g.pan = OFFICE_PAN_MAX;
    game_input(&g, IN_LIGHT, 0, 0);
    CHECK(g.light[1] && game_light_visible(&g, 1) && !g.light[0], 170);
    g.sound_count = 0;
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.monitor && g.monitor_ms == 1 && !g.light[0] && sound(-1, A_DOOR_LIGHT, 0), 171);
    step(340);
    CHECK(g.monitor_ms != 0, 172);
    step(40);
    CHECK(!g.monitor_ms && g.camera_cut_ms, 173);
    MonitorFrame m = monitor_frame(151);
    CHECK(m.frame == 4 && m.next == 5 && m.fraction > .49f && m.fraction < .51f, 174);
    CHECK(monitor_frame(0).frame == 0 && monitor_frame(999).frame == 10 &&
              monitor_frame(999).next == 10,
          175);
    unsigned reversed = monitor_reverse(151);
    m = monitor_frame(reversed);
    CHECK(m.frame == 5 && m.fraction > .48f && m.fraction < .52f, 176);
    game_input(&g, IN_OK, 0, 0);
    step(140);
    game_input(&g, IN_OK, 0, 0);
    CHECK(!g.monitor && g.monitor_ms == 141, 177);
    start(1);
    g.power = 0;
    g.pan = 320;
    g.pan_speed = 200;
    g.pan_fraction = 99;
    g.monitor = 1;
    g.monitor_ms = 100;
    g.door_ms[0] = 200;
    game_tick(&g, 20);
    CHECK(g.phase == G_OUTAGE && g.pan == 320 && !g.pan_speed && !g.pan_fraction, 178);
    CHECK(!g.monitor && !g.monitor_ms && !g.door_ms[0] && !g.door_ms[1] && !g.outage_stage, 179);
    game_input(&g, 0, IN_LEFT, 0);
    step(100);
    CHECK(g.pan == 244, 247);
    start(1);
    game_select_camera(&g, 10);
    CHECK(g.camera_cut_ms == CAMERA_CUT_MS && !g.static_ms, 180);
    g.pan = 0;
    game_input(&g, IN_R, 0, 0);
    step(200);
    game_input(&g, IN_R, 0, 0);
    CHECK(g.door[0] && g.door_ms[0] == 201, 181);
    SpriteTween d = sprite_tween(226, 16, 20);
    CHECK(d.frame == 4 && d.next == 5 && d.fraction > .49f && d.fraction < .51f, 182);
    g.pan = OFFICE_PAN_MAX;
    game_input(&g, IN_R, 0, 0);
    game_input(&g, IN_L, 0, 0);
    game_input(&g, IN_R, IN_LIGHT, 0);
    CHECK(g.monitor && g.door[1] && !g.light[1], 193);
    step(DOOR_DURATION_MS - 20);
    CHECK(g.door_ms[1] != 0, 183);
    step(20);
    CHECK(!g.door_ms[1], 184);
    game_input(&g, IN_R, IN_LIGHT, 0);
    CHECK(g.door[1] && !g.light[1], 194);
    int feeds[3];
    start(5);
    g.camera = 1;
    g.position[1] = g.position[2] = 76;
    g.branch[0] = g.branch[1] = 1;
    CHECK(game_camera_feeds(&g, feeds) == 1 && feeds[0] == 40, 195);
    g.branch[1] = 2;
    CHECK(game_camera_feeds(&g, feeds) == 1 && feeds[0] == 55, 234);
    g.position[2] = 83;
    CHECK(game_camera_feeds(&g, feeds) == 1 && feeds[0] == 28, 235);
    g.camera = 7;
    g.position[0] = g.position[2] = 81;
    g.camera_twitch = 27;
    CHECK(game_camera_feeds(&g, feeds) == 1 && feeds[0] == 56, 248);
    g.position[2] = 83;
    CHECK(game_camera_feeds(&g, feeds) == 1 && feeds[0] == 64, 249);

    start(5);
    for (int a = 0; a < 4; ++a)
        g.ai[a] = -10;
    step(534980);
    CHECK(g.phase == G_PLAY && g.hour == 5 && g.power == 287, 250);
    game_tick(&g, 20);
    CHECK(g.phase == G_WIN && g.save.next_night == 5 && g.save.unlocked == 6 &&
              (g.save.stars & 1),
          251);
    step(8980);
    game_tick(&g, 20);
    CHECK(g.phase == G_ENDING && sound(30, 3, 100) && g.sounds[g.sound_count - 1].loop, 252);

    game_defaults(&s);
    game_encode_save(&s, bytes);
    CHECK(!bytes[26] && !bytes[27] && !game_decode_save(&t, bytes) &&
              t.options.control_layout == CONTROL_CONTEXTUAL &&
              game_control_binding(&t.options, CONTROL_DOOR) == BUTTON_R,
          215);
    start(1);
    g.phase = G_OPTIONS;
    g.selection = OPTIONS_CONTROLS;
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.phase == G_CONTROL_OPTIONS && !g.selection, 216);
    game_input(&g, IN_RIGHT, 0, 0);
    CHECK(g.save.options.control_layout == CONTROL_CLASSIC, 217);
    game_input(&g, IN_BACK, 0, 0);
    CHECK(g.phase == G_OPTIONS && g.selection == OPTIONS_CONTROLS, 218);
    game_start(&g, 1);
    game_input(&g, IN_OK, 0, 0);
    g.pan = 0;
    game_input(&g, IN_R, IN_BACK, 0);
    CHECK(!g.monitor && !g.door[0] && !g.door[1] && !g.light[0] && !g.light[1], 219);
    game_input(&g, IN_L | IN_LIGHT, IN_LIGHT, 0);
    CHECK(!g.monitor && g.door[0] && g.light[0], 220);
    g.pan = OFFICE_PAN_MAX;
    game_input(&g, IN_R | IN_BACK, IN_BACK, 0);
    CHECK(!g.monitor && g.door[1] && g.light[1] && !g.light[0], 221);
    game_input(&g, IN_OK, IN_BACK, 0);
    CHECK(g.monitor && !g.light[1], 222);

    g.phase = G_CONTROL_OPTIONS;
    g.selection = 0;
    game_input(&g, IN_RIGHT, 0, 0);
    CHECK(g.save.options.control_layout == CONTROL_CUSTOM, 223);
    g.selection = CONTROLS_FIRST_ACTION + CONTROL_MONITOR;
    game_input(&g, IN_OK, 0, 0);
    game_input(&g, IN_R, 0, 0);
    CHECK(!g.binding_capture && game_control_binding(&g.save.options, CONTROL_MONITOR) == BUTTON_R &&
              game_control_binding(&g.save.options, CONTROL_DOOR) == BUTTON_CROSS,
          224);
    g.selection = CONTROLS_FIRST_ACTION + CONTROL_MUTE;
    game_input(&g, IN_OK, 0, 0);
    game_input(&g, IN_HELP, 0, 0);
    CHECK(!g.binding_capture && game_control_binding(&g.save.options, CONTROL_MUTE) == BUTTON_TRIANGLE,
          225);
    g.save.next_night = g.save.unlocked = 4;
    game_encode_save(&g.save, bytes);
    CHECK(!game_decode_save(&t, bytes) && t.next_night == 4 &&
              t.options.control_layout == CONTROL_CUSTOM &&
              game_control_binding(&t.options, CONTROL_MONITOR) == BUTTON_R &&
              game_control_binding(&t.options, CONTROL_DOOR) == BUTTON_CROSS,
          226);
    game_start(&g, 4);
    game_input(&g, IN_OK, 0, 0);
    g.pan = 0;
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.door[0] && !g.monitor, 227);
    game_input(&g, IN_R, 0, 0);
    CHECK(g.monitor, 228);
    start(1);
    g.phone_finished = 1;
    game_input(&g, IN_PHONE, 0, 0);
    CHECK(!game_phone_can_mute(&g) && !g.phone_muted && !g.sound_count, 229);
    for (int actor = 1; actor <= 2; ++actor) {
        start(1);
        g.monitor = 1;
        g.camera = actor == 1 ? 3 : 9;
        g.position[actor] = actor == 1 ? 77 : 83;
        g.side_visited[actor - 1] = (1u << (79 - 75)) | (1u << (84 - 75));
        g.ai[actor] = 20;
        g.move_ms[actor] = 4960;
        game_tick(&g, 20);
        CHECK(g.position[actor] == (actor == 1 ? 78 : 80), 230);
        CHECK(g.static_ms == CAMERA_MOVE_STATIC_MS && g.camera_cut_ms == CAMERA_CUT_MS, 231);
        g.static_ms = 0;
        g.camera_cut_ms = 0;
        g.camera = 0;
        g.move_ms[actor] = 4960;
        game_tick(&g, 20);
        CHECK(!g.static_ms && !g.camera_cut_ms, 232);
    }
    return 0;
}
