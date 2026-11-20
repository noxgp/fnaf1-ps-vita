#include "data_files.h"
#include <stdio.h>
#include <string.h>

int fnaf_data_check(void) {
    FILE *file = fopen(FNAF_DATA_ROOT "data.version", "rb");
    if (!file) return FNAF_DATA_MISSING;
    char version[64];
    size_t count = fread(version, 1, sizeof(version) - 1, file);
    fclose(file);
    if (count == sizeof(version) - 1) return FNAF_DATA_INCOMPATIBLE;
    while (count && (version[count - 1] == '\n' || version[count - 1] == '\r')) --count;
    version[count] = '\0';
    if (count != sizeof(FNAF_DATA_VERSION) - 1 || strcmp(version, FNAF_DATA_VERSION))
        return FNAF_DATA_INCOMPATIBLE;
    return FNAF_DATA_READY;
}
