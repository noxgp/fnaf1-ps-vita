#ifndef FNAF_GAME_EVENTS_H
#define FNAF_GAME_EVENTS_H
#include "game.h"
enum {
    AUDIO_VOICES = 24,
    A_DOOR_LIGHT = 5,
    A_PIRATE = 8,
    A_CREEK = 9,
    A_DRONE = 10,
    A_HALLUCINATION = 11,
    A_DANGER = 12,
    A_KITCHEN = 13,
    A_FREDDY_MUSIC = 14,
    A_INSIDE = 15,
    A_CAMERA = 16,
    A_INTRUDER = 17,
    A_GOLDEN = 18,
    A_MONITOR = 19,
    A_RUN = 20,
    A_SIGHTING = 22
};
unsigned game_event_random(Game *g, unsigned n);
void game_events_start(Game *g);
void game_events_tick(Game *g, unsigned ms);
void game_events_levels(Game *g);
void game_hallucinate(Game *g);
void game_set_date(Game *g, int month, int day);
void game_sound_volume(Game *g, int channel, int volume);
#endif
