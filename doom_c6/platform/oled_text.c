// Tekst na ekranie 128x64: czcionka 5x7, 6x8 px na znak => 21 kolumn x 8 wierszy.
#include <stdint.h>
#include <string.h>

extern const uint8_t oled_font5x7[95 * 5];
void oled_flush(const uint8_t *pages);

// Rysuje do 8 linii tekstu (NULL = pusta), wysrodkowanych w poziomie, i wysyla na wyswietlacz.
void oled_text_screen(const char *const *lines, int n, int first_row)
{
    uint8_t pages[8 * 128];
    memset(pages, 0, sizeof pages);
    for (int r = 0; r < n && first_row + r < 8; r++)
    {
        const char *s = lines[r];
        if (!s) continue;
        int len = (int) strlen(s);
        if (len > 21) len = 21;
        int x = (128 - len * 6) / 2;
        uint8_t *row = &pages[(first_row + r) * 128];
        for (int i = 0; i < len; i++, x += 6)
        {
            int c = (uint8_t) s[i];
            if (c < 32 || c > 126) c = '?';
            memcpy(&row[x], &oled_font5x7[(c - 32) * 5], 5);
        }
    }
    oled_flush(pages);
}
