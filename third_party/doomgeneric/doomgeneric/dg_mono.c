// Konwersja klatki 8-bit (indeksy palety) -> 1 bit:
//   jasnosc z palety (LUT) -> wyrownanie histogramu (wygladzane w czasie)
//   -> statyczny dithering Bayera 4x4 -> upakowanie w strony SH1106/SSD1306.
// Tylko arytmetyka calkowita (bez float/libm).
#include "i_video.h"
#include "dg_mono.h"

uint8_t DG_MonoBuffer[SCREENWIDTH * SCREENHEIGHT / 8];

#ifndef DG_MONO_EQ_PCT
#define DG_MONO_EQ_PCT 70   // ile % wyrownania histogramu (reszta: mapowanie liniowe)
#endif
#ifndef DG_MONO_SMOOTH
#define DG_MONO_SMOOTH 2    // wygladzanie mapy w czasie: 1/2^N na klatke
#endif

static uint8_t lum[256];       // jasnosc kazdego indeksu palety
static uint8_t lvl[256];       // jasnosc po auto-poziomach (budowana co klatke)
static int map16[256];         // mapa jasnosci w fixed-point 12.4 (stan wygladzania)
static int levels_init;

// progi Bayera 4x4 przeskalowane do 8..248
static const uint8_t bayer4[4][4] = {
    {   8, 136,  40, 168 },
    { 200,  72, 232, 104 },
    {  56, 184,  24, 152 },
    { 248, 120, 216,  88 },
};

void DG_MonoSetPalette(const uint8_t *rgb)
{
    for (int i = 0; i < 256; i++)
        lum[i] = (rgb[3 * i] * 77 + rgb[3 * i + 1] * 151 + rgb[3 * i + 2] * 28) >> 8;
    levels_init = 0;
}

static void build_levels(const uint8_t *indexed)
{
    int hist[256] = { 0 };
    const int n = SCREENWIDTH * SCREENHEIGHT;
    for (int i = 0; i < n; i++) hist[lum[indexed[i]]]++;

    // wyrownanie histogramu zmieszane z mapowaniem liniowym, wygladzone w czasie (bez migotania)
    const int recip = (255 << 16) / n;
    int acc = 0;
    for (int v = 0; v < 256; v++)
    {
        acc += hist[v];
        int eq = (acc * recip) >> 16;
        int target = (eq * DG_MONO_EQ_PCT + v * (100 - DG_MONO_EQ_PCT)) / 100;
        if (!levels_init) map16[v] = target << 4;
        else map16[v] += ((target << 4) - map16[v]) >> DG_MONO_SMOOTH;
        lvl[v] = map16[v] >> 4;
    }
    levels_init = 1;
}

void DG_MonoPack(const uint8_t *indexed)
{
    build_levels(indexed);
    // tablica jasnosci po poziomach dla kazdego indeksu (1 przeszukanie zamiast dwoch)
    uint8_t px[256];
    for (int i = 0; i < 256; i++) px[i] = lvl[lum[i]];

    for (int page = 0; page < DG_MONO_PAGES; page++)
    {
        uint8_t *dst = &DG_MonoBuffer[page * SCREENWIDTH];
        for (int x = 0; x < SCREENWIDTH; x++)
        {
            uint8_t bits = 0;
            const uint8_t *src = indexed + (page * 8) * SCREENWIDTH + x;
            for (int b = 0; b < 8; b++)
            {
                if (px[*src] > bayer4[(page * 8 + b) & 3][x & 3]) bits |= 1 << b;
                src += SCREENWIDTH;
            }
            dst[x] = bits;
        }
    }
}

// tylko do podgladu na PC: surowa jasnosc z palety (0..255), bez poziomow i ditheringu
void DG_MonoGray(const uint8_t *indexed, uint8_t *out)
{
    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++) out[i] = lum[indexed[i]];
}
