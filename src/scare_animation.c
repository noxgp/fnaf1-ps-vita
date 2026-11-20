#include "scare_animation.h"
#include "generated_scares.h"
#include "generated_power_out.h"
static const ScareAnimation sequences[] = {{scare_145, 31, scare_145_speed, 0},
                                           {scare_143, 11, scare_143_speed, 1},
                                           {scare_144, 16, scare_144_speed, 1},
                                           {scare_146, 18, scare_146_speed, 0}};
static const ScareAnimation power_out = {power_out_frames, POWER_OUT_COUNT, POWER_OUT_SPEED, 0};
const ScareAnimation *scare_animation(int killer) {
    return killer == SCARE_POWER_OUT   ? &power_out
           : killer >= 0 && killer < 4 ? &sequences[killer]
                                       : 0;
}
unsigned scare_duration(int killer) {
    return killer == SCARE_POWER_OUT
               ? (POWER_OUT_COUNT * 100000u + POWER_OUT_SPEED * 60 - 1) / (POWER_OUT_SPEED * 60)
               : 1700;
}
int scare_frame(int killer, unsigned ms) {
    const ScareAnimation *a = scare_animation(killer);
    if (!a)
        return 0;
    /* Android CRunKyso.Animation adds speed * normalized 60 Hz delta to
     * AnimCounter and advances once per 100 units. Speed is not FPS. */
    unsigned frame = ms * (unsigned)a->speed * 60 / 100000;
    return a->loop                       ? (int)(frame % (unsigned)a->count)
           : frame >= (unsigned)a->count ? a->count - 1
                                         : (int)frame;
}
int scare_image(int handle) {
    for (int k = 0; k < 4; ++k)
        for (int f = 0; f < sequences[k].count; ++f)
            if (sequences[k].frames[f] == handle)
                return 1;
    for (int f = 0; f < POWER_OUT_COUNT; ++f)
        if (power_out.frames[f] == handle)
            return 1;
    return 0;
}
void scare_texture_size(int handle, unsigned *width, unsigned *height) {
    /* Existing files are at 75% source scale. A 50% source texture lets a
     * complete sequence remain resident within the 64 MiB GPU cache. Geometry
     * stays unchanged and the GPU filters the image to the display size. */
    if (scare_image(handle)) {
        *width = (*width * 2 + 1) / 3;
        *height = (*height * 2 + 1) / 3;
    }
}
