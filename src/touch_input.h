#ifndef FNAF_TOUCH_INPUT_H
#define FNAF_TOUCH_INPUT_H
#include <stdint.h>
typedef struct {int left,top,right,bottom;} TouchArea;
typedef struct {uint32_t previous[8],current[8];} TouchContacts;
void touch_begin(TouchContacts *contacts);
int touch_contact(TouchContacts *contacts,unsigned id);
int touch_to_game(TouchArea area,int raw_x,int raw_y,int display,int *x,int *y);
#endif
