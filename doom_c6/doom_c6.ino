// Demo Dooma (shareware) na XIAO ESP32-C6 + OLED SH1106 128x64.
// WAD jest w osobnej partycji flash "wad" (patrz partitions.csv i tools/flash_wad.sh).
#include <Arduino.h>
#include "esp_heap_caps.h"

// Doom potrzebuje duzego stosu
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

extern "C" {
    void doomgeneric_Create(int argc, char **argv);
    void doomgeneric_Tick(void);
    void oled_stats(uint32_t *avg_us, uint32_t *frames, uint32_t *errors);
}

static char zoneArg[16];

void setup()
{
    Serial.begin(115200);
    delay(1500);
    size_t freeNow = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    Serial.printf("DOOM: start, free heap=%u largest=%u\n", (unsigned) freeNow, (unsigned) largest);

    // strefa pamieci Dooma: tyle, ile sie da, z zapasem na reszte (lumpinfo, stosy itp.)
    int zoneKB = ((int) largest - 12 * 1024) / 1024;   // reszta sterty: lumpinfo (~14 KB) i drobiazgi
    if (zoneKB > 300) zoneKB = 300;
    snprintf(zoneArg, sizeof zoneArg, "%d", zoneKB);
    Serial.printf("DOOM: strefa %s KB\n", zoneArg);

static char *argv[] = { (char *) "doom", (char *) "-iwad", (char *) "doom1.wad", (char *) "-kb", zoneArg, NULL };
    doomgeneric_Create(5, argv);
    Serial.printf("DOOM: po inicjalizacji, free heap=%u largest=%u\n",
                  (unsigned) heap_caps_get_free_size(MALLOC_CAP_8BIT), (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void loop()
{
    static uint32_t t0 = millis();
    doomgeneric_Tick();
    if (millis() - t0 >= 2000) {
        uint32_t avg, fr, err;
        oled_stats(&avg, &fr, &err);
        Serial.printf("DOOM: %u kl/s, flush %u us, bledy I2C %u, free heap=%u\n",
                      (unsigned) (fr / 2), (unsigned) avg, (unsigned) err,
                      (unsigned) heap_caps_get_free_size(MALLOC_CAP_8BIT));
        t0 = millis();
    }
}
