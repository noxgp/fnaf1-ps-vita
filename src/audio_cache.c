/* Immutable sound files are loaded once, before playback. Both PCM effects
 * and compressed OGG streams subsequently read RAM, never the storage device. */
#include "audio_cache.h"
#include "data_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned le16(const unsigned char *p) {
    return p[0] | (unsigned)p[1] << 8;
}
static unsigned le32(const unsigned char *p) {
    return le16(p) | le16(p + 2) << 16;
}
static int pcm_info(AudioSample *s) {
    const unsigned char *p = s->data;
    if (s->bytes < 12 || memcmp(p, "RIFF", 4) || memcmp(p + 8, "WAVE", 4))
        return -1;
    int format = 0;
    for (unsigned pos = 12; pos <= s->bytes - 8;) {
        unsigned n = le32(p + pos + 4);
        if (n > s->bytes - pos - 8)
            return -1;
        if (!memcmp(p + pos, "fmt ", 4) && n >= 16) {
            const unsigned char *f = p + pos + 8;
            format = le16(f) == 1 && le16(f + 2) == 2 && le32(f + 4) == 44100 && le16(f + 14) == 16;
        } else if (!memcmp(p + pos, "data", 4) && format && n && !(n & 3)) {
            s->pcm_offset = pos + 8;
            s->pcm_bytes = n;
            return 0;
        }
        if (n > 0x7fffffffu)
            return -1;
        pos += 8 + n + (n & 1);
    }
    return -1;
}
int audio_cache_load(AudioCache *c, const AudioAsset *assets, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        AudioAsset a = assets[i];
        if (a.id < 0 || a.id >= AUDIO_ASSETS || c->sample[a.id].data) {
            ++c->errors;
            continue;
        }
        char path[96];
        snprintf(path, sizeof(path), FNAF_DATA_ROOT "audio/s%04d.%s", a.id,
                 a.vorbis ? "ogg" : "wav");
        FILE *f = fopen(path, "rb");
        if (!f) {
            ++c->errors;
            continue;
        }
        long length = -1;
        if (!fseek(f, 0, SEEK_END))
            length = ftell(f);
        if (length < 4 || (unsigned long)length > AUDIO_CACHE_LIMIT - c->bytes ||
            fseek(f, 0, SEEK_SET)) {
            fclose(f);
            ++c->errors;
            continue;
        }
        AudioSample s = {0};
        s.bytes = (unsigned)length;
        s.vorbis = a.vorbis;
        s.data = malloc(s.bytes);
        int ok = s.data && fread(s.data, 1, s.bytes, f) == s.bytes;
        fclose(f);
        if (ok)
            ok = s.vorbis ? !memcmp(s.data, "OggS", 4) : !pcm_info(&s);
        if (!ok) {
            free(s.data);
            ++c->errors;
            continue;
        }
        c->sample[a.id] = s;
        c->bytes += s.bytes;
        ++c->loaded;
    }
    return c->errors ? -1 : 0;
}
void audio_cache_clear(AudioCache *c) {
    for (int i = 0; i < AUDIO_ASSETS; ++i)
        free(c->sample[i].data);
    memset(c, 0, sizeof(*c));
}
size_t audio_memory_read(void *out, size_t size, size_t count, void *source) {
    AudioMemory *m = source;
    if (!size || !count || m->position > m->bytes)
        return 0;
    size_t available = (m->bytes - m->position) / size;
    if (count > available)
        count = available;
    memcpy(out, m->data + m->position, size * count);
    m->position += size * count;
    return count;
}
int audio_memory_seek(void *source, int64_t offset, int whence) {
    AudioMemory *m = source;
    int64_t base = whence == SEEK_SET   ? 0
                   : whence == SEEK_CUR ? (int64_t)m->position
                   : whence == SEEK_END ? (int64_t)m->bytes
                                        : -1;
    if (base < 0 || offset < -base || offset > (int64_t)m->bytes - base)
        return -1;
    m->position = (size_t)(base + offset);
    return 0;
}
long audio_memory_tell(void *source) {
    return (long)((AudioMemory *)source)->position;
}
int audio_memory_close(void *source) {
    (void)source;
    return 0;
}
