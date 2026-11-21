#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H
int game_audio_start(void);
void game_audio_stop(void);
void game_audio_play(int id, int channel, int loop, int volume);
void game_audio_settings(int volume, int paused, int music);
unsigned game_audio_position(int channel);
int game_audio_errors(void);
int game_audio_ready(void);
int game_audio_title_ready(void);
#endif
