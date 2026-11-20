#ifndef FNAF_RASTER_H
#define FNAF_RASTER_H
#include <stdint.h>
void raster_blit(uint32_t *destination, int destination_width, int destination_height,
                 const uint32_t *source, int source_width, int source_height,
                 int x, int y, int width, int height, int transparency, int opaque);
#endif
