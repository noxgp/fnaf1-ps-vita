#ifndef FNAF_ASSET_STREAM_H
#define FNAF_ASSET_STREAM_H
#include <stdint.h>
enum { ASSET_SLOTS = 8192, ASSET_RAM_LIMIT = 16 * 1024 * 1024 };
typedef struct {
    unsigned width, height, bytes;
    unsigned char *pixels;
} AssetBuffer;
int asset_stream_start(void);
void asset_stream_stop(void);
void asset_stream_request(int handle, int urgent);
void asset_stream_retain_requests(const unsigned char wanted[ASSET_SLOTS]);
int asset_stream_take(int handle, AssetBuffer *buffer);
void asset_stream_release(AssetBuffer *buffer);
unsigned asset_stream_pending(void);
unsigned asset_stream_errors(void);
unsigned asset_stream_read_us(void);
#endif
