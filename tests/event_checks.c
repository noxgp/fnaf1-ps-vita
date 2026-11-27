#include "game.h"
#include "game_events.h"
static Game g;
static void fresh(int night) {
    GameSave s;
    game_defaults(&s);
    game_init(&g, &s, 12345);
    game_start(&g, night);
    game_input(&g, IN_OK, 0, 0);
    for (int a = 0; a < 4; ++a)
        g.ai[a] = 0;
    g.sound_count = 0;
}
static int sound(int id, int channel) {
    for (int i = 0; i < g.sound_count; ++i)
        if (g.sounds[i].id == id && g.sounds[i].channel == channel)
            return 1;
    return 0;
}
static void step(unsigned ms) {
    while (ms >= 20) {
        game_tick(&g, 20);
        g.sound_count = 0;
        ms -= 20;
    }
}
#define CHECK(c, n)                                                                                \
    do {                                                                                           \
        if (!(c))                                                                                  \
            return 800 + (n);                                                                      \
    } while (0)
int run_event_checks(void) {
    fresh(1);
    game_set_date(&g, 10, 31);
    CHECK(g.halloween, 1);
    game_start(&g, 2);
    CHECK(g.halloween, 2);
    game_set_date(&g, 10, 30);
    CHECK(!g.halloween, 3);
    fresh(1);
    g.event_rng = 623;
    g.event_second_ms = 980;
    game_events_tick(&g, 20);
    CHECK(g.hallucination_left == 1667, 4);
    unsigned remaining = g.hallucination_left;
    game_input(&g, IN_PAUSE, 0, 0);
    step(1000);
    CHECK(g.hallucination_left == remaining, 5);
    game_input(&g, IN_BACK, 0, 0);
    step(1680);
    CHECK(!g.hallucination_left, 6);
    fresh(1);
    g.event_rng = 19704;
    g.event_second_ms = 980;
    game_events_tick(&g, 20);
    CHECK(g.golden == 1 && !g.golden_ms, 7);
    g.monitor = 1;
    g.camera = 4;
    g.position[1] = 78;
    game_events_tick(&g, 20);
    CHECK(g.golden == 1 && !sound(38, A_GOLDEN), 8);
    g.position[1] = 75;
    game_events_tick(&g, 20);
    CHECK(g.golden == 2 && sound(38, A_GOLDEN), 9);
    game_input(&g, IN_OK, 0, 0);
    game_tick(&g, 20);
    CHECK(g.golden_ms == 20 && g.hallucination_left && g.phase == G_PLAY, 10);
    game_input(&g, IN_OK, 0, 0);
    CHECK(!g.monitor && g.golden == 2, 11);
    step(360);
    game_input(&g, IN_OK, 0, 0);
    CHECK(!g.golden && !g.golden_ms && g.monitor, 35);
    fresh(1);
    g.golden = 2;
    step(4980);
    CHECK(g.phase == G_PLAY, 12);
    game_tick(&g, 20);
    CHECK(g.phase == G_CREEPY_END && sound(46, 6), 13);
    step(980);
    CHECK(g.phase == G_CREEPY_END, 14);
    game_tick(&g, 20);
    CHECK(g.phase == G_TITLE, 15);
    GameSave s;
    game_defaults(&s);
    game_init(&g, &s, 123);
    g.startup_roll = 0;
    game_input(&g, IN_OK, 0, 0);
    CHECK(g.phase == G_CREEPY_START, 16);
    step(9980);
    CHECK(g.phase == G_CREEPY_START, 17);
    game_tick(&g, 20);
    CHECK(g.phase == G_TITLE, 18);
    fresh(1);
    g.phase = G_DEAD;
    g.phase_ms = 9980;
    g.dead_random_ms = 980;
    g.event_rng = 22510;
    game_tick(&g, 20);
    CHECK(g.phase == G_CREEPY_END, 19);
    fresh(1);
    g.phase = G_DEAD;
    g.phase_ms = 9980;
    g.dead_random_ms = 0;
    g.dead_roll = 2;
    game_tick(&g, 20);
    CHECK(g.phase == G_TITLE, 20);
    fresh(1);
    g.event_rng = 10;
    g.event_four_ms = 3980;
    game_events_tick(&g, 20);
    CHECK(sound(21, A_PIRATE), 21);
    fresh(1);
    g.event_rng = 10;
    g.event_five_ms = 4980;
    game_events_tick(&g, 20);
    CHECK(sound(29, A_CREEK) && g.ambience_cooldown_ms == 90000, 22);
    g.sound_count = 0;
    g.event_rng = 10;
    g.event_five_ms = 4980;
    game_events_tick(&g, 20);
    CHECK(!sound(29, A_CREEK), 34);
    fresh(1);
    g.event_rng = 17;
    g.event_ten_ms = 9980;
    game_events_tick(&g, 20);
    CHECK(sound(39, A_DRONE), 23);
    fresh(1);
    g.position[0] = 83;
    game_events_tick(&g, 20);
    CHECK(sound(30, A_FREDDY_MUSIC), 24);
    g.sound_count = 0;
    g.position[0] = 122;
    game_events_tick(&g, 20);
    CHECK(sound(59, A_INSIDE), 25);
    fresh(1);
    g.position[1] = 77;
    game_events_levels(&g);
    CHECK(g.event_volumes[A_DANGER] == 31, 26);
    g.position[2] = 81;
    game_events_levels(&g);
    CHECK(g.event_volumes[A_DANGER] == 51, 27);
    g.fox = 2;
    game_events_levels(&g);
    CHECK(g.event_volumes[A_DANGER] == 76, 28);
    g.position[0] = 122;
    game_events_levels(&g);
    CHECK(g.event_volumes[A_DANGER] == 101, 29);
    g.monitor = 1;
    g.camera = 9;
    g.position[2] = 83;
    game_events_levels(&g);
    CHECK(g.event_volumes[A_KITCHEN] == 76 && g.event_volumes[2] == 51, 30);
    g.sound_count = 0;
    game_events_levels(&g);
    CHECK(!g.sound_count, 31);
    fresh(4);
    g.position[1] = 78;
    g.monitor = 1;
    g.camera = 4;
    g.murmur_ms = 80;
    game_events_tick(&g, 20);
    CHECK(g.murmur_volume >= 1 && g.murmur_volume <= 81 && g.murmur_volume % 20 == 1, 32);
    unsigned elapsed = g.event_four_ms;
    game_input(&g, IN_PAUSE, 0, 0);
    step(1000);
    CHECK(g.event_four_ms == elapsed, 33);
    fresh(1);
    game_sound_volume(&g, A_PIRATE, 25);
    CHECK(sound(-2, A_PIRATE), 34);
    fresh(1);
    g.jam[0] = g.jam[1] = 1;
    g.monitor = 1;
    g.side_attack_armed = 3;
    game_events_tick(&g, 20);
    int breaths = 0;
    for (int id = 22; id <= 25; ++id)
        breaths += sound(id, A_INTRUDER);
    CHECK(breaths == 1 && g.intruder_ms == 5000, 35);
    g.sound_count = 0;
    g.monitor = 0;
    game_events_tick(&g, 20);
    CHECK(sound(-1, A_INTRUDER) && !g.intruder_ms, 36);
    return 0;
}
