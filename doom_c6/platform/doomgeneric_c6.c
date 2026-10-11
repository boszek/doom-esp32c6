// Platforma ESP32-C6: czas, mapowanie WAD-a z partycji flash, wyjscie na OLED (SH1106 128x64, I2C).
#include "doomgeneric.h"
#include "i_video.h"
#include "dg_mono.h"
#include "doomkeys.h"
#include "pad_xbox.h"

#include <stdint.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// sterownik wyswietlacza (oled_c6.cpp)
extern void oled_init(void);
extern void oled_flush(const uint8_t *pages);
extern void oled_text_screen(const char *const *lines, int n, int first_row);

static const char *TAG = "doom";

void DG_Init()
{
    oled_init();
}

void DG_DrawFrame()
{
    oled_flush(DG_MonoBuffer);
}

void DG_SleepMs(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1));
}

uint32_t DG_GetTicksMs()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// --- sterowanie padem ---------------------------------------------------------------------------
// Przyciski -> klawisze Dooma (zdarzenia z roznicy stanu); drazki -> analogowo przez DG_GetAnalog.
#define TRIGGER_ON 300

static unsigned padKeysDown(const pad_state_t *s)
{
    unsigned k = 0;
    if (s->rt > TRIGGER_ON || (s->buttons & PAD_BTN_B)) k |= 1u << 0;   // ogien
    if (s->buttons & (PAD_BTN_A | PAD_BTN_X))           k |= 1u << 1;   // uzyj / otworz
    if (s->buttons & PAD_BTN_LB)                        k |= 1u << 2;   // poprzednia bron
    if (s->buttons & PAD_BTN_RB)                        k |= 1u << 3;   // nastepna bron
    return k;
}

static const unsigned char padKeyCodes[4] = { KEY_FIRE, KEY_USE, ',', '.' };   // ',' i '.' = key_prevweapon / key_nextweapon

int DG_GetKey(int *pressed, unsigned char *key)
{
    static unsigned prev = 0;
    pad_state_t s;
    pad_read(&s);
    unsigned now = padKeysDown(&s), diff = now ^ prev;
    if (!diff) return 0;
    int i = __builtin_ctz(diff);
    *pressed = (now >> i) & 1;
    *key = padKeyCodes[i];
    prev ^= 1u << i;
    return 1;
}

static int stick(int v, int dead)   // martwa strefa, potem skalowanie do +-32768
{
    int a = v < 0 ? -v : v;
    if (a <= dead) return 0;
    a = (int) ((long) (a - dead) * 32768 / (32768 - dead));
    return v < 0 ? -a : a;
}

void DG_GetAnalog(int *fwd, int *side, int *turn)
{
    pad_state_t s;
    pad_read(&s);
    int ly = stick(s.ly, 6000), lx = stick(s.lx, 6000), rx = stick(s.rx, 5000);
    *fwd = -(ly * 50) / 32768;                    // pelne wychylenie = bieg (50)
    *side = (lx * 40) / 32768;
    long t = (long) rx * (rx < 0 ? -rx : rx) / 32768;   // krzywa kwadratowa: precyzja w srodku
    *turn = -(int) (t * 1100 / 32768);
}

void DG_EpisodeComplete(void)
{
    static const char *l[] = { "THE END", "", "Episode 1 done!", "", "Restarting..." };
    oled_text_screen(l, 5, 1);
    vTaskDelay(pdMS_TO_TICKS(6000));
}
void DG_SetWindowTitle(const char *title) {}

// WAD odwzorowany w pamieci przez MMU flasha (partycja danych "wad", typ 0x40)
const unsigned char *DG_MapWad(const char *path, unsigned int *length)
{
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t) 0x40, "wad");
    if (!p) { ESP_LOGE(TAG, "brak partycji wad"); return NULL; }
    const void *ptr = NULL;
    esp_partition_mmap_handle_t h;
    esp_err_t e = esp_partition_mmap(p, 0, p->size, ESP_PARTITION_MMAP_DATA, &ptr, &h);
    if (e != ESP_OK) { ESP_LOGE(TAG, "mmap nieudany: %d", e); return NULL; }
    const unsigned char *b = (const unsigned char *) ptr;
    if (memcmp(b, "IWAD", 4) != 0) { ESP_LOGE(TAG, "partycja wad nie zawiera IWAD"); return NULL; }
    int32_t numlumps, infoofs;
    memcpy(&numlumps, b + 4, 4); memcpy(&infoofs, b + 8, 4);
    *length = infoofs + numlumps * 16;
    ESP_LOGI(TAG, "WAD zmapowany: %u B, %d lumpow, %p", *length, (int) numlumps, ptr);
    return b;
}
