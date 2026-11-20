#include "perspective_mesh.h"
/* A continuous triangle strip: one submission instead of 32 sprite draws. */
void perspective_mesh(PerspectiveVertex *v, float x, float y, float w, float h) {
    for (int i = 0; i <= PERSPECTIVE_SEGMENTS; ++i) {
        float u = (float)i / PERSPECTIVE_SEGMENTS, px = x + w * u, offset = (px - 480) / 480;
        float height = h * (1 - .055f * offset * offset), top = y + (h - height) * .5f;
        v[i * 2] = (PerspectiveVertex){px, top, .5f, u, 0};
        v[i * 2 + 1] = (PerspectiveVertex){px, top + height, .5f, u, 1};
    }
}
