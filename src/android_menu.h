#ifndef FNAF_ANDROID_MENU_H
#define FNAF_ANDROID_MENU_H
#include "game.h"
int android_menu_phase(int phase);
int android_menu_warning_ready(void);
void android_menu_night_card(int night);
int android_menu_night_card_ready(int night);
void android_menu_loading_clock(void);
void android_menu_draw_controls(const Game *g);
void android_menu_draw_extras_status(const Game *g);
int android_menu_prepare(const Game *game, unsigned clock_ms);
void android_menu_touch(Game *game, int x, int y);
#endif
