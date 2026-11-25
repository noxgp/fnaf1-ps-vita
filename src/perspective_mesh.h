#ifndef FNAF_PERSPECTIVE_MESH_H
#define FNAF_PERSPECTIVE_MESH_H
enum { PERSPECTIVE_SEGMENTS = 32, PERSPECTIVE_VERTICES = (PERSPECTIVE_SEGMENTS + 1) * 2 };
typedef struct {
    float x, y, z, u, v;
} PerspectiveVertex;
static inline float office_height_scale(float x, float half_width) {
    float offset = (x - 480.0f) / half_width;
    return 1.0f + .32f * offset * offset;
}
void perspective_mesh(PerspectiveVertex *vertices, float x, float y, float width, float height,
                      float half_width);
#endif
