// Platforma ESP32-C6: czas, mapowanie WAD-a z partycji flash, wyjscie na OLED (SH1106 128x64, I2C).
#include "doomgeneric.h"
#include "i_video.h"
#include "dg_mono.h"

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

int DG_GetKey(int *pressed, unsigned char *key) { return 0; }
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
