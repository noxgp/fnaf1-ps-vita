#ifndef FNAF_AUDIO_MIX_H
#define FNAF_AUDIO_MIX_H
#include <stdint.h>
void mix_menu_audio(int16_t *stereo, unsigned frames, const int16_t *click,
                    unsigned click_frames, unsigned *cursor, int muted);
#endif
