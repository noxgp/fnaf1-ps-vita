#ifndef FNAF_MONITOR_ANIMATION_H
#define FNAF_MONITOR_ANIMATION_H
#include "animation_tween.h"
/* Original objects 69/73: eleven reversed frames, speed50 at 60Hz. */
enum { MONITOR_FRAMES = 11, MONITOR_SPEED = 50, MONITOR_DURATION = 367 };
typedef SpriteTween MonitorFrame;
static inline MonitorFrame monitor_frame(unsigned ms) {
    return sprite_tween(ms, MONITOR_FRAMES, MONITOR_SPEED * 60 / 100);
}
static inline unsigned monitor_reverse(unsigned ms) {
    return sprite_reverse(ms, MONITOR_FRAMES, MONITOR_SPEED * 60 / 100);
}
#endif
