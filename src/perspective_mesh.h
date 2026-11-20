#ifndef FNAF_PERSPECTIVE_MESH_H
#define FNAF_PERSPECTIVE_MESH_H
enum { PERSPECTIVE_SEGMENTS = 32, PERSPECTIVE_VERTICES = (PERSPECTIVE_SEGMENTS + 1) * 2 };
typedef struct {
    float x, y, z, u, v;
} PerspectiveVertex;
void perspective_mesh(PerspectiveVertex *vertices, float x, float y, float width, float height);
#endif
