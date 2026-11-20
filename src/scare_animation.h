#ifndef FNAF_SCARE_ANIMATION_H
#define FNAF_SCARE_ANIMATION_H
typedef struct {
    const int *frames;
    int count, speed, loop;
} ScareAnimation;
enum { SCARE_POWER_OUT = 5 };
const ScareAnimation *scare_animation(int killer);
int scare_frame(int killer, unsigned ms);
unsigned scare_duration(int killer);
int scare_image(int handle);
void scare_texture_size(int handle, unsigned *width, unsigned *height);
#endif
