#include "data_files.h"
#include <stdio.h>
#include <string.h>

static int version_status(const char *path, const char *expected, size_t length) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return FNAF_DATA_MISSING;
    char version[64];
    size_t count = fread(version, 1, sizeof(version) - 1, file);
    fclose(file);
    if (count == sizeof(version) - 1)
        return FNAF_DATA_INCOMPATIBLE;
    while (count && (version[count - 1] == '\n' || version[count - 1] == '\r'))
        --count;
    if (count != length || memcmp(version, expected, count))
        return FNAF_DATA_INCOMPATIBLE;
    return FNAF_DATA_READY;
}

int fnaf_data_check(void) {
    return version_status(FNAF_DATA_ROOT "data.version", FNAF_DATA_VERSION,
                          sizeof(FNAF_DATA_VERSION) - 1);
}

static int has_header(const char *path, const char *magic, size_t size) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return 0;
    char header[12];
    size_t count = fread(header, 1, size, file);
    fclose(file);
    return count == size && (!magic || !memcmp(header, magic, 4));
}

static int sprites_ready(const char *const files[], size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (!has_header(files[i], "FSPI", 12))
            return 0;
    return 1;
}

int fnaf_game_data_check(void) {
    int game = version_status(FNAF_DATA_ROOT "game.version", FNAF_GAME_DATA_VERSION,
                              sizeof(FNAF_GAME_DATA_VERSION) - 1);
    if (game == FNAF_DATA_MISSING && fnaf_data_check() != FNAF_DATA_MISSING)
        return FNAF_DATA_INCOMPATIBLE;
    if (game != FNAF_DATA_READY)
        return game;

    static const char *const sprites[] = {
        FNAF_DATA_ROOT "sprites/0591.rgba", FNAF_DATA_ROOT "sprites/0018.rgba",
        FNAF_DATA_ROOT "sprites/0059.rgba", FNAF_DATA_ROOT "sprites/0045.rgba",
        FNAF_DATA_ROOT "sprites/0056.rgba", FNAF_DATA_ROOT "sprites/0090.rgba",
        FNAF_DATA_ROOT "sprites/0307.rgba", FNAF_DATA_ROOT "sprites/0103.rgba",
        FNAF_DATA_ROOT "sprites/0040.rgba", FNAF_DATA_ROOT "sprites/0344.rgba",
        FNAF_DATA_ROOT "sprites/0301.rgba", FNAF_DATA_ROOT "sprites/0574.rgba",
        FNAF_DATA_ROOT "sprites/0027.rgba", FNAF_DATA_ROOT "sprites/0028.rgba",
        FNAF_DATA_ROOT "sprites/0029.rgba", FNAF_DATA_ROOT "sprites/0030.rgba",
    };
    if (!sprites_ready(sprites, sizeof(sprites) / sizeof(*sprites)))
        return FNAF_DATA_MISSING;

    static const char *const ui[] = {
        FNAF_DATA_ROOT "ui/7000.rgba", FNAF_DATA_ROOT "ui/7007.rgba",
        FNAF_DATA_ROOT "ui/7800.rgba", FNAF_DATA_ROOT "ui/7801.rgba",
    };
    int ui_version = version_status(FNAF_DATA_ROOT "public-ui.version", FNAF_PUBLIC_UI_VERSION,
                                    sizeof(FNAF_PUBLIC_UI_VERSION) - 1);
    if (ui_version != FNAF_DATA_READY ||
        !sprites_ready(ui, sizeof(ui) / sizeof(*ui)) ||
        !has_header(FNAF_DATA_ROOT "ui/subtitles.idx", "FCIX", 12) ||
        !has_header(FNAF_DATA_ROOT "ui/subtitles.pack", NULL, 2))
        return FNAF_DATA_NEEDS_UI;
    return FNAF_DATA_READY;
}
