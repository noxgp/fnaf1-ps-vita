#include "perspective_mesh.h"
/* A continuous triangle strip: one submission instead of 32 sprite draws. */
void perspective_mesh(PerspectiveVertex *v, float x, float y, float w, float h, float half_width) {
    for (int i = 0; i <= PERSPECTIVE_SEGMENTS; ++i) {
        float u = (float)i / PERSPECTIVE_SEGMENTS, px = x + w * u;
        /* Use the same screen-space projection for every office layer. */
        float scale = office_height_scale(px, half_width);
        float height = h * scale, top = 272 + (y - 272) * scale;
        v[i * 2] = (PerspectiveVertex){px, top, .5f, u, 0};
        v[i * 2 + 1] = (PerspectiveVertex){px, top + height, .5f, u, 1};
    }
}
