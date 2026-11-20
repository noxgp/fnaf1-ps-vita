/* Test-only stdio fixtures for checking the real external-data loader on ARM. */
#include <stddef.h>
#include <stdio.h>
#include "data_files.h"

int io_fixture;
int io_bad_path;

FILE *fopen(const char *path, const char *mode) {
    const char *expected = FNAF_DATA_ROOT "data.version";
    size_t i = 0;
    while (expected[i] && path[i] == expected[i]) ++i;
    if (expected[i] != path[i] || mode[0] != 'r' || mode[1] != 'b' || mode[2]) io_bad_path = 1;
    return io_fixture == 0 ? NULL : (FILE *)1;
}

size_t fread(void *output, size_t size, size_t count, FILE *file) {
    (void)file;
    const char *value = FNAF_DATA_VERSION "\n";
    if (io_fixture == 2) value = "OTHER-DATA-VERSION\n";
    if (io_fixture == 3) value = "";
    if (io_fixture == 4) value = FNAF_DATA_VERSION "\r\n";
    if (io_fixture == 5) value = FNAF_DATA_VERSION "extra";
    size_t used = 0;
    unsigned char *bytes = (unsigned char *)output;
    while (value[used] && used < size * count) { bytes[used] = (unsigned char)value[used]; ++used; }
    return size ? used / size : 0;
}

int fclose(FILE *file) { (void)file; return 0; }
