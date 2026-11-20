#include "touch_input.h"
#include <string.h>
void touch_begin(TouchContacts *c) {
    memcpy(c->previous, c->current, sizeof(c->current));
    memset(c->current, 0, sizeof(c->current));
}
int touch_contact(TouchContacts *c, unsigned id) {
    if (id >= 256)
        return 0;
    uint32_t mask = 1u << (id % 32);
    int fresh = (c->previous[id / 32] & mask) == 0;
    c->current[id / 32] |= mask;
    return fresh;
}
int touch_to_game(TouchArea a, int raw_x, int raw_y, int display, int *x, int *y) {
    if (a.right <= a.left || a.bottom <= a.top || raw_x < a.left || raw_x > a.right ||
        raw_y < a.top || raw_y > a.bottom)
        return 0;
    *x = (raw_x - a.left) * 960 / (a.right - a.left + 1);
    *y = (raw_y - a.top) * 544 / (a.bottom - a.top + 1);
    if (display == 2) {
        if (*x < 117 || *x >= 843)
            return 0;
        *x = (*x - 117) * 960 / 726;
    } else if (display == 1)
        *y = *y * 540 / 544 + 2;
    return 1;
}
