#ifndef FNAF_ANDROID_MENU_H
#define FNAF_ANDROID_MENU_H
#include "game.h"
int android_menu_phase(int phase);
void android_menu_prepare(const Game *game,unsigned clock_ms);
void android_menu_touch(Game *game,int x,int y);
#endif
