#ifndef FNAF_PIXEL_TEXT_H
#define FNAF_PIXEL_TEXT_H
#include <stdint.h>
#include <string.h>
static const char *pixel_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 :/-+.";
static const unsigned char pixel_glyphs[][5] = {
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
static inline void pixel_text(uint32_t *screen, int x, int y, const char *value, uint32_t color) {
    for (; *value; ++value, x += 12) {
        unsigned char ch = (unsigned char)*value;
        if (ch >= 'a' && ch <= 'z')
            ch -= 32;
        const char *match = strchr(pixel_alphabet, ch);
        if (!match)
            continue;
        const unsigned char *glyph = pixel_glyphs[match - pixel_alphabet];
        for (int col = 0; col < 5; ++col)
            for (int row = 0; row < 7; ++row)
                if (glyph[col] & (1u << row))
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dx = 0; dx < 2; ++dx) {
                            int px = x + col * 2 + dx, py = y + row * 2 + dy;
                            if (px >= 0 && px < 960 && py >= 0 && py < 544)
                                screen[py * 960 + px] = color;
                        }
    }
}
#endif
