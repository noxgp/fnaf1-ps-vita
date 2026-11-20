#include "game_audio.h"
#include "data_files.h"
#include "game_events.h"
#include "audio_cache.h"
#include "generated_audio_bank.h"
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>
typedef struct {
    int id, channel, loop, volume;
} Command;
typedef struct {
    const AudioSample *sample;
    AudioMemory memory;
    OggVorbis_File ogg;
    int vorbis, active, loop, volume, id;
    unsigned frames;
} Voice;
static Voice voices[AUDIO_VOICES];
static Command queue[128];
static AudioCache bank;
static atomic_uint head, tail, positions[AUDIO_VOICES];
static atomic_int running, paused, master, music_enabled, errors, ready;
static SceUID thread = -1;
static void close_voice(Voice *v) {
    if (v->active && v->vorbis)
        ov_clear(&v->ogg);
    memset(v, 0, sizeof(*v));
}
static int open_voice(Voice *v, Command c) {
    close_voice(v);
    atomic_store(&positions[c.channel], 0);
    if (c.id < 0)
        return 0;
    if (c.id >= AUDIO_ASSETS || !bank.sample[c.id].data)
        return -1;
    v->sample = &bank.sample[c.id];
    v->id = c.id;
    v->loop = c.loop;
    v->volume = c.volume;
    if (!v->sample->vorbis) {
        v->memory = (AudioMemory){v->sample->data + v->sample->pcm_offset, v->sample->pcm_bytes, 0};
        v->active = 1;
        return 0;
    }
    v->memory = (AudioMemory){v->sample->data, v->sample->bytes, 0};
    ov_callbacks callbacks = {audio_memory_read, audio_memory_seek, audio_memory_close,
                              audio_memory_tell};
    if (ov_open_callbacks(&v->memory, &v->ogg, NULL, 0, callbacks) < 0)
        return -1;
    vorbis_info *info = ov_info(&v->ogg, -1);
    if (!info || info->channels != 2 || info->rate != 44100) {
        ov_clear(&v->ogg);
        return -1;
    }
    v->active = v->vorbis = 1;
    return 0;
}
static unsigned read_voice(Voice *v, int16_t *pcm) {
    unsigned used = 0, attempts = 0;
    while (used < 4096 && v->active) {
        long got;
        if (v->vorbis) {
            int stream;
            got = ov_read(&v->ogg, (char *)pcm + used, 4096 - used, 0, 2, 1, &stream);
        } else
            got = (long)audio_memory_read((char *)pcm + used, 1, 4096 - used, &v->memory);
        if (got > 0) {
            used += (unsigned)got;
            attempts = 0;
        } else if (!got && v->loop && attempts++ < 2) {
            if (v->vorbis) {
                if (ov_pcm_seek(&v->ogg, 0))
                    break;
            } else if (audio_memory_seek(&v->memory, 0, SEEK_SET))
                break;
        } else if (got < 0 && attempts++ < 3)
            continue;
        else {
            close_voice(v);
            break;
        }
    }
    return used / 4;
}
static void audio_step(int16_t out[2048]) {
    int16_t pcm[2048];
    int32_t mix[2048];
    unsigned t = atomic_load_explicit(&tail, memory_order_relaxed),
             h = atomic_load_explicit(&head, memory_order_acquire);
    while (t != h) {
        Command c = queue[t % 128];
        if (c.id == -2) {
            voices[c.channel].volume = c.volume;
            ++t;
            continue;
        }
        if (open_voice(&voices[c.channel], c))
            atomic_fetch_add(&errors, 1);
        ++t;
    }
    atomic_store_explicit(&tail, t, memory_order_release);
    memset(mix, 0, sizeof(mix));
    if (!atomic_load(&paused))
        for (int channel = 0; channel < AUDIO_VOICES; ++channel) {
            Voice *v = &voices[channel];
            if (!v->active)
                continue;
            int volume = v->volume;
            if (!atomic_load(&music_enabled) && (v->id == 35 || v->id == 30))
                volume = 0;
            unsigned frames = v->frames, count = read_voice(v, pcm);
            if (v->active)
                v->frames = frames + count;
            atomic_store(&positions[channel],
                         (unsigned)((uint64_t)(frames + count) * 1000u / 44100u));
            for (unsigned i = 0; i < count * 2; ++i)
                mix[i] += (int)pcm[i] * volume / 100;
        }
    int volume = atomic_load(&master);
    for (int i = 0; i < 2048; ++i) {
        int value = mix[i] * volume / 100;
        out[i] = (int16_t)(value < -32768 ? -32768 : value > 32767 ? 32767 : value);
    }
}
#ifdef GAME_AUDIO_TEST
void game_audio_test_step(int16_t out[2048]) {
    audio_step(out);
}
int game_audio_test_prepare(const AudioAsset *assets, unsigned count) {
    int result = audio_cache_load(&bank, assets, count);
    atomic_store(&ready, 1);
    return result;
}
#endif
static int worker(SceSize n, void *p) {
    (void)n;
    (void)p;
    (void)audio_cache_load(&bank, audio_assets, sizeof(audio_assets) / sizeof(*audio_assets));
    atomic_fetch_add(&errors, (int)bank.errors);
    int port =
        sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, 1024, 44100, SCE_AUDIO_OUT_MODE_STEREO);
    if (port < 0) {
        atomic_fetch_add(&errors, 1);
        atomic_store(&ready, 1);
        return 0;
    }
    atomic_store_explicit(&ready, 1, memory_order_release);
    int16_t out[2048];
    while (atomic_load(&running)) {
        audio_step(out);
        if (sceAudioOutOutput(port, out) < 0) {
            atomic_fetch_add(&errors, 1);
            break;
        }
    }
    for (int i = 0; i < AUDIO_VOICES; ++i)
        close_voice(&voices[i]);
    sceAudioOutOutput(port, NULL);
    sceAudioOutReleasePort(port);
    return 0;
}
int game_audio_start(void) {
    if (thread >= 0)
        return 0;
    atomic_store(&ready, 0);
    atomic_store(&master, 80);
    atomic_store(&music_enabled, 1);
    atomic_store(&running, 1);
    thread = sceKernelCreateThread("FnafSound", worker, 0x10000100, 256 * 1024, 0,
                                   SCE_KERNEL_CPU_MASK_USER_1, NULL);
    if (thread < 0)
        thread = sceKernelCreateThread("FnafSound", worker, 0x10000100, 256 * 1024, 0, 0, NULL);
    if (thread >= 0 && sceKernelStartThread(thread, 0, NULL) >= 0)
        return 0;
    if (thread >= 0)
        sceKernelDeleteThread(thread);
    thread = -1;
    atomic_store(&running, 0);
    atomic_fetch_add(&errors, 1);
    atomic_store(&ready, 1);
    return -1;
}
void game_audio_stop(void) {
    atomic_store(&running, 0);
    if (thread >= 0) {
        sceKernelWaitThreadEnd(thread, NULL, NULL);
        sceKernelDeleteThread(thread);
        thread = -1;
    }
    for (int i = 0; i < AUDIO_VOICES; ++i)
        close_voice(&voices[i]);
    audio_cache_clear(&bank);
    atomic_store(&ready, 0);
}
void game_audio_play(int id, int channel, int loop, int volume) {
    if (channel < 0 || channel >= AUDIO_VOICES)
        return;
    unsigned h = atomic_load_explicit(&head, memory_order_relaxed);
    if (h - atomic_load_explicit(&tail, memory_order_acquire) >= 128) {
        atomic_fetch_add(&errors, 1);
        return;
    }
    queue[h % 128] = (Command){id, channel, loop, volume};
    atomic_store_explicit(&head, h + 1, memory_order_release);
}
void game_audio_settings(int volume, int pause, int music) {
    atomic_store(&master, volume);
    atomic_store(&paused, pause);
    atomic_store(&music_enabled, music);
}
unsigned game_audio_position(int channel) {
    return channel >= 0 && channel < AUDIO_VOICES ? atomic_load(&positions[channel]) : 0;
}
int game_audio_errors(void) {
    return atomic_load(&errors);
}

int game_audio_ready(void) {
    return atomic_load_explicit(&ready, memory_order_acquire);
}
