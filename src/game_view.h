#ifndef GAME_VIEW_H
#define GAME_VIEW_H
#include "game.h"
int game_view_prepare(const Game *g, unsigned clock_ms, unsigned phone_ms, int preparing_night);
void game_view_draw(const Game *g, unsigned fps, int save_error);
int game_view_loading(const Game *g);
int game_touch(Game *g, int x, int y, int initial);
#endif
