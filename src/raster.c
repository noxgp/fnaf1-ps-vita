#include "raster.h"

void raster_blit(uint32_t *destination, int destination_width, int destination_height,
                 const uint32_t *source, int source_width, int source_height,
                 int x, int y, int width, int height, int transparency, int opaque) {
    if (!destination || !source || width <= 0 || height <= 0 || source_width <= 0 || source_height <= 0 ||
        destination_width <= 0 || destination_height <= 0 || transparency >= 255) return;
    if (transparency < 0) transparency = 0;
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    int right = x + width > destination_width ? destination_width : x + width;
    int bottom = y + height > destination_height ? destination_height : y + height;
    if (left >= right || top >= bottom) return;
    uint32_t opacity = (uint32_t)(255 - transparency);
    for (int dy = top; dy < bottom; ++dy) {
        int sy = (int)((int64_t)(dy - y) * source_height / height);
        const uint32_t *row = source + sy * source_width;
        uint32_t *output = destination + dy * destination_width;
        if (opaque && !transparency && width == source_width) {
            for (int dx = left; dx < right; ++dx) output[dx] = row[dx - x];
            continue;
        }
        for (int dx = left; dx < right; ++dx) {
            int sx = width == source_width ? dx - x : (int)((int64_t)(dx - x) * source_width / width);
            uint32_t pixel = row[sx];
            unsigned alpha = ((pixel >> 24) * opacity + 127) / 255;
            if (!alpha) continue;
            if (alpha == 255) { output[dx] = pixel; continue; }
            uint32_t back = output[dx];
            unsigned inverse = 255 - alpha;
            unsigned red = ((pixel & 255) * alpha + (back & 255) * inverse + 127) / 255;
            unsigned green = (((pixel >> 8) & 255) * alpha + ((back >> 8) & 255) * inverse + 127) / 255;
            unsigned blue = (((pixel >> 16) & 255) * alpha + ((back >> 16) & 255) * inverse + 127) / 255;
            output[dx] = 0xff000000u | red | (green << 8) | (blue << 16);
        }
    }
}
