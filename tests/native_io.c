/* Test-only stdio fixtures for checking the real external-data loader on ARM. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "data_files.h"

int io_fixture;
int io_bad_path;
static const char *contents;

static int starts_with(const char *path, const char *prefix) {
    while (*prefix && *path == *prefix) {
        ++path;
        ++prefix;
    }
    return !*prefix;
}

static const char *public_file(const char *path) {
    if (!strcmp(path, FNAF_DATA_ROOT "game.version")) {
        if (io_fixture == 6)
            return NULL;
        return io_fixture == 10 ? "OTHER-GAME-DATA\n" : FNAF_GAME_DATA_VERSION "\n";
    }
    if (!strcmp(path, FNAF_DATA_ROOT "data.version"))
        return FNAF_DATA_VERSION "\n";
    if (io_fixture == 9 && !strcmp(path, FNAF_DATA_ROOT "sprites/0029.rgba"))
        return NULL;
    if (starts_with(path, FNAF_DATA_ROOT "sprites/"))
        return "FSPI........";
    if (!strcmp(path, FNAF_DATA_ROOT "public-ui.version"))
        return io_fixture == 7 ? NULL : FNAF_PUBLIC_UI_VERSION "\n";
    if (!strcmp(path, FNAF_DATA_ROOT "ui/subtitles.idx"))
        return io_fixture == 11 ? NULL : "FCIX........";
    if (!strcmp(path, FNAF_DATA_ROOT "ui/subtitles.pack"))
        return "ok";
    if (starts_with(path, FNAF_DATA_ROOT "ui/"))
        return "FSPI........";
    io_bad_path = 1;
    return NULL;
}

FILE *fopen(const char *path, const char *mode) {
    static const char *const scene_versions[] = {
        NULL, FNAF_DATA_VERSION "\n", "OTHER-DATA-VERSION\n", "",
        FNAF_DATA_VERSION "\r\n", FNAF_DATA_VERSION "extra",
    };
    if (strcmp(mode, "rb"))
        io_bad_path = 1;
    if (io_fixture < 6) {
        if (strcmp(path, FNAF_DATA_ROOT "data.version"))
            io_bad_path = 1;
        contents = scene_versions[io_fixture];
    } else
        contents = public_file(path);
    return contents ? (FILE *)1 : NULL;
}

size_t fread(void *output, size_t size, size_t count, FILE *file) {
    (void)file;
    size_t used = 0;
    unsigned char *bytes = (unsigned char *)output;
    while (used < size * count && contents[used]) {
        bytes[used] = (unsigned char)contents[used];
        ++used;
    }
    return size ? used / size : 0;
}

int fclose(FILE *file) {
    (void)file;
    return 0;
}
