/* Native FNAF rules transcribed from this APK's room event records.
 * Portable state machine: no rendering, OS calls or wall-clock dependencies. */
#include "game.h"
#include <string.h>
#include "generated_game_rules.h"
#include "camera_map.h"
#include "game_events.h"
#include "scare_animation.h"
#include "monitor_animation.h"
static unsigned random_n(Game *g, unsigned n) {
    g->rng ^= g->rng << 13;
    g->rng ^= g->rng >> 17;
    g->rng ^= g->rng << 5;
    return n ? g->rng % n : 0;
}
static unsigned visual_random(Game *g, unsigned n) {
    g->visual_rng ^= g->visual_rng << 13;
    g->visual_rng ^= g->visual_rng >> 17;
    g->visual_rng ^= g->visual_rng << 5;
    return g->visual_rng % n;
}
static int clamp(int n, int lo, int hi) {
    return n < lo ? lo : n > hi ? hi : n;
}
void game_sound(Game *g, int id, int channel, int loop, int volume) {
    if (g->sound_count < 64)
        g->sounds[g->sound_count++] = (GameSound){id, channel, loop, volume};
}
static void stop_all(Game *g) {
    for (int i = 0; i < AUDIO_VOICES; ++i)
        game_sound(g, -1, i, 0, 0);
}
static void phase(Game *g, int p) {
    g->phase = p;
    g->phase_ms = 0;
    g->selection = 0;
    g->binding_capture = 0;
}
static void title(Game *g) {
    int keep_music = g->phase == G_OPTIONS || g->phase == G_EXTRAS || g->phase == G_CUSTOM ||
                     g->phase == G_CONFIRM_NEW;
    phase(g, G_TITLE);
    g->selection = g->save.next_night > 1 ? 1 : 0;
    if (!keep_music) {
        stop_all(g);
        game_sound(g, 34, 1, 0, 70);
        game_sound(g, 35, 0, 1, 70);
    }
}
static void creepy(Game *g, int ending) {
    phase(g, ending ? G_CREEPY_END : G_CREEPY_START);
    stop_all(g);
    g->killer = 4;
    if (ending)
        game_sound(g, 46, 6, 0, 100);
}
static void warning_done(Game *g) {
    if (!g->startup_roll)
        creepy(g, 0);
    else
        title(g);
}
static const unsigned char control_presets[][CONTROL_ACTION_COUNT] = {
    {BUTTON_CROSS, BUTTON_R, BUTTON_SQUARE, BUTTON_NONE, BUTTON_NONE, BUTTON_NONE,
     BUTTON_NONE, BUTTON_TRIANGLE},
    {BUTTON_CROSS, BUTTON_NONE, BUTTON_NONE, BUTTON_L, BUTTON_R, BUTTON_SQUARE,
     BUTTON_CIRCLE, BUTTON_TRIANGLE},
};
static const unsigned control_inputs[CONTROL_BUTTON_COUNT] = {
    0, IN_OK, IN_BACK, IN_LIGHT, IN_PHONE, IN_L, IN_R,
};
int game_control_binding(const GameOptions *o, int action) {
    return o->control_layout == CONTROL_CUSTOM ? o->control_buttons[action]
                                               : control_presets[o->control_layout][action];
}
const char *game_control_action_name(int action) {
    static const char *names[] = {"Monitor", "Door on viewed side", "Light on viewed side",
                                  "Left door", "Right door", "Left light", "Right light",
                                  "Mute phone"};
    return names[action];
}
const char *game_control_button_name(const GameOptions *o, int action) {
    static const char *names[] = {"Unassigned", "Cross", "Circle", "Square", "Triangle", "L", "R"};
    if (o->control_layout == CONTROL_CONTEXTUAL && action == CONTROL_MONITOR)
        return "L / Cross";
    return names[game_control_binding(o, action)];
}
static unsigned control_input(const Game *g, int action) {
    return control_inputs[game_control_binding(&g->save.options, action)];
}
static void bind_control(Game *g, int action, int button) {
    unsigned char *buttons = g->save.options.control_buttons;
    for (int i = 0; button != BUTTON_NONE && i < CONTROL_ACTION_COUNT; ++i)
        if (i != action && buttons[i] == button) {
            buttons[i] = buttons[action];
            break;
        }
    buttons[action] = (unsigned char)button;
    g->dirty = 1;
}
static void controls_input(Game *g, unsigned pressed) {
    GameOptions *o = &g->save.options;
    if (g->binding_capture) {
        if (pressed & (IN_PAUSE | IN_HELP)) {
            g->binding_capture = 0;
            return;
        }
        unsigned physical = pressed & (IN_OK | IN_BACK | IN_LIGHT | IN_PHONE | IN_L | IN_R);
        if (physical && !(physical & (physical - 1)))
            for (int i = BUTTON_CROSS; i < CONTROL_BUTTON_COUNT; ++i)
                if (physical == control_inputs[i]) {
                    bind_control(g, g->selection - CONTROLS_FIRST_ACTION, i);
                    g->binding_capture = 0;
                    break;
                }
        return;
    }
    if ((pressed & IN_BACK) || (g->selection == CONTROLS_BACK && (pressed & IN_OK))) {
        phase(g, G_OPTIONS);
        g->selection = OPTIONS_CONTROLS;
        return;
    }
    if (pressed & (IN_UP | IN_DOWN)) {
        int size = CONTROLS_BACK + 1;
        int dir = pressed & IN_UP ? size - 1 : 1;
        g->selection = (g->selection + dir) % size;
    }
    int direction = pressed & IN_LEFT ? -1 : pressed & IN_RIGHT ? 1 : 0;
    if (g->selection == 0) {
        if (direction || (pressed & IN_OK)) {
            o->control_layout = (o->control_layout + (direction ? direction : 1) +
                                 CONTROL_LAYOUT_COUNT) % CONTROL_LAYOUT_COUNT;
            g->dirty = 1;
        }
    } else if (g->selection < CONTROLS_BACK && o->control_layout == CONTROL_CUSTOM) {
        int action = g->selection - CONTROLS_FIRST_ACTION;
        if (direction)
            bind_control(g, action, (o->control_buttons[action] + direction + CONTROL_BUTTON_COUNT) %
                                    CONTROL_BUTTON_COUNT);
        else if (pressed & IN_OK)
            g->binding_capture = 1;
    }
}
void game_defaults(GameSave *s) {
    memset(s, 0, sizeof(*s));
    s->options.perspective = 1;
    memcpy(s->options.control_buttons, control_presets[CONTROL_CONTEXTUAL], CONTROL_ACTION_COUNT);
    s->options.vibration = 1;
    s->options.subtitles = 1;
    s->options.forced = 1;
    s->options.music = 1;
    s->options.volume = 80;
    s->next_night = 1;
    s->unlocked = 1;
    for (int i = 0; i < 4; ++i)
        s->custom[i] = 5;
}
void game_init(Game *g, const GameSave *s, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    g->save = *s;
    g->rng = seed ? seed : 1;
    g->event_rng = g->rng ^ 0x85ebca6bu;
    g->startup_roll = (int)game_event_random(g, 1000);
    phase(g, G_WARNING);
}
int game_title_action(const Game *g, int index) {
    if (index < 2)
        return index;
    if (g->save.unlocked >= 6) {
        if (index == 2)
            return TITLE_NIGHT6;
        --index;
    }
    if (g->save.unlocked >= 7) {
        if (index == 2)
            return TITLE_CUSTOM;
        --index;
    }
    return index == 2 ? TITLE_OPTIONS : TITLE_UNLOCKS;
}
int game_camera_view(const Game *g) {
    return camera_map[clamp(g->camera, 0, 10)].view;
}
int game_camera_waypoint(const Game *g) {
    return camera_map[clamp(g->camera, 0, 10)].object;
}
int game_camera_animation(const Game *g) {
    return original_camera_animation(g);
}
int game_camera_feeds(const Game *g, int animations[3]) {
    int room = game_camera_waypoint(g);
    int count = 1, occupants = 0;
    animations[0] = game_camera_animation(g);
    for (int actor = 0; actor < 3; ++actor)
        occupants += g->position[actor] == room;

    int view = game_camera_view(g);
    /* Keep the stage group artwork and the authored Chica-priority feeds. */
    if (occupants < 2 || view == 1 || view == 2 || view == 6 || view == 42)
        return count;

    Game angle = *g;
    for (int actor = 0; actor < 3 && count < 3; ++actor) {
        if (g->position[actor] != room)
            continue;
        for (int other = 0; other < 3; ++other)
            angle.position[other] =
                other != actor && g->position[other] == room ? -1 : g->position[other];
        int animation = game_camera_animation(&angle);
        int duplicate = 0;
        for (int i = 0; i < count; ++i)
            duplicate |= animations[i] == animation;
        if (!duplicate)
            animations[count++] = animation;
    }
    return count;
}
int game_camera_variants(const Game *g, int animations[3]) {
    int view = game_camera_view(g);
    if (view == 3 && g->fox < 3) {
        animations[0] = 14;
        animations[1] = g->position[1] == 77 ? 33 : 15;
        return 2;
    }
    if (view == 22 && g->night >= 4 && g->position[1] == 78) {
        animations[0] = 30;
        animations[1] = 58;
        animations[2] = 59;
        return 3;
    }
    if (view == 42 && g->night >= 4 && g->position[2] == 81) {
        animations[0] = 38;
        animations[1] = 56;
        animations[2] = 57;
        return 3;
    }
    animations[0] = game_camera_animation(g);
    return 1;
}
void game_select_camera(Game *g, int camera) {
    if (camera < 0 || camera >= 11 || camera == g->camera)
        return;
    g->camera = camera;
    g->camera_selected_ms = g->elapsed;
    g->camera_cut_ms = CAMERA_CUT_MS;
    g->static_ms = 0;
    game_sound(g, 11, 7, 0, 60);
}
int game_camera_pan(const Game *g) {
    /* APK room events 13-22, 284, 316: 60 source pixels/sec, 320 travel,
     * 100 reference frames at each end. Supply closet stays at the left edge. */
    if (game_camera_view(g) == 33 || game_camera_view(g) == 6)
        return 0;
    unsigned t = g->camera_scan % 840000;
    return t < 320000   ? (int)(t / 1000)
           : t < 420000 ? 320
           : t < 740000 ? (int)((740000 - t) / 1000)
                        : 0;
}
void game_start(Game *g, int night) {
    GameSave save = g->save;
    uint32_t seed = g->rng;
    uint32_t event_seed = g->event_rng;
    int halloween = g->halloween;
    int dirty = g->dirty, diagnostics = g->diagnostics;
    memset(g, 0, sizeof(*g));
    g->save = save;
    g->rng = seed;
    g->dirty = dirty;
    g->diagnostics = diagnostics;
    g->halloween = halloween;
    g->event_rng = event_seed;
    g->night = clamp(night, 1, 7);
    g->power = 999;
    g->pan = 160;
    g->killer = -1;
    for (int i = 0; i < 4; ++i)
        g->ai[i] = night == 7 ? save.custom[i] : original_starting_ai[g->night - 1][i];
    if (g->night <= 6) {
        unsigned bound = original_freddy_random_bound[g->night - 1];
        if (bound)
            g->ai[0] += (int)random_n(g, bound);
    }
    for (int i = 0; i < 3; ++i)
        g->position[i] = 75;
    g->picture = 1 + (int)random_n(g, 100);
    g->visual_rng = g->rng ^ 0x9e3779b9u;
    if (!g->visual_rng)
        g->visual_rng = 1;
    g->camera_flicker = 1;
    g->camera_twitch = 1;
    g->cheated = save.options.unlimited || save.options.fast || save.options.radar;
    phase(g, G_INTRO);
    stop_all(g);
    game_sound(g, 11, 7, 0, 55);
    if (night == 7 && save.custom[0] == 1 && save.custom[1] == 9 && save.custom[2] == 8 &&
        save.custom[3] == 7) {
        creepy(g, 1);
    }
}
static void play(Game *g) {
    phase(g, G_PLAY);
    game_sound(g, 0, 0, 1, 24);
    game_sound(g, 2, 1, 1, 20);
    if (g->night <= 5)
        game_sound(g, 40 + g->night, 2, 0, 100);
    game_events_start(g);
}
static void scare(Game *g, int killer) {
    if (g->phase != G_PLAY && g->phase != G_OUTAGE)
        return;
    g->killer = killer;
    phase(g, G_SCARE);
    stop_all(g);
    game_sound(g, killer == 4 ? 46 : 15, 6, 0, 100);
}
static void win(Game *g) {
    phase(g, G_WIN);
    stop_all(g);
    game_sound(g, 32, 0, 0, 100);
    if (!g->cheated) {
        int n = g->night;
        if (n < 6) {
            g->save.next_night = n < 5 ? n + 1 : 5;
            if (g->save.unlocked < n + 1)
                g->save.unlocked = n + 1;
        }
        if (n == 5)
            g->save.stars |= 1;
        if (n == 6) {
            g->save.stars |= 2;
            g->save.unlocked = 7;
        }
        if (n == 7 && g->save.custom[0] == 20 && g->save.custom[1] == 20 &&
            g->save.custom[2] == 20 && g->save.custom[3] == 20)
            g->save.stars |= 4;
        g->dirty = 1;
    }
}
static int menu_count(const Game *g) {
    if (g->phase == G_TITLE)
        return 4 + (g->save.unlocked >= 6) + (g->save.unlocked >= 7);
    if (g->phase == G_OPTIONS)
        return 9;
    if (g->phase == G_EXTRAS)
        return 8;
    if (g->phase == G_AUDIO_OPTIONS)
        return 4;
    if (g->phase == G_CONFIRM_NEW)
        return 2;
    if (g->phase == G_CUSTOM)
        return 6;
    if (g->phase == G_PAUSE)
        return g->pause_action ? 2 : PAUSE_COUNT;
    return 1;
}
static void change_option(Game *g, int direction) {
    GameOptions *o = &g->save.options;
    int i = g->selection;
    if (g->phase == G_OPTIONS) {
        if (i == OPTIONS_CONTROLS) {
            phase(g, G_CONTROL_OPTIONS);
            return;
        }
        int *fields[] = {&o->display, &o->perspective, &o->vibration, &o->subtitles,
                         &o->forced,  &o->language,    &o->music,     &o->volume};
        if (i == 7) {
            if (g->options_parent == G_TITLE)
                title(g);
            else
                phase(g, g->options_parent);
            return;
        }
        if (i == 6) {
            phase(g, G_AUDIO_OPTIONS);
            return;
        }
        int max = i == 0 ? 3 : i == 5 ? 11 : i == 7 ? 11 : 2;
        int step = i == 7 ? 10 : 1;
        *fields[i] = ((*fields[i] / step + direction + max) % max) * step;
        if (i == 6 && g->options_parent == G_TITLE)
            game_sound(g, o->music ? 35 : -1, 0, 1, 70);
        g->dirty = 1;
    } else if (g->phase == G_EXTRAS) {
        if (i == 7) {
            title(g);
            return;
        }
        int *fields[] = {&o->unlimited, &o->fast,     &o->radar,   &o->plush[0],
                         &o->plush[1],  &o->plush[2], &o->plush[3]};
        *fields[i] = !*fields[i];
        g->dirty = 1;
    } else if (g->phase == G_AUDIO_OPTIONS) {
        if (i == 0) {
            o->music = !o->music;
            g->dirty = 1;
        } else if (i == 1) {
            o->volume = (o->volume + direction * 10 + 110) % 110;
            g->dirty = 1;
        } else if (i == 2)
            g->diagnostics = !g->diagnostics;
        else
            phase(g, G_OPTIONS);
    } else if (g->phase == G_CUSTOM && i < 4) {
        g->save.custom[i] = clamp(g->save.custom[i] + direction, 0, 20);
        g->dirty = 1;
    }
}
int game_office_side(const Game *g) {
    if (g->pan <= 56)
        return 0;
    if (g->pan >= OFFICE_PAN_MAX - 56)
        return 1;
    return -1;
}
int game_light_visible(const Game *g, int side) {
    return g->light[side] && !g->light_flicker;
}
int game_phone_can_mute(const Game *g) {
    return g->night >= 1 && g->night <= 5 && !g->phone_muted && !g->phone_finished;
}
static void set_light(Game *g, int side) {
    if ((side < 0 && !g->light[0] && !g->light[1]) || (side >= 0 && g->light[side]))
        return;
    g->light[0] = side == 0;
    g->light[1] = side == 1;
    g->light_flicker = 0;
    game_sound(g, side >= 0 ? 1 : -1, A_DOOR_LIGHT, side >= 0, side >= 0 ? 100 : 0);
    g->event_volumes[A_DOOR_LIGHT] = side >= 0 ? 101 : 1;
}
static void check_door_sighting(Game *g, int side) {
    if (side >= 0 && game_light_visible(g, side) && !g->door[side] &&
        !g->door_ms[side] && !g->door_seen[side] && g->position[side + 1] == 99 + side) {
        g->door_seen[side] = 1;
        game_sound(g, 31, A_SIGHTING, 0, 80);
    }
}
static void start_pause(Game *g) {
    g->return_phase = g->phase;
    g->pause_action = 0;
    phase(g, G_PAUSE);
}
static int office_pan_speed(unsigned held, int analog) {
    int direction = ((held & IN_RIGHT) != 0) - ((held & IN_LEFT) != 0);
    return (direction ? direction * 127 : clamp(analog, -127, 127)) * 6;
}
static void tick_office_pan(Game *g, unsigned ms) {
    g->pan_fraction += g->pan_speed * (int)ms;
    g->pan = clamp(g->pan + g->pan_fraction / 1000, 0, OFFICE_PAN_MAX);
    g->pan_fraction %= 1000;
}
void game_input(Game *g, unsigned pressed, unsigned held, int analog) {
    if (g->phase == G_CONTROL_OPTIONS) {
        controls_input(g, pressed);
        return;
    }
    if ((pressed & IN_PAUSE) &&
        (g->phase == G_TITLE || g->phase == G_WARNING || g->phase == G_CONFIRM_NEW))
        pressed |= IN_OK;
    if (g->phase == G_WARNING) {
        if (pressed & (IN_OK | IN_BACK))
            warning_done(g);
        return;
    }
    if (g->phase == G_CREEPY_START || g->phase == G_CREEPY_END)
        return;
    if (g->phase == G_CONFIRM_NEW) {
        if (pressed & IN_BACK) {
            title(g);
            return;
        }
        if (pressed & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT))
            g->selection ^= 1;
        if (pressed & IN_OK) {
            if (g->selection) {
                g->save.next_night = 1;
                g->dirty = 1;
                phase(g, G_NEWSPAPER);
                stop_all(g);
            } else
                title(g);
        }
        return;
    }
    if (g->phase == G_PLAY) {
        if (!g->monitor && g->side_attack_armed)
            return;
        if (pressed & (IN_PAUSE | IN_HELP))
            set_light(g, -1);
        if (pressed & IN_PAUSE) {
            start_pause(g);
            return;
        }
        if (pressed & IN_HELP) {
            g->return_phase = G_PLAY;
            phase(g, G_HELP);
            return;
        }
        unsigned monitor_keys = IN_DIRECT_MONITOR | control_input(g, CONTROL_MONITOR);
        if (g->save.options.control_layout == CONTROL_CONTEXTUAL)
            monitor_keys |= IN_L;
        int flip_monitor = !g->monitor_ms && (pressed & monitor_keys);
        if (flip_monitor) {
            g->monitor ^= 1;
            g->monitor_ms = 1;
            g->camera_cut_ms = 0;
            g->static_ms = 0;
            set_light(g, -1);
            game_sound(g, g->monitor ? 6 : 7, 3, 0, 65);
            game_sound(g, g->monitor ? 5 : -1, A_MONITOR, 0, 100);
            if (!g->monitor)
                g->picture = 1 + (int)visual_random(g, 100);
            if (g->monitor) {
                for (int i = 0; i < 2; ++i)
                    if (g->jam[i])
                        g->side_attack_armed |= 1 << i;
                if (g->golden_ms) {
                    g->golden = 0;
                    g->golden_ms = 0;
                }
            }
        }
        if (g->monitor && !g->monitor_ms) {
            int dx = ((pressed & IN_RIGHT) != 0) - ((pressed & IN_LEFT) != 0),
                dy = ((pressed & IN_DOWN) != 0) - ((pressed & IN_UP) != 0);
            game_select_camera(g, camera_map_move(g->camera, dx, dy));
        }
        g->pan_speed = (g->monitor || g->monitor_ms) ? 0 : office_pan_speed(held, analog);
        int side = !g->monitor && !g->monitor_ms ? game_office_side(g) : -1;
        unsigned door_keys = IN_DIRECT_DOOR | control_input(g, CONTROL_DOOR) |
                             control_input(g, side == 0 ? CONTROL_LEFT_DOOR : CONTROL_RIGHT_DOOR);
        unsigned light_keys = IN_DIRECT_LIGHT | control_input(g, CONTROL_LIGHT) |
                              control_input(g, side == 0 ? CONTROL_LEFT_LIGHT : CONTROL_RIGHT_LIGHT);
        if (side < 0 || g->jam[side] || g->light[1 - side])
            set_light(g, -1);
        if (side >= 0) {
            if (g->jam[side]) {
                if (pressed & (door_keys | light_keys))
                    game_sound(g, 4, 3, 0, 70);
            } else {
                if (!g->door_ms[side] && (pressed & door_keys)) {
                    g->door[side] ^= 1;
                    g->door_ms[side] = 1;
                    game_sound(g, 3, 3, 0, 85);
                }
                if (pressed & light_keys)
                    set_light(g, g->light[side] ? -1 : side);
            }
        }
        check_door_sighting(g, side);
        if (game_phone_can_mute(g) && (pressed & (IN_DIRECT_MUTE | control_input(g, CONTROL_MUTE)))) {
            g->phone_muted = 1;
            game_sound(g, -1, 2, 0, 0);
        }
        return;
    }
    if (g->phase == G_OUTAGE) {
        g->pan_speed = office_pan_speed(held, analog);
        if (pressed & IN_PAUSE)
            start_pause(g);
        return;
    }
    if (g->phase == G_INTRO || g->phase == G_NEWSPAPER) {
        if (pressed & IN_OK) {
            if (g->phase == G_NEWSPAPER)
                game_start(g, 1);
            else
                play(g);
        }
        return;
    }
    if (g->phase == G_SCARE || g->phase == G_WIN)
        return;
    if (g->phase == G_DEAD || g->phase == G_ENDING) {
        if (pressed & IN_OK)
            title(g);
        return;
    }
    if (g->phase == G_HELP) {
        if (pressed & (IN_OK | IN_BACK | IN_HELP))
            phase(g, g->return_phase);
        return;
    }
    if (pressed & IN_BACK) {
        if (g->phase == G_PAUSE) {
            if (g->pause_action) {
                g->pause_action = 0;
                g->selection = PAUSE_RESUME;
            } else
                phase(g, g->return_phase);
        } else if (g->phase == G_OPTIONS) {
            if (g->options_parent == G_TITLE)
                title(g);
            else
                phase(g, g->options_parent);
        } else if (g->phase == G_AUDIO_OPTIONS) {
            phase(g, G_OPTIONS);
        } else if (g->phase != G_TITLE)
            title(g);
        return;
    }
    if ((pressed & IN_HELP) && g->phase == G_TITLE)
        g->diagnostics = !g->diagnostics;
    if (pressed & (IN_UP | IN_DOWN)) {
        int count = menu_count(g);
        int dir = pressed & IN_UP ? count - 1 : 1;
        if (g->phase == G_OPTIONS) {
            const int order[] = {0, 1, 2, 5, 3, 4, 6, OPTIONS_CONTROLS, 7};
            int position = 0;
            for (int i = 0; i < count; ++i)
                if (order[i] == g->selection)
                    position = i;
            g->selection = order[(position + dir) % count];
        } else
            g->selection = (g->selection + dir) % count;
        game_sound(g, 11, 7, 0, 55);
    }
    if (pressed & (IN_LEFT | IN_RIGHT))
        change_option(g, pressed & IN_LEFT ? -1 : 1);
    if (!(pressed & IN_OK))
        return;
    game_sound(g, 11, 7, 0, 55);
    if (g->phase == G_TITLE) {
        int i = game_title_action(g, g->selection);
        if (i == TITLE_NEW)
            phase(g, G_CONFIRM_NEW);
        else if (i == TITLE_CONTINUE)
            game_start(g, g->save.next_night);
        else if (i == TITLE_OPTIONS) {
            g->options_parent = G_TITLE;
            phase(g, G_OPTIONS);
        } else if (i == TITLE_UNLOCKS)
            phase(g, G_EXTRAS);
        else if (i == TITLE_NIGHT6)
            game_start(g, 6);
        else
            phase(g, G_CUSTOM);
    } else if (g->phase == G_OPTIONS || g->phase == G_EXTRAS || g->phase == G_AUDIO_OPTIONS)
        change_option(g, 1);
    else if (g->phase == G_CUSTOM) {
        if (g->selection == 4)
            game_start(g, 7);
        else if (g->selection == 5)
            title(g);
        else
            change_option(g, 1);
    } else if (g->phase == G_PAUSE) {
        int i = g->selection;
        if (g->pause_action) {
            int action = g->pause_action;
            g->pause_action = 0;
            g->selection = PAUSE_RESUME;
            if (i && action == PAUSE_TITLE)
                title(g);
            else if (i && action == PAUSE_EXIT)
                g->quit = 1;
        } else if (i == PAUSE_RESUME)
            phase(g, g->return_phase);
        else if (i == PAUSE_OPTIONS) {
            g->options_parent = G_PAUSE;
            phase(g, G_OPTIONS);
        } else {
            g->pause_action = i;
            g->selection = 0;
        }
    }
}
static void camera_actor_moved(Game *g, int from, int to, int sound) {
    int room = game_camera_waypoint(g);
    if (!g->monitor || (room != from && room != to))
        return;
    g->static_ms = CAMERA_MOVE_STATIC_MS;
    g->camera_cut_ms = CAMERA_CUT_MS;
    if (sound) {
        const int glitches[] = {8, 12, 13, 14};
        game_sound(g, glitches[game_event_random(g, 4)], A_CAMERA, 0, 100);
    }
}
static unsigned route_room(int position) {
    return position >= 75 && position <= 84 ? 1u << (position - 75) : 0;
}
static void move_side(Game *g, int actor) {
    int index = actor - 1, b = 1 + (int)random_n(g, 2), pos = g->position[actor];
    int next = original_move(actor, pos, b, g->door[index]);
    if (pos == 99 + index) {
        g->side_visited[index] = 0;
    } else {
        /* Prefer a new room when the chosen branch would repeat this approach. */
        g->side_visited[index] |= route_room(pos);
        if (g->side_visited[index] & route_room(next)) {
            int alternate = original_move(actor, pos, 3 - b, g->door[index]);
            if (alternate != pos && !(g->side_visited[index] & route_room(alternate))) {
                b = 3 - b;
                next = alternate;
            }
        }
    }
    g->branch[index] = b;
    if (next != pos) {
        camera_actor_moved(g, pos, next, 1);
        g->position[actor] = next;
        g->door_seen[index] = 0;
        if (next == 101 || next == 102) {
            g->jam[index] = 1;
            if (g->light[index])
                set_light(g, -1);
            g->inside_ms[index] = 0;
            if (g->monitor)
                g->side_attack_armed |= 1 << index;
        } else if (next == 99 || next == 100) {
            if (!g->monitor)
                game_sound(g, 9, 4, 0, 40);
        } else if (actor == 1)
            game_sound(g, 9, 4, 0, 22);
    }
}
void game_tick(Game *g, unsigned ms) {
    /* Call in fixed 20 ms steps. Paused/menu time never advances a night. */
    if (g->phase == G_PAUSE || g->phase == G_HELP)
        return;
    if (g->phase == G_WARNING) {
        g->phase_ms += ms;
        if (g->phase_ms >= WARNING_DURATION_MS)
            warning_done(g);
        return;
    }
    if (g->phase == G_OPTIONS || g->phase == G_EXTRAS || g->phase == G_CUSTOM ||
        g->phase == G_TITLE || g->phase == G_CONFIRM_NEW || g->phase == G_AUDIO_OPTIONS ||
        g->phase == G_CONTROL_OPTIONS) {
        g->phase_ms += ms;
        return;
    }
    g->phase_ms += ms;
    if (g->phase == G_CREEPY_START || g->phase == G_CREEPY_END) {
        if (g->phase_ms >= (g->phase == G_CREEPY_START ? 10000u : 1000u))
            title(g);
        return;
    }
    if (g->phase == G_NEWSPAPER) {
        if (g->phase_ms >= NEWSPAPER_DURATION_MS)
            game_start(g, 1);
        return;
    }
    if (g->phase == G_INTRO) {
        if (g->phase_ms >= NIGHT_INTRO_DURATION_MS)
            play(g);
        return;
    }
    if (g->phase == G_SCARE) {
        if (g->phase_ms >= scare_duration(g->killer)) {
            phase(g, G_DEAD);
            stop_all(g);
            game_sound(g, 20, 0, 0, 60);
        }
        return;
    }
    if (g->phase == G_DEAD) {
        g->dead_random_ms += ms;
        while (g->dead_random_ms >= 1000) {
            g->dead_random_ms -= 1000;
            g->dead_roll = 1 + (int)game_event_random(g, 10000);
        }
        if (g->phase_ms >= 10000) {
            if (g->dead_roll == 1)
                creepy(g, 1);
            else
                title(g);
        }
        return;
    }
    if (g->phase == G_WIN) {
        if (g->phase_ms >= 3000 && g->phase_ms - ms < 3000)
            game_sound(g, 33, 3, 0, 80);
        if (g->phase_ms >= 9000) {
            if (g->night >= 5) {
                phase(g, G_ENDING);
                game_sound(g, 30, 3, 1, 100);
            } else
                game_start(g, g->night + 1);
        }
        return;
    }
    if (g->phase == G_ENDING)
        return;
    g->elapsed += ms;
    g->hour_ms += ms;
    g->camera_scan = (g->camera_scan + ms * 60) % 840000;
    unsigned second = g->save.options.fast ? 500 : 1000;
    if (g->hour_ms >= second * (g->hour ? 89u : 90u)) {
        g->hour_ms -= second * (g->hour ? 89u : 90u);
        ++g->hour;
        if (g->hour >= 6) {
            win(g);
            return;
        }
        if (g->hour == 2)
            ++g->ai[1];
        if (g->hour == 3 || g->hour == 4)
            for (int i = 1; i < 4; ++i)
                ++g->ai[i];
    }
    if (g->phase == G_OUTAGE) {
        if (g->outage_stage < 2)
            tick_office_pan(g, ms);
        /* Room events 406/407, 423/424 and 433/434. Each stage also has a deadline. */
        unsigned interval = g->outage_stage < 2 ? 5000u : 2000u;
        g->outage_roll += ms;
        if (g->phase_ms < 20000 && g->outage_roll < interval)
            return;
        g->outage_roll = 0;
        if (g->phase_ms < 20000 && random_n(g, 5) != 0)
            return;

        g->phase_ms = 0;
        if (g->outage_stage == 0) {
            g->outage_stage = 1;
            game_sound(g, 30, 0, 1, 85);
        } else if (g->outage_stage == 1) {
            g->outage_stage = 2;
            stop_all(g);
        } else {
            scare(g, SCARE_POWER_OUT);
        }
        return;
    }
    /* APK room events 246/549: independent visual randoms, not the stable
     * picture roll (event 481). Keep wall-clock rates independent of FPS. */
    g->flicker_clock += ms * 60;
    while (g->flicker_clock >= 1000) {
        g->flicker_clock -= 1000;
        g->camera_flicker = 1 + (int)visual_random(g, 10);
        g->hallucination_roll = (int)visual_random(g, 10);
    }
    g->twitch_ms += ms;
    while (g->twitch_ms >= 50) {
        g->twitch_ms -= 50;
        g->camera_twitch = 1 + (int)visual_random(g, 30);
    }
    if (g->static_ms > ms)
        g->static_ms -= ms;
    else
        g->static_ms = 0;
    g->camera_cut_ms = g->camera_cut_ms > ms ? g->camera_cut_ms - ms : 0;
    if (!g->monitor && !g->monitor_ms)
        tick_office_pan(g, ms);
    for (int i = 0; i < 2; ++i)
        if (g->door_ms[i]) {
            g->door_ms[i] += ms;
            if (g->door_ms[i] >= DOOR_DURATION_MS)
                g->door_ms[i] = 0;
        }
    if (g->monitor_ms) {
        g->monitor_ms += ms;
        if (g->monitor_ms >= MONITOR_DURATION) {
            g->monitor_ms = 0;
            if (g->monitor) {
                g->monitor_opened_ms = g->elapsed;
                g->camera_cut_ms = CAMERA_CUT_MS;
            }
        }
    }
    if (!g->monitor && !g->monitor_ms && g->side_attack_armed) {
        scare(g, (g->side_attack_armed & 1) ? 1 : 2);
        return;
    }
    if (g->light[0] || g->light[1]) {
        /* Room events 243-246 and 251-253 use the same 1-in-10 visual roll
         * for the unlit image and the muted hum. Keep the hum looping. */
        g->light_flicker = g->camera_flicker == 1;
        game_sound_volume(g, A_DOOR_LIGHT, g->light_flicker ? 0 : 100);
        check_door_sighting(g, game_office_side(g));
    }
    g->usage = 1 + g->door[0] + g->door[1] + g->light[0] + g->light[1] + g->monitor;
    g->drain_ms += ms;
    if (g->drain_ms >= 1000) {
        g->drain_ms -= 1000;
        g->power -= g->usage;
    }
    if (g->night >= 2) {
        unsigned interval = g->night == 2   ? 6000
                            : g->night == 3 ? 5000
                            : g->night == 4 ? 4000
                                            : 3000;
        g->passive_ms += ms;
        if (g->passive_ms >= interval) {
            g->passive_ms -= interval;
            --g->power;
        }
    }
    if (g->save.options.unlimited)
        g->power = 1000;
    if (g->power <= 0) {
        g->power = 0;
        g->door[0] = g->door[1] = g->light[0] = g->light[1] = g->monitor = 0;
        g->monitor_ms = g->door_ms[0] = g->door_ms[1] = 0;
        g->pan_speed = g->pan_fraction = 0;
        phase(g, G_OUTAGE);
        g->outage_stage = 0;
        g->outage_roll = 0;
        stop_all(g);
        game_sound(g, 26, 3, 0, 100);
        game_sound(g, 28, 1, 1, 50);
        return;
    }
    for (int i = 0; i < 2; ++i)
        if (g->monitor && (g->side_attack_armed & (1 << i))) {
            g->inside_ms[i] += ms;
            if (g->inside_ms[i] >= 30000)
                game_input(g, IN_DIRECT_MONITOR, 0, 0);
        }
    if (g->monitor) {
        g->watch_ms += ms;
        if (g->watch_ms >= 100) {
            g->watch_ms -= 100;
            g->fox_cooldown = (50 + random_n(g, 1000)) * 1000 / 60;
        }
    } else if (g->fox_cooldown > ms)
        g->fox_cooldown -= ms;
    else
        g->fox_cooldown = 0;
    static const unsigned interval[4] = {3020, 4970, 4980, 5010};
    for (int a = 0; a < 4; ++a) {
        g->move_ms[a] += ms;
        if (g->move_ms[a] >= interval[a]) {
            g->move_ms[a] -= interval[a];
            int success = (int)random_n(g, 20) + 1 <= g->ai[a];
            if (a == 1 || a == 2) {
                if (success && !g->jam[a - 1])
                    move_side(g, a);
            } else if (a == 0) {
                if (success && !g->monitor && !g->freddy_wait && g->position[0] != 122)
                    g->freddy_wait = 1;
            } else if (success && g->fox < 3 && !g->monitor && !g->fox_cooldown) {
                ++g->fox;
                if (g->fox == 3)
                    g->fox_ms = 0;
            }
        }
    }
    if (g->freddy_wait) {
        if (g->monitor && game_camera_waypoint(g) == g->position[0])
            g->freddy_wait = 1;
        else
            g->freddy_wait += ms;
        unsigned delay = (unsigned)clamp(1000 - g->ai[0] * 100, 0, 1000) * 1000 / 60;
        if (g->freddy_wait >= delay && !g->monitor && g->position[0] != 81) {
            int p = g->position[0];
            if (p != 75 || (g->position[1] != 75 && g->position[2] != 75)) {
                g->position[0] = p == 75 ? 76 : p == 76 ? 84 : p == 84 ? 83 : p == 83 ? 80 : 81;
                g->freddy_wait = 0;
                game_sound(g, 56 + (int)random_n(g, 3), 4, 0, 45);
            }
        }
    }
    if (g->position[0] == 81 && g->monitor && game_camera_view(g) != 42) {
        if (!g->door[1]) {
            g->position[0] = 122;
            camera_actor_moved(g, 81, 122, 0);
        } else if (game_camera_view(g) != 4) {
            g->position[0] = 80;
            camera_actor_moved(g, 81, 80, 0);
            g->freddy_wait = 0;
        }
    }
    if (g->position[0] == 122 && !g->monitor) {
        g->freddy_inside_ms += ms;
        if (g->freddy_inside_ms >= 1000) {
            g->freddy_inside_ms -= 1000;
            if (random_n(g, 4) == 0) {
                scare(g, 0);
                return;
            }
        }
    }
    if (g->fox >= 3) {
        g->fox_ms += ms;
        if (g->fox == 3 && g->monitor && !g->monitor_ms && game_camera_view(g) == 3) {
            g->fox = 4;
            g->fox_ms = 0;
            game_sound(g, 10, A_RUN, 0, 100);
            game_sound(g, 55, A_RUN + 1, 0, 80);
        }
        if ((g->fox == 3 && g->fox_ms >= 25000) || (g->fox == 4 && g->fox_ms >= 1667)) {
            if (g->door[0]) {
                g->power -= 10 + g->fox_hits * 50;
                ++g->fox_hits;
                g->fox = (int)random_n(g, 2);
                g->fox_ms = 0;
                game_sound(g, 27, 4, 0, 100);
            } else {
                scare(g, 3);
                return;
            }
        }
    }
    game_events_tick(g, ms);
    if (g->golden == 2 && !g->monitor) {
        g->golden_ms += ms;
        if (g->golden_ms >= 5000) {
            creepy(g, 1);
            return;
        }
    }
}
static uint32_t crc(const unsigned char *p, int n) {
    uint32_t c = 0xffffffffu;
    while (n--) {
        c ^= *p++;
        for (int j = 0; j < 8; ++j)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}
void game_encode_save(const GameSave *s, unsigned char out[128]) {
    memset(out, 0, 128);
    out[0] = 'F';
    out[1] = 'N';
    out[2] = 'S';
    out[3] = 2;
    const GameOptions *o = &s->options;
    int values[] = {o->display,    o->perspective, o->vibration, o->subtitles, o->forced,
                    o->language,   o->music,       o->volume,    o->unlimited, o->fast,
                    o->radar,      o->plush[0],    o->plush[1],  o->plush[2],  o->plush[3],
                    s->next_night, s->unlocked,    s->stars,     s->custom[0], s->custom[1],
                    s->custom[2],  s->custom[3]};
    for (int i = 0; i < 22; ++i)
        out[4 + i] = (unsigned char)values[i];
    /* Older v2 saves left these bytes zero and ignore them when loading. */
    if (o->control_layout != CONTROL_CONTEXTUAL ||
        memcmp(o->control_buttons, control_presets[CONTROL_CONTEXTUAL], CONTROL_ACTION_COUNT)) {
        out[26] = 1;
        out[27] = (unsigned char)o->control_layout;
        memcpy(out + 28, o->control_buttons, CONTROL_ACTION_COUNT);
    }
    uint32_t c = crc(out, 124);
    for (int i = 0; i < 4; ++i)
        out[124 + i] = (unsigned char)(c >> (i * 8));
}
int game_decode_save(GameSave *s, const unsigned char in[128]) {
    if (in[0] != 'F' || in[1] != 'N' || in[2] != 'S' || in[3] != 2)
        return -1;
    uint32_t expected = 0;
    for (int i = 0; i < 4; ++i)
        expected |= (uint32_t)in[124 + i] << (8 * i);
    if (crc(in, 124) != expected)
        return -1;
    const unsigned char *v = in + 4;
    const int max[] = {2, 1, 1, 1, 1, 10, 1, 100, 1, 1, 1, 1, 1, 1, 1, 6, 7, 7, 20, 20, 20, 20};
    for (int i = 0; i < 22; ++i)
        if (v[i] > max[i])
            return -1;
    if (!v[15] || !v[16] || v[15] > v[16] || v[7] % 10)
        return -1;
    GameSave t;
    game_defaults(&t);
    GameOptions *o = &t.options;
    int *fields[] = {&o->display,   &o->perspective, &o->vibration, &o->subtitles, &o->forced,
                     &o->language,  &o->music,       &o->volume,    &o->unlimited, &o->fast,
                     &o->radar,     &o->plush[0],    &o->plush[1],  &o->plush[2],  &o->plush[3],
                     &t.next_night, &t.unlocked,     &t.stars,      &t.custom[0],  &t.custom[1],
                     &t.custom[2],  &t.custom[3]};
    for (int i = 0; i < 22; ++i)
        *fields[i] = v[i];
    if (t.next_night > 5)
        t.next_night = 5;
    if (in[26] == 1 && in[27] < CONTROL_LAYOUT_COUNT) {
        unsigned used = 0;
        int valid = 1;
        for (int i = 0; i < CONTROL_ACTION_COUNT; ++i) {
            unsigned button = in[28 + i];
            if (button >= CONTROL_BUTTON_COUNT || (button && (used & (1u << button)))) {
                valid = 0;
                break;
            }
            if (button)
                used |= 1u << button;
        }
        if (valid) {
            o->control_layout = in[27];
            memcpy(o->control_buttons, in + 28, CONTROL_ACTION_COUNT);
        }
    }
    *s = t;
    return 0;
}
