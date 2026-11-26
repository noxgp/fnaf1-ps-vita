/* Native frontend. Game simulation uses fixed steps; rendering uses GXM. */
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/rtc.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "game_events.h"
#include "game_view.h"
#include "android_menu.h"
#include "game_audio.h"
#include "gpu_renderer.h"
#include "data_files.h"
#include "scene_engine.h"
#include "touch_input.h"
unsigned int _newlib_heap_size_user = 112 * 1024 * 1024;
static Game game;
static int title_revealed;
static int runtime_loading(void) {
    return (game.phase == G_TITLE && !title_revealed) ||
           (game.phase == G_PLAY && !game_audio_ready()) || game_view_loading(&game);
}
static void draw_data_help(int status) {
    gpu_begin();
    if (status == FNAF_DATA_NEEDS_UI) {
        gpu_text(80, 150, "PUBLIC UI UPDATE REQUIRED", 0xffffffffu);
        gpu_text(80, 205, "USE THE DATA TOOL WITH --UI-ONLY", 0xffffffffu);
        gpu_text(80, 250, "MERGE INTO UX0:DATA/FNAF1/ AND KEEP YOUR SAVES", 0xffffffffu);
    } else {
        const char *title = status == FNAF_DATA_INCOMPATIBLE ? "OLD GAME DATA FORMAT"
                                                             : "GAME DATA REQUIRED";
        gpu_text(80, 150, title, 0xffffffffu);
        gpu_text(80, 205, "PREPARE DATA FROM YOUR OWN ANDROID COPY", 0xffffffffu);
        gpu_text(80, 250, "COPY FNAF1 FOLDER TO UX0:DATA/ AND KEEP YOUR SAVES", 0xffffffffu);
    }
    gpu_text(80, 300, "GITHUB.COM/NOXGP/FNAF1-PS-VITA", 0xffffffffu);
    gpu_text(80, 360, "CROSS: RETRY  START: EXIT", 0xffffffffu);
    gpu_end();
}

static int load_save(GameSave *save, const char *path) {
    unsigned char bytes[128];
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    size_t n = fread(bytes, 1, 128, f);
    int end = fgetc(f);
    fclose(f);
    return n == 128 && end == EOF ? game_decode_save(save, bytes) : -1;
}

static void read_progress(GameSave *save) {
    game_defaults(save);
    if (load_save(save, FNAF_DATA_ROOT "progress.sav"))
        load_save(save, FNAF_DATA_ROOT "progress.bak");
}

static int write_save(const GameSave *save) {
    unsigned char bytes[128];
    game_encode_save(save, bytes);
    FILE *f = fopen(FNAF_DATA_ROOT "progress.tmp", "wb");
    if (!f)
        return -1;
    int good = fwrite(bytes, 1, 128, f) == 128;
    if (fflush(f))
        good = 0;
    if (fclose(f))
        good = 0;
    if (!good)
        return -1;
    GameSave check;
    if (load_save(&check, FNAF_DATA_ROOT "progress.tmp"))
        return -1;
    remove(FNAF_DATA_ROOT "progress.bak");
    rename(FNAF_DATA_ROOT "progress.sav", FNAF_DATA_ROOT "progress.bak");
    if (rename(FNAF_DATA_ROOT "progress.tmp", FNAF_DATA_ROOT "progress.sav")) {
        rename(FNAF_DATA_ROOT "progress.bak", FNAF_DATA_ROOT "progress.sav");
        return -1;
    }
    return 0;
}
static unsigned translate(unsigned b) {
    unsigned r = 0;
    if (b & SCE_CTRL_UP)
        r |= IN_UP;
    if (b & SCE_CTRL_DOWN)
        r |= IN_DOWN;
    if (b & SCE_CTRL_LEFT)
        r |= IN_LEFT;
    if (b & SCE_CTRL_RIGHT)
        r |= IN_RIGHT;
    if (b & SCE_CTRL_CROSS)
        r |= IN_OK;
    if (b & SCE_CTRL_CIRCLE)
        r |= IN_BACK;
    if (b & SCE_CTRL_SQUARE)
        r |= IN_LIGHT;
    if (b & SCE_CTRL_TRIANGLE)
        r |= IN_PHONE;
    if (b & SCE_CTRL_LTRIGGER)
        r |= IN_L;
    if (b & SCE_CTRL_RTRIGGER)
        r |= IN_R;
    if (b & SCE_CTRL_START)
        r |= IN_PAUSE;
    if (b & SCE_CTRL_SELECT)
        r |= IN_HELP;
    return r;
}
int main(void) {
    if (gpu_init()) {
        sceKernelExitProcess(1);
        return 1;
    }
    (void)sceKernelChangeThreadCpuAffinityMask(sceKernelGetThreadId(), SCE_KERNEL_CPU_MASK_USER_0);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    SceTouchPanelInfo panel = {0};
    TouchArea area = {0, 0, 1919, 1087};
    TouchContacts contacts = {0};
    if (sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panel) >= 0) {
        if (panel.maxDispX > panel.minDispX && panel.maxDispY > panel.minDispY)
            area = (TouchArea){panel.minDispX, panel.minDispY, panel.maxDispX, panel.maxDispY};
        else if (panel.maxAaX > panel.minAaX && panel.maxAaY > panel.minAaY)
            area = (TouchArea){panel.minAaX, panel.minAaY, panel.maxAaX, panel.maxAaY};
    }
    GameSave save;
    read_progress(&save);
    uint64_t now = sceKernelGetSystemTimeWide(), last = now, fps_time = now, next_save = 0;
    game_init(&game, &save, (uint32_t)now);
    SceDateTime date = {0};
    if (!sceRtcGetCurrentClockLocalTime(&date))
        game_set_date(&game, date.month, date.day);
    int data_status = fnaf_game_data_check(), save_error = 0, rumbling = 0;
    unsigned previous = 0, accum = 0, frames = 0, fps = 0, clock_ms = 0, microseconds = 0;
    GameSound pending_audio[AUDIO_VOICES] = {0};
    unsigned pending_channels = 0;
    if (data_status == FNAF_DATA_READY)
        game_audio_start();
    while (!game.quit) {
        if (game.phase != G_TITLE)
            title_revealed = 0;
        SceCtrlData pad = {0};
        sceCtrlPeekBufferPositive(0, &pad, 1);
        unsigned pressed = pad.buttons & ~previous;
        previous = pad.buttons;
        if (data_status != FNAF_DATA_READY) {
            if (pressed & SCE_CTRL_START)
                break;
            if (pressed & SCE_CTRL_CROSS) {
                data_status = fnaf_game_data_check();
                if (data_status == FNAF_DATA_READY) {
                    read_progress(&game.save);
                    game_audio_start();
                }
            }
            if (data_status != FNAF_DATA_READY)
                draw_data_help(data_status);
            last = sceKernelGetSystemTimeWide();
            continue;
        }
        SceTouchData touch = {0};
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
        unsigned touch_held = 0;
        touch_begin(&contacts);
        for (unsigned t = 0; t < touch.reportNum && t < SCE_TOUCH_MAX_REPORT; ++t) {
            int tx, ty, initial = touch_contact(&contacts, touch.report[t].id);
            if (!runtime_loading() && touch_to_game(area, touch.report[t].x, touch.report[t].y,
                                                    game.save.options.display, &tx, &ty))
                touch_held |= game_touch(&game, tx, ty, initial);
        }
        int analog = (int)pad.lx - 128;
        if (analog > -24 && analog < 24)
            analog = 0;
        int loading = runtime_loading();
        game_input(&game, loading ? translate(pressed) & (IN_PAUSE | IN_HELP) : translate(pressed),
                   loading ? 0 : translate(pad.buttons) | touch_held, loading ? 0 : analog);
        now = sceKernelGetSystemTimeWide();
        uint64_t elapsed = now - last + microseconds;
        unsigned dt = (unsigned)(elapsed / 1000);
        microseconds = (unsigned)(elapsed % 1000);
        last = now;
        if (dt > 100)
            dt = 100;
        clock_ms += dt;
        accum += dt;
        loading = runtime_loading();
        if (loading)
            accum = 0;
        else
            while (accum >= 20) {
                if (runtime_loading()) {
                    accum = 0;
                    break;
                }
                game_tick(&game, 20);
                accum -= 20;
            }
        loading = runtime_loading();
        int pause = loading || game.phase == G_PAUSE || game.phase == G_HELP ||
                    ((game.phase == G_OPTIONS || game.phase == G_AUDIO_OPTIONS ||
                      game.phase == G_CONTROL_OPTIONS) && game.options_parent == G_PAUSE);
        game_audio_settings(game.save.options.volume, pause, game.save.options.music);
        int audio_ready = game_audio_ready() ||
            (game_audio_title_ready() && (android_menu_phase(game.phase) ||
                                          game.phase == G_NEWSPAPER));
        if (audio_ready && pending_channels) {
            for (int i = 0; i < AUDIO_VOICES; ++i) {
                if (pending_channels & (1u << i)) {
                    GameSound s = pending_audio[i];
                    if (s.id >= 0)
                        game_audio_play(s.id, i, s.loop, s.volume);
                }
            }
            pending_channels = 0;
        }
        for (int i = 0; i < game.sound_count; ++i) {
            GameSound s = game.sounds[i];
            if (audio_ready)
                game_audio_play(s.id, s.channel, s.loop, s.volume);
            else if (s.id != -2) {
                pending_audio[s.channel] = s;
                pending_channels |= 1u << s.channel;
            } else if (pending_channels & (1u << s.channel))
                pending_audio[s.channel].volume = s.volume;
        }
        game.sound_count = 0;
        if (game.phase == G_PLAY && audio_ready && game_audio_finished(2))
            game.phone_finished = 1;
        if (game.dirty && now >= next_save) {
            save_error = write_save(&game.save);
            if (!save_error)
                game.dirty = 0;
            next_save = now + 3000000;
        }
        int rumble = game.save.options.vibration &&
                     (game.phase == G_SCARE || game.phase == G_CREEPY_END) && !loading;
        if (rumble != rumbling) {
            SceCtrlActuator a = {0};
            a.small = rumble ? 180 : 0;
            a.large = rumble ? 220 : 0;
            sceCtrlSetActuator(1, &a);
            rumbling = rumble;
        }
        /* Interpolate the visual clock between 20 ms simulation ticks. */
        Game rendered = game;
        if (rendered.phase == G_SCARE && !loading)
            rendered.phase_ms += accum;
        if (rendered.phase == G_PLAY && !loading) {
            if (rendered.monitor_ms)
                rendered.monitor_ms += accum;
            rendered.camera_cut_ms =
                rendered.camera_cut_ms > accum ? rendered.camera_cut_ms - accum : 0;
            for (int i = 0; i < 2; ++i)
                if (rendered.door_ms[i])
                    rendered.door_ms[i] += accum;
        }
        int waiting_for_title = rendered.phase == G_TITLE && !title_revealed;
        if (waiting_for_title)
            rendered.phase_ms = 500;
        game_view_prepare(&rendered, waiting_for_title ? 0 : clock_ms, game_audio_position(2),
                          rendered.phase == G_PLAY && loading);
        if (waiting_for_title && gpu_queue_draw_ready() && game_audio_title_ready()) {
            title_revealed = 1;
            game.phase_ms = rendered.phase_ms;
            game_audio_settings(game.save.options.volume, 0, game.save.options.music);
        }
        gpu_begin();
        if (waiting_for_title && !title_revealed)
            gpu_text(gpu_asset_errors() ? 180 : 810, 506,
                     gpu_asset_errors() ? "MENU FILE ERROR - CHECK GAME FILES" : "Preparing...",
                     0xffffffffu);
        else
            game_view_draw(&rendered, fps, save_error);
        if (game_audio_errors())
            gpu_text(180, 52, "AUDIO FILE ERROR - CHECK GAME FILES V2", 0xff5555ffu);
        gpu_end();
        ++frames;
        now = sceKernelGetSystemTimeWide();
        if (now - fps_time >= 1000000) {
            fps = (unsigned)((uint64_t)frames * 1000000 / (now - fps_time));
            frames = 0;
            fps_time = now;
        }
    }
    if (game.dirty)
        write_save(&game.save);
    SceCtrlActuator off = {0};
    sceCtrlSetActuator(1, &off);
    game_audio_stop();
    gpu_shutdown();
    sceKernelExitProcess(0);
    return 0;
}
