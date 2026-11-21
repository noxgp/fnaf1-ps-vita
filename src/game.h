#ifndef FNAF_GAME_H
#define FNAF_GAME_H
#include <stdint.h>
enum {
    G_TITLE,
    G_NEWSPAPER,
    G_INTRO,
    G_PLAY,
    G_OUTAGE,
    G_SCARE,
    G_DEAD,
    G_WIN,
    G_ENDING,
    G_OPTIONS,
    G_EXTRAS,
    G_CUSTOM,
    G_PAUSE,
    G_HELP,
    G_WARNING,
    G_CONFIRM_NEW,
    G_AUDIO_OPTIONS,
    G_CREEPY_START,
    G_CREEPY_END,
    G_CONTROL_OPTIONS
};
enum { TITLE_NEW, TITLE_CONTINUE, TITLE_NIGHT6, TITLE_CUSTOM, TITLE_OPTIONS, TITLE_UNLOCKS };
enum {
    IN_UP = 1,
    IN_DOWN = 2,
    IN_LEFT = 4,
    IN_RIGHT = 8,
    IN_OK = 16,
    IN_BACK = 32,
    IN_L = 64,
    IN_R = 128,
    IN_LIGHT = 256,
    IN_PHONE = 1024,
    IN_PAUSE = 2048,
    IN_HELP = 4096,
    IN_DIRECT_MONITOR = 8192,
    IN_DIRECT_DOOR = 16384,
    IN_DIRECT_LIGHT = 32768,
    IN_DIRECT_MUTE = 65536
};
enum { WARNING_DURATION_MS = 2800, NEWSPAPER_DURATION_MS = 4000, NIGHT_INTRO_DURATION_MS = 1800 };
enum { CONTROL_CONTEXTUAL, CONTROL_CLASSIC, CONTROL_CUSTOM, CONTROL_LAYOUT_COUNT };
enum { BUTTON_NONE, BUTTON_CROSS, BUTTON_CIRCLE, BUTTON_SQUARE, BUTTON_TRIANGLE,
       BUTTON_L, BUTTON_R, CONTROL_BUTTON_COUNT };
enum { CONTROL_MONITOR, CONTROL_DOOR, CONTROL_LIGHT, CONTROL_LEFT_DOOR,
       CONTROL_RIGHT_DOOR, CONTROL_LEFT_LIGHT, CONTROL_RIGHT_LIGHT, CONTROL_MUTE,
       CONTROL_ACTION_COUNT };
enum { CONTROLS_FIRST_ACTION = 1, CONTROLS_BACK = CONTROLS_FIRST_ACTION + CONTROL_ACTION_COUNT };
enum { OPTIONS_CONTROLS = 8 };
enum { OFFICE_PAN_MAX = 320, DOOR_FPS = 30, DOOR_DURATION_MS = 540 };
typedef struct {
    int display, perspective, vibration, subtitles, forced, language, music, volume;
    int unlimited, fast, radar, plush[4];
    int control_layout;
    unsigned char control_buttons[CONTROL_ACTION_COUNT];
} GameOptions;
typedef struct {
    GameOptions options;
    int next_night, unlocked, stars, custom[4];
} GameSave;
typedef struct {
    int id, channel, loop, volume;
} GameSound;
typedef struct {
    GameSave save;
    int phase, return_phase, options_parent, selection, night, hour, power, usage;
    int door[2], light[2], jam[2], monitor, camera, pan, ai[4], position[3], branch[2], fox,
        fox_hits;
    int phone_muted, killer, golden, picture, outage_stage, cheated, dirty, quit, diagnostics,
        extras_tab, pan_speed, pan_fraction, camera_flicker, camera_twitch;
    int halloween, hallucination_roll, startup_roll, dead_roll, last_freddy_position, murmur_volume,
        event_volumes[24], side_attack_armed, binding_capture;
    uint32_t rng, elapsed, phase_ms, hour_ms, drain_ms, passive_ms, move_ms[4], freddy_wait, fox_ms,
        inside_ms[2], light_ms, golden_ms, outage_roll, phone_ms, static_ms, door_ms[2], monitor_ms,
        fox_cooldown, watch_ms, freddy_inside_ms, kitchen_ms, camera_scan, visual_rng,
        flicker_clock, twitch_ms;
    uint32_t event_rng, hallucination_left, event_second_ms, event_four_ms, event_five_ms,
        event_ten_ms, murmur_ms, freddy_music_ms, ambience_cooldown_ms, dead_random_ms, camera_selected_ms;
    GameSound sounds[64];
    int sound_count;
} Game;
void game_defaults(GameSave *s);
void game_init(Game *g, const GameSave *s, uint32_t seed);
void game_start(Game *g, int night);
void game_input(Game *g, unsigned pressed, unsigned held, int analog);
void game_tick(Game *g, unsigned ms);
int game_office_side(const Game *g);
const char *game_control_action_name(int action);
int game_control_binding(const GameOptions *options, int action);
const char *game_control_button_name(const GameOptions *options, int action);
int game_light_visible(const Game *g, int side);
void game_sound(Game *g, int id, int channel, int loop, int volume);
int game_camera_view(const Game *g);
int game_camera_waypoint(const Game *g);
int game_camera_animation(const Game *g);
int game_camera_feeds(const Game *g, int animations[3]);
int game_camera_variants(const Game *g, int animations[3]);
int game_camera_pan(const Game *g);
void game_select_camera(Game *g, int camera);
int game_title_action(const Game *g, int index);
void game_encode_save(const GameSave *s, unsigned char bytes[128]);
int game_decode_save(GameSave *s, const unsigned char bytes[128]);
#endif
