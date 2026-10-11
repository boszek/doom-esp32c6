// Konwersja klatki 8-bit (indeksy palety) -> 1 bit, upakowana w strony SH1106/SSD1306.
#ifndef DG_MONO_H
#define DG_MONO_H

#include <stdint.h>

#define DG_MONO_PAGES (SCREENHEIGHT / 8)

// bufor wyjsciowy: SCREENWIDTH * SCREENHEIGHT / 8 bajtow, uklad stronicowy (bit0 = gorny piksel strony)
extern uint8_t DG_MonoBuffer[];

// palette: 256 * RGB (po korekcji gamma silnika)
void DG_MonoSetPalette(const uint8_t *rgb);
void DG_MonoPack(const uint8_t *indexed);

#endif
