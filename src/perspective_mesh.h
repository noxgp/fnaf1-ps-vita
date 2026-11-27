#ifndef FNAF_PERSPECTIVE_MESH_H
#define FNAF_PERSPECTIVE_MESH_H
enum { PERSPECTIVE_SEGMENTS = 32, PERSPECTIVE_VERTICES = (PERSPECTIVE_SEGMENTS + 1) * 2 };
typedef struct {
    float x, y, z, u, v;
} PerspectiveVertex;
static inline float office_height_scale(float x, float half_width) {
    float cell = (x - (480.0f - half_width)) * PERSPECTIVE_SEGMENTS / (2 * half_width);
    if (cell <= 0 || cell >= PERSPECTIVE_SEGMENTS)
        return 1.18f;
    int whole = (int)cell;
    const float step = 2.0f / PERSPECTIVE_SEGMENTS;
    float a = whole * step - 1.0f, b = a + step;
    return 1.0f + .18f * (a * a + (b * b - a * a) * (cell - whole));
}
unsigned perspective_mesh(PerspectiveVertex *vertices, float x, float y, float width, float height,
                          float half_width);
#endif
