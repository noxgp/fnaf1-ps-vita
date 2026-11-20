/* Native PS Vita resource probe for the supplied Android APK.
 * This program deliberately identifies itself as a resource test.
 * It does not run Clickteam events or simulate FNAF gameplay.
 */
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int _newlib_heap_size_user = 16 * 1024 * 1024;
enum { WIDTH = 960, HEIGHT = 544, IMAGE_HEIGHT = 480, MAX_IMAGES = 2048 };
static uint32_t *screen;

/* Five-column glyphs, top bit first. Authored here to avoid a font dependency. */
static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 :/-+.";
static const unsigned char glyphs[][5] = {
    {126, 9, 9, 9, 126},   {127, 73, 73, 73, 54}, {62, 65, 65, 65, 34},   {127, 65, 65, 34, 28},
    {127, 73, 73, 73, 65}, {127, 9, 9, 9, 1},     {62, 65, 73, 73, 122},  {127, 8, 8, 8, 127},
    {65, 65, 127, 65, 65}, {32, 64, 65, 63, 1},   {127, 8, 20, 34, 65},   {127, 64, 64, 64, 64},
    {127, 2, 12, 2, 127},  {127, 4, 8, 16, 127},  {62, 65, 65, 65, 62},   {127, 9, 9, 9, 6},
    {62, 65, 81, 33, 94},  {127, 9, 25, 41, 70},  {38, 73, 73, 73, 50},   {1, 1, 127, 1, 1},
    {63, 64, 64, 64, 63},  {31, 32, 64, 32, 31},  {127, 32, 24, 32, 127}, {99, 20, 8, 20, 99},
    {3, 4, 120, 4, 3},     {97, 81, 73, 69, 67},  {62, 81, 73, 69, 62},   {0, 66, 127, 64, 0},
    {98, 81, 73, 73, 70},  {34, 65, 73, 73, 54},  {24, 20, 18, 127, 16},  {39, 69, 69, 69, 57},
    {60, 74, 73, 73, 48},  {1, 113, 9, 5, 3},     {54, 73, 73, 73, 54},   {6, 73, 73, 41, 30},
    {0, 0, 0, 0, 0},       {0, 54, 54, 0, 0},     {32, 16, 8, 4, 2},      {8, 8, 8, 8, 8},
    {8, 8, 62, 8, 8},      {0, 96, 96, 0, 0}};

static void text(int x, int y, const char *value, uint32_t color) {
    for (; *value; ++value, x += 12) {
        const char *match = strchr(alphabet, *value);
        if (!match)
            continue;
        const unsigned char *glyph = glyphs[match - alphabet];
        for (int col = 0; col < 5; ++col)
            for (int row = 0; row < 7; ++row)
                if (glyph[col] & (1u << row))
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dx = 0; dx < 2; ++dx) {
                            int px = x + col * 2 + dx, py = y + row * 2 + dy;
                            if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
                                screen[py * WIDTH + px] = color;
                        }
    }
}

static void background(int checker) {
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x) {
            unsigned shade = y < IMAGE_HEIGHT && checker && ((x / 16 + y / 16) & 1) ? 64 : 20;
            screen[y * WIDTH + x] = 0xff000000u | shade * 0x010101u;
        }
}

static int draw_image(int handle, int *width, int *height) {
    char path[80];
    unsigned char header[8];
    snprintf(path, sizeof(path), "app0:/data/images/%04d.rgba", handle);
    FILE *file = fopen(path, "rb");
    if (!file)
        return -1;
    if (fread(header, 1, sizeof(header), file) != sizeof(header) || memcmp(header, "FIMG", 4)) {
        fclose(file);
        return -2;
    }
    int w = header[4] | header[5] << 8, h = header[6] | header[7] << 8;
    if (w < 1 || w > WIDTH || h < 1 || h > IMAGE_HEIGHT) {
        fclose(file);
        return -3;
    }
    unsigned char *row = malloc((size_t)w * 4);
    if (!row) {
        fclose(file);
        return -4;
    }
    int ox = (WIDTH - w) / 2, oy = (IMAGE_HEIGHT - h) / 2;
    for (int y = 0; y < h; ++y) {
        if (fread(row, 4, (size_t)w, file) != (size_t)w) {
            free(row);
            fclose(file);
            return -5;
        }
        for (int x = 0; x < w; ++x) {
            uint32_t back = screen[(oy + y) * WIDTH + ox + x];
            unsigned a = row[x * 4 + 3], result = 0xff000000u;
            for (unsigned component = 0; component < 3; ++component) {
                unsigned source = row[x * 4 + component];
                unsigned dest = (back >> (component * 8)) & 255;
                result |= ((source * a + dest * (255 - a) + 127) / 255) << (component * 8);
            }
            screen[(oy + y) * WIDTH + ox + x] = result;
        }
    }
    free(row);
    fclose(file);
    *width = w;
    *height = h;
    return 0;
}

int main(void) {
    SceUID block = sceKernelAllocMemBlock(
        "FnafProbeScreens", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 4 * 1024 * 1024, NULL);
    void *base = NULL;
    if (block < 0 || sceKernelGetMemBlockBase(block, &base) < 0) {
        if (block >= 0)
            sceKernelFreeMemBlock(block);
        sceKernelExitProcess(1);
        return 1;
    }
    int handles[MAX_IMAGES], count = 0;
    FILE *index = fopen("app0:/data/images.txt", "r");
    if (index) {
        while (count < MAX_IMAGES && fscanf(index, "%d", &handles[count]) == 1)
            ++count;
        fclose(index);
    }
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
    unsigned previous = 0;
    int selected = 0, checker = 1, dirty = 1, buffer = 0;
    SceDisplayFrameBuf frame = {.size = sizeof(frame),
                                .pitch = WIDTH,
                                .pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,
                                .width = WIDTH,
                                .height = HEIGHT};
    for (;;) {
        SceCtrlData pad = {0};
        sceCtrlPeekBufferPositive(0, &pad, 1);
        unsigned pressed = pad.buttons & ~previous;
        previous = pad.buttons;
        if (pressed & SCE_CTRL_START)
            break;
        int movement = 0;
        if (pressed & SCE_CTRL_RIGHT)
            movement = 1;
        if (pressed & SCE_CTRL_LEFT)
            movement = -1;
        if (pressed & SCE_CTRL_RTRIGGER)
            movement = 10;
        if (pressed & SCE_CTRL_LTRIGGER)
            movement = -10;
        if (movement && count) {
            selected = (selected + movement + count) % count;
            dirty = 1;
        }
        if (pressed & SCE_CTRL_CROSS) {
            checker = !checker;
            dirty = 1;
        }
        if (dirty) {
            screen = (uint32_t *)base + buffer * WIDTH * HEIGHT;
            background(checker);
            char label[100];
            int w = 0, h = 0, error = count ? draw_image(handles[selected], &w, &h) : -10;
            if (error)
                text(25, 220, "IMAGE READ ERROR", 0xff4444ff);
            text(16, 487, "FNAF1 APK RESOURCE TEST - GAMEPLAY NOT IMPLEMENTED", 0xff66ccff);
            snprintf(label, sizeof(label), "IMAGE %d/%d  HANDLE %d  %d X %d", selected + 1, count,
                     count ? handles[selected] : -1, w, h);
            text(16, 506, label, 0xffffffff);
            text(16, 525, "LEFT/RIGHT: IMAGE  L/R: 10  CROSS: BACKGROUND  START: EXIT", 0xffbbbbbb);
            frame.base = screen;
            if (sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME) < 0)
                break;
            buffer ^= 1;
            dirty = 0;
        }
        sceDisplayWaitVblankStart();
    }
    sceDisplaySetFrameBuf(NULL, SCE_DISPLAY_SETBUF_IMMEDIATE);
    sceDisplayWaitVblankStart();
    sceKernelFreeMemBlock(block);
    sceKernelExitProcess(0);
    return 0;
}
