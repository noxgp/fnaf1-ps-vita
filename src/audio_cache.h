#ifndef FNAF_AUDIO_CACHE_H
#define FNAF_AUDIO_CACHE_H
#include <stdint.h>
#include <stddef.h>
enum { AUDIO_ASSETS = 60, AUDIO_CACHE_LIMIT = 48 * 1024 * 1024 };
typedef struct {
    int id, vorbis;
    unsigned bytes;
} AudioAsset;
typedef struct {
    unsigned char *data;
    unsigned bytes, pcm_offset, pcm_bytes;
    int vorbis;
} AudioSample;
typedef struct {
    AudioSample sample[AUDIO_ASSETS];
    unsigned bytes, loaded, errors;
} AudioCache;
typedef struct {
    const unsigned char *data;
    size_t bytes, position;
} AudioMemory;
int audio_cache_load(AudioCache *cache, const AudioAsset *assets, unsigned count);
void audio_cache_clear(AudioCache *cache);
size_t audio_memory_read(void *out, size_t size, size_t count, void *source);
int audio_memory_seek(void *source, int64_t offset, int whence);
long audio_memory_tell(void *source);
int audio_memory_close(void *source);
#endif
