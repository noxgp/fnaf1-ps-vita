#include "menu_audio.h"
#include "audio_mix.h"
#include "data_files.h"
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

static atomic_int running, click_request, muted, audio_status;
static SceUID thread_id = -1;
static int16_t *click_samples;
static unsigned click_frames;

static unsigned little16(const unsigned char *value) {
    return value[0] | (unsigned)value[1] << 8;
}
static unsigned little32(const unsigned char *value) {
    return little16(value) | little16(value + 2) << 16;
}

static int load_click(void) {
    FILE *file = fopen(FNAF_DATA_ROOT "audio/blip.wav", "rb");
    if (!file)
        file = fopen("app0:/audio/blip.wav", "rb");
    if (!file)
        return -1;
    unsigned char header[12];
    if (fread(header, 1, 12, file) != 12 || memcmp(header, "RIFF", 4) ||
        memcmp(header + 8, "WAVE", 4)) {
        fclose(file);
        return -1;
    }
    int format_ok = 0;
    unsigned chunks = 0;
    while (++chunks < 64) {
        unsigned char chunk[8];
        if (fread(chunk, 1, 8, file) != 8)
            break;
        unsigned size = little32(chunk + 4);
        if (size > 2 * 1024 * 1024)
            break;
        if (!memcmp(chunk, "fmt ", 4) && size >= 16) {
            unsigned char format[16];
            if (fread(format, 1, 16, file) != 16)
                break;
            format_ok = little16(format) == 1 && little16(format + 2) == 1 &&
                        little32(format + 4) == 44100 && little16(format + 14) == 16;
            if (fseek(file, (long)(size - 16 + (size & 1)), SEEK_CUR))
                break;
        } else if (!memcmp(chunk, "data", 4) && format_ok && size && !(size & 1)) {
            click_samples = malloc(size);
            if (!click_samples)
                break;
            if (fread(click_samples, 1, size, file) != size) {
                free(click_samples);
                click_samples = NULL;
                break;
            }
            click_frames = size / 2;
            fclose(file);
            return 0;
        } else if (fseek(file, (long)(size + (size & 1)), SEEK_CUR))
            break;
    }
    fclose(file);
    return -1;
}

static int audio_thread(SceSize args, void *argument) {
    (void)args;
    (void)argument;
    OggVorbis_File music;
    FILE *music_file = fopen(FNAF_DATA_ROOT "audio/menu.ogg", "rb");
    if (!music_file)
        music_file = fopen("app0:/audio/menu.ogg", "rb");
    if (!music_file) {
        atomic_store(&audio_status, -1);
        return 0;
    }
    if (ov_open(music_file, &music, NULL, 0) < 0) {
        fclose(music_file);
        atomic_store(&audio_status, -1);
        return 0;
    }
    vorbis_info *info = ov_info(&music, -1);
    if (!info || info->rate != 44100 || info->channels != 2) {
        ov_clear(&music);
        atomic_store(&audio_status, -2);
        return 0;
    }
    int port =
        sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, 1024, 44100, SCE_AUDIO_OUT_MODE_STEREO);
    if (port < 0) {
        ov_clear(&music);
        atomic_store(&audio_status, -3);
        return 0;
    }
    atomic_store(&audio_status, 1);
    int16_t pcm[2048];
    unsigned click_position = click_frames;
    while (atomic_load(&running)) {
        unsigned used = 0, attempts = 0;
        while (used < sizeof(pcm) && atomic_load(&running)) {
            int stream = 0;
            long got = ov_read(&music, (char *)pcm + used, sizeof(pcm) - used, 0, 2, 1, &stream);
            if (got > 0) {
                used += (unsigned)got;
                attempts = 0;
            } else if (!got && attempts++ < 4) {
                if (ov_pcm_seek(&music, 0))
                    break;
            } else if (got < 0 && attempts++ < 4)
                continue;
            else
                break;
        }
        if (used < sizeof(pcm))
            memset((char *)pcm + used, 0, sizeof(pcm) - used);
        if (!used) {
            atomic_store(&audio_status, -4);
            break;
        }
        if (atomic_exchange(&click_request, 0))
            click_position = 0;
        mix_menu_audio(pcm, 1024, click_samples, click_frames, &click_position,
                       atomic_load(&muted));
        if (sceAudioOutOutput(port, pcm) < 0) {
            atomic_store(&audio_status, -3);
            break;
        }
    }
    sceAudioOutOutput(port, NULL);
    sceAudioOutReleasePort(port);
    ov_clear(&music);
    return 0;
}

int menu_audio_start(void) {
    if (thread_id >= 0)
        return 0;
    atomic_store(&audio_status, 0);
    if (load_click()) {
        atomic_store(&audio_status, -1);
        return -1;
    }
    atomic_store(&running, 1);
    atomic_store(&click_request, 0);
    atomic_store(&muted, 0);
    thread_id =
        sceKernelCreateThread("FnafMenuAudio", audio_thread, 0x10000100, 128 * 1024, 0, 0, NULL);
    if (thread_id < 0 || sceKernelStartThread(thread_id, 0, NULL) < 0) {
        if (thread_id >= 0)
            sceKernelDeleteThread(thread_id);
        thread_id = -1;
        free(click_samples);
        click_samples = NULL;
        atomic_store(&audio_status, -3);
        return -1;
    }
    return 0;
}
void menu_audio_click(void) {
    atomic_store(&click_request, 1);
}
void menu_audio_mute(int mute) {
    atomic_store(&muted, mute != 0);
}
int menu_audio_status(void) {
    return atomic_load(&audio_status);
}
void menu_audio_stop(void) {
    atomic_store(&running, 0);
    if (thread_id >= 0) {
        sceKernelWaitThreadEnd(thread_id, NULL, NULL);
        sceKernelDeleteThread(thread_id);
        thread_id = -1;
    }
    free(click_samples);
    click_samples = NULL;
    click_frames = 0;
}
