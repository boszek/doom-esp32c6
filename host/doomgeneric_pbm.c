// Backend hosta (PC): zamiast okna zapisuje wybrane klatki jako PPM.
// Zmienne srodowiskowe: DG_DUMP_DIR (katalog, domyslnie ./frames), DG_DUMP_EVERY (co ile klatek, 0 = wylacz)
#include "doomgeneric.h"
#ifdef DG_MONO
#include "i_video.h"
#include "dg_mono.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

static const char *dumpDir = "frames";
static int dumpEvery = 0;
static unsigned frameNo = 0;
static struct timespec t0;

void DG_Init()
{
    const char *d = getenv("DG_DUMP_DIR");
    const char *e = getenv("DG_DUMP_EVERY");
    if (d) dumpDir = d;
    if (e) dumpEvery = atoi(e);
    if (dumpEvery > 0) mkdir(dumpDir, 0755);
    clock_gettime(CLOCK_MONOTONIC, &t0);
}

void DG_DrawFrame()
{
    frameNo++;
    if (dumpEvery <= 0 || frameNo % dumpEvery) return;
    char name[512];
    snprintf(name, sizeof name, "%s/f%06u.ppm", dumpDir, frameNo);
    FILE *f = fopen(name, "wb");
    if (!f) return;
#ifdef DG_MONO
    // wyswietlacz 1-bit: skalujemy x4, zeby latwo bylo ogladac
    const int S = 4;
    fprintf(f, "P6\n%d %d\n255\n", SCREENWIDTH * S, SCREENHEIGHT * S);
    for (int y = 0; y < SCREENHEIGHT * S; y++)
        for (int x = 0; x < SCREENWIDTH * S; x++) {
            int sx = x / S, sy = y / S;
            int on = (DG_MonoBuffer[(sy >> 3) * SCREENWIDTH + sx] >> (sy & 7)) & 1;
            unsigned char v = on ? 255 : 0;
            fputc(v, f); fputc(v, f); fputc(v, f);
        }
#else
    fprintf(f, "P6\n%d %d\n255\n", DOOMGENERIC_RESX, DOOMGENERIC_RESY);
    for (int i = 0; i < DOOMGENERIC_RESX * DOOMGENERIC_RESY; i++) {
        uint32_t p = DG_ScreenBuffer[i];
        fputc((p >> 16) & 255, f); fputc((p >> 8) & 255, f); fputc(p & 255, f);
    }
#endif
    fclose(f);
#ifdef DG_MONO
    if (getenv("DG_DUMP_GRAY")) {
        extern byte *I_VideoBuffer;
        extern void DG_MonoGray(const uint8_t *, uint8_t *);
        static uint8_t g[SCREENWIDTH * SCREENHEIGHT];
        DG_MonoGray(I_VideoBuffer, g);
        snprintf(name, sizeof name, "%s/g%06u.pgm", dumpDir, frameNo);
        FILE *fg = fopen(name, "wb");
        if (fg) { fprintf(fg, "P5\n%d %d\n255\n", SCREENWIDTH, SCREENHEIGHT); fwrite(g, 1, sizeof g, fg); fclose(fg); }
    }
#endif
}

// DOOM_FAST=1: wirtualny zegar zamiast prawdziwego czasu (sen tylko przesuwa zegar), gra biegnie z maksymalna predkoscia
static int fastClock = -1;
static uint32_t virtualMs;

void DG_SleepMs(uint32_t ms)
{
    if (fastClock < 0) fastClock = getenv("DOOM_FAST") != NULL;
    if (fastClock) { virtualMs += ms; return; }
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

uint32_t DG_GetTicksMs()
{
    if (fastClock < 0) fastClock = getenv("DOOM_FAST") != NULL;
    if (fastClock) return virtualMs;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((t.tv_sec - t0.tv_sec) * 1000 + (t.tv_nsec - t0.tv_nsec) / 1000000);
}

// Prosty bot do testow bez pada: DOOM_BOT=<ziarno> -> losowo chodzi, skreca, strzela i uzywa.
#include "doomkeys.h"
extern int gametic;
static unsigned botRng;
static unsigned botNext(void) { botRng = botRng * 1103515245u + 12345u; return (botRng >> 16) & 0x7fff; }

int DG_GetKey(int *pressed, unsigned char *key)
{
    static int enabled = -1, lastTic = -1, qn = 0, qi = 0;
    static int held[6];                        // fwd, left, right, fire, use, strafe
    static int turnDir = 1, nextTurn = 0;
    static struct { int p; unsigned char k; } q[16];
    static const unsigned char codes[6] = { KEY_UPARROW, KEY_LEFTARROW, KEY_RIGHTARROW, KEY_FIRE, KEY_USE, KEY_STRAFE_R };
    if (enabled < 0) { const char *e = getenv("DOOM_BOT"); enabled = e != NULL; botRng = e ? atoi(e) : 0; }
    if (!enabled) return 0;
    if (gametic != lastTic)
    {
        lastTic = gametic;
        int want[6] = { 1, 0, 0, 0, 0, 0 };
        if (gametic >= nextTurn) { turnDir = (botNext() & 1) ? 1 : -1; nextTurn = gametic + 5 + botNext() % 40; }
        if (botNext() % 100 < 35) { if (turnDir > 0) want[2] = 1; else want[1] = 1; }
        want[3] = (gametic / 20) & 1;          // ogien na przemian
        want[4] = (gametic % 25) < 3;          // uzycie (drzwi/przelaczniki)
        want[5] = (botNext() % 100) < 10;
        if (botNext() % 100 < 4) want[0] = 0;
        qn = qi = 0;
        for (int i = 0; i < 6; i++)
            if (want[i] != held[i]) { held[i] = want[i]; q[qn].p = want[i]; q[qn].k = codes[i]; qn++; }
    }
    if (qi < qn) { *pressed = q[qi].p; *key = q[qi].k; qi++; return 1; }
    return 0;
}
void DG_SetWindowTitle(const char *title) {}

#ifdef DG_MONO
void DG_GetAnalog(int *fwd, int *side, int *turn) { *fwd = *side = *turn = 0; }
void DG_EpisodeComplete(void) { fprintf(stderr, "EPISODE COMPLETE\n"); }
#endif

// WAD mapowany w pamieci (tak jak na ESP32 przez esp_partition_mmap)
const unsigned char *DG_MapWad(const char *path, unsigned int *length)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;
    off_t n = lseek(fd, 0, SEEK_END);
    void *p = mmap(NULL, n, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return NULL;
    *length = (unsigned) n;
    return p;
}

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);
    for (;;) doomgeneric_Tick();
    return 0;
}
