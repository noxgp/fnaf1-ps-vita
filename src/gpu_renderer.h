#ifndef FNAF_GPU_RENDERER_H
#define FNAF_GPU_RENDERER_H
#include <stdint.h>
int gpu_init(void);
void gpu_shutdown(void);
void gpu_reset_scene(void);
void gpu_prepare(void);
void gpu_warm_title(void);
void gpu_begin(void);
void gpu_draw_scene(void);
void gpu_text(int x, int y, const char *value, uint32_t color);
void gpu_rectangle(int x, int y, int width, int height, uint32_t color);
void gpu_check_badge(int x, int y);
void gpu_end(void);
unsigned gpu_asset_errors(void);
typedef struct {
    unsigned prepare_us, sync_us, upload_us, draw_us, present_us, loads, upload_bytes, evictions,
        cache_bytes, pending, read_us, draw_calls;
} GpuStats;
void gpu_get_stats(GpuStats *stats);
int gpu_texture_ready(int handle);
int gpu_texture_dimensions(int handle, unsigned *width, unsigned *height);
void gpu_queue_reset(int display);
void gpu_queue_discard_draws(void);
void gpu_queue_image(int handle, float x, float y, float w, float h, unsigned color,
                     int perspective);
void gpu_queue_prefetch(int handle);
void gpu_queue_resident(int handle);
void gpu_queue_focus(int handle);
void gpu_queue_warm(int handle);
void gpu_queue_prepare(void);
int gpu_queue_draw_ready(void);
void gpu_queue_draw(void);
#endif
