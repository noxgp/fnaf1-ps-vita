#include "camera_map.h"
#include <limits.h>
/* Original scene 05 button anchors, view counters and room-name animations. */
const CameraInfo camera_map[11] = {
    {75, 1, 983, 353, 12},  {76, 2, 963, 409, 0},   {85, 99, 931, 487, 14}, {77, 3, 983, 603, 15},
    {78, 22, 983, 643, 17}, {79, 33, 899, 567, 20}, {80, 4, 1089, 604, 21}, {81, 42, 1089, 644, 16},
    {82, 5, 857, 436, 13},  {83, 6, 1186, 568, 19}, {84, 7, 1195, 437, 18}};
int camera_map_move(int current, int dx, int dy) {
    if (current < 0 || current >= 11 || (!dx && !dy))
        return current;
    for (int pass = 0; pass < 2; ++pass) {
        int best = current, score = INT_MAX;
        for (int i = 0; i < 11; ++i) {
            int x = camera_map[i].x - camera_map[current].x,
                y = camera_map[i].y - camera_map[current].y;
            int along = x * dx + y * dy, across = x * dy - y * dx;
            /* A one-pixel editor offset does not make the opposite hall a
             * lower row. Treat aligned map buttons as the same row/column. */
            if (along <= 12 || (!pass && (across > along || across < -along)))
                continue;
            int candidate = along * along + 4 * across * across;
            if (candidate < score) {
                best = i;
                score = candidate;
            }
        }
        if (best != current)
            return best;
    }
    return current;
}
int camera_map_hit(int x, int y) {
    /* Actual 59 x 40 source rectangles; adjacent hall buttons do not overlap. */
    for (int i = 0; i < 11; ++i) {
        int left = (camera_map[i].x - 28) * 3 / 4, top = 2 + (camera_map[i].y - 19) * 3 / 4;
        int right = (camera_map[i].x + 31) * 3 / 4, bottom = 2 + (camera_map[i].y + 21) * 3 / 4;
        if (x >= left && x < right && y >= top && y < bottom)
            return i;
    }
    return -1;
}
