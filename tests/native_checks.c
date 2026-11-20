#include "scene_engine.h"
#include "raster.h"
#include "data_files.h"
#include "menu_ui.h"
#include "audio_mix.h"
#include <stddef.h>

/* Runs the actual ARM code under Unicorn without Vita OS calls. Returns a
 * failing check ID, or zero. The Vita build links the same engine and raster. */
int run_checks(void) {
    extern int io_fixture, io_bad_path;
    if (scene_open(3))
        return 1;
    scene_tick(0);
    Instance *bar = scene_find(18), *noise = scene_find(9), *sixth = scene_find(20);
    if (!bar || !noise || !sixth)
        return 2;
    if (runtime.layer_visible[2])
        return 3;
    if (sixth->visible)
        return 4;
    if (bar->scale_x != 83.0f)
        return 5;
    if (noise->x != 0 || noise->y != 0)
        return 6;
    for (int i = 0; i < 30; ++i)
        scene_tick(20);
    if (noise->alpha < 50 || noise->alpha >= 150)
        return 7;
    if (runtime.executed_actions < 20 || runtime.unsupported_conditions == 0)
        return 8;
    if (runtime.pending_scene != -1)
        return 9;

    const Expression invalid = {NULL, 0, 0, 0};
    int valid = 1;
    (void)scene_evaluate(&invalid, &valid);
    if (valid)
        return 10;
    const Instruction zero_division[] = {
        {EX_CONST, 0, 0, 1}, {EX_CONST, 0, 0, 0}, {EX_DIV, 0, 0, 0}};
    const Expression divide = {zero_division, 3, 1, 0};
    valid = 1;
    (void)scene_evaluate(&divide, &valid);
    if (valid)
        return 11;

    uint32_t guarded[18];
    for (int i = 0; i < 18; ++i)
        guarded[i] = 0xff000000u;
    guarded[0] = 0x12345678u;
    guarded[17] = 0x87654321u;
    const uint32_t source[] = {0xff0000ffu, 0xff00ff00u, 0xffff0000u, 0xffffffffu};
    raster_blit(guarded + 1, 4, 4, source, 2, 2, -1, -1, 2, 2, 0, 1);
    if (guarded[1] != 0xffffffffu)
        return 12;
    if (guarded[0] != 0x12345678u || guarded[17] != 0x87654321u)
        return 13;
    const uint32_t half_red[] = {0x800000ffu};
    raster_blit(guarded + 1, 4, 4, half_red, 1, 1, 1, 1, 1, 1, 0, 0);
    if (guarded[6] != 0xff000080u)
        return 14;
    raster_blit(guarded + 1, 4, 4, source, 2, 2, 0, 0, 4, 4, 255, 1);
    if (guarded[6] != 0xff000080u)
        return 15;
    if (scene_open(0))
        return 16;
    if (runtime.tick != 0 || runtime.time_ms != 0)
        return 17;
    if (!scene_find(0) || scene_instance_image(scene_find(0)) != 24)
        return 18;
    if (scene_open(1234) != -1 || runtime.scene->index != 0)
        return 19;
    scene_tick(0);
    for (int i = 0; i < 30; ++i)
        scene_tick(20);
    Instance *fade = scene_find(1);
    if (!fade || fade->variables[0] < 100 || fade->variables[0] > 200)
        return 20;
    io_fixture = 0;
    if (fnaf_data_check() != FNAF_DATA_MISSING)
        return 21;
    io_fixture = 1;
    if (fnaf_data_check() != FNAF_DATA_READY)
        return 22;
    io_fixture = 2;
    if (fnaf_data_check() != FNAF_DATA_INCOMPATIBLE)
        return 23;
    io_fixture = 3;
    if (fnaf_data_check() != FNAF_DATA_INCOMPATIBLE)
        return 24;
    io_fixture = 4;
    if (fnaf_data_check() != FNAF_DATA_READY)
        return 25;
    io_fixture = 5;
    if (fnaf_data_check() != FNAF_DATA_INCOMPATIBLE)
        return 26;
    if (io_bad_path)
        return 27;
    MenuUI ui;
    menu_ui_reset(&ui);
    if (ui.selection != 0 || !ui.music || menu_ui_object(&ui) != 12)
        return 28;
    menu_ui_move(&ui, -1);
    if (ui.selection != 3 || menu_ui_object(&ui) != 33)
        return 29;
    menu_ui_activate(&ui);
    if (ui.panel != MENU_UNLOCKS)
        return 30;
    menu_ui_back(&ui);
    menu_ui_move(&ui, -1);
    menu_ui_activate(&ui);
    if (ui.panel != MENU_OPTIONS)
        return 31;
    menu_ui_activate(&ui);
    if (ui.music)
        return 32;
    menu_ui_move(&ui, 1);
    menu_ui_activate(&ui);
    if (ui.panel != MENU_TITLE)
        return 33;
    menu_ui_reset(&ui);
    menu_ui_activate(&ui);
    if (ui.panel != MENU_NOT_READY)
        return 34;
    int16_t music_samples[] = {32767, -32768, 1000, -1000};
    const int16_t click_sample[] = {32767};
    unsigned cursor = 0;
    mix_menu_audio(music_samples, 2, click_sample, 1, &cursor, 0);
    if (cursor != 1 || music_samples[0] != 32766 || music_samples[1] != -1)
        return 35;
    if (music_samples[2] != 500 || music_samples[3] != -500)
        return 36;
    cursor = 0;
    mix_menu_audio(music_samples, 2, click_sample, 1, &cursor, 1);
    if (music_samples[0] || music_samples[1] || music_samples[2] || music_samples[3])
        return 37;
    extern int run_game_checks(void), run_event_checks(void);
    int result = run_game_checks();
    return result ? result : run_event_checks();
}

/* Exported entry points for creating a native renderer reference image. */
uint32_t reference_frame[960 * 544];
int reference_commands[MAX_INSTANCES][6];
int capture_scene(int original_index) {
    if (scene_open(original_index))
        return -1;
    scene_tick(0);
    for (int i = 0; i < 30; ++i)
        scene_tick(20);
    int count = 0;
    for (int layer = 0; layer < 32; ++layer) {
        if (!runtime.layer_visible[layer])
            continue;
        for (int i = 0; i < runtime.count; ++i) {
            Instance *instance = &runtime.instances[i];
            if (!instance->create || !instance->visible || instance->layer != layer ||
                instance->alpha == 255)
                continue;
            int handle = scene_instance_image(instance);
            const ImageDef *image = scene_image(handle);
            if (!image)
                continue;
            int *command = reference_commands[count];
            command[0] = handle;
            scene_geometry(instance, image, &command[1], &command[2], &command[3], &command[4]);
            command[5] = instance->alpha;
            if (command[1] >= 960 || command[2] >= 544 || command[1] + command[3] <= 0 ||
                command[2] + command[4] <= 0)
                continue;
            ++count;
        }
    }
    return count;
}
