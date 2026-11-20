#include "audio_mix.h"

void mix_menu_audio(int16_t *stereo, unsigned frames, const int16_t *click,
                    unsigned click_frames, unsigned *cursor, int muted) {
    for (unsigned i = 0; i < frames; ++i) {
        int sound = click && *cursor < click_frames ? click[(*cursor)++] : 0;
        for (unsigned channel = 0; channel < 2; ++channel) {
            int value = stereo[i * 2 + channel] / 2 + sound / 2;
            if (value > 32767) value = 32767;
            if (value < -32768) value = -32768;
            stereo[i * 2 + channel] = muted ? 0 : (int16_t)value;
        }
    }
}
