#include "perspective_mesh.h"
static void column(PerspectiveVertex *v, float x, float y, float h, float u, float half_width) {
    float scale = office_height_scale(x, half_width);
    float top = 272 + (y - 272) * scale;
    v[0] = (PerspectiveVertex){x, top, .5f, u, 0};
    v[1] = (PerspectiveVertex){x, top + h * scale, .5f, u, 1};
}
/* Keep the room, door and lit patch on the same screen-space mesh. */
unsigned perspective_mesh(PerspectiveVertex *v, float x, float y, float w, float h,
                          float half_width) {
    float left = 480.0f - half_width, right = 480.0f + half_width;
    float start = x > left ? x : left, end = x + w < right ? x + w : right;
    if (start >= end)
        return 0;
    column(v, start, y, h, (start - x) / w, half_width);
    unsigned count = 2;
    for (int i = 1; i < PERSPECTIVE_SEGMENTS; ++i) {
        float px = left + (right - left) * i / PERSPECTIVE_SEGMENTS;
        if (px > start && px < end) {
            column(v + count, px, y, h, (px - x) / w, half_width);
            count += 2;
        }
    }
    column(v + count, end, y, h, (end - x) / w, half_width);
    return count + 2;
}
