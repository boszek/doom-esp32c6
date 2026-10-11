// Doom (shareware, epizod 1) na XIAO ESP32-C6 + OLED SH1106 128x64, sterowany padem Xbox po BLE.
// WAD jest w osobnej partycji flash "wad" (patrz partitions.csv i tools/flash_wad.sh).
#include <Arduino.h>
#include "esp_heap_caps.h"

// Doom potrzebuje duzego stosu
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

extern "C" {
    void doomgeneric_Create(int argc, char **argv);
    void doomgeneric_Tick(void);
    void oled_init(void);
    void oled_stats(uint32_t *avg_us, uint32_t *frames, uint32_t *errors);
    void oled_text_screen(const char *const *lines, int n, int first_row);
}
#include "src/engine/pad_xbox.h"

static char zoneArg[16];

static void heapLine(const char *tag)
{
    Serial.printf("DOOM: %s free heap=%u largest=%u\n", tag,
                  (unsigned) heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    oled_init();
    heapLine("start");

    // czekamy na pad w trybie parowania
    static const char *wait[] = { "Turn on your Xbox", "controller and hold", "the Sync button", "until the logo", "flashes fast..." };
    oled_text_screen(wait, 5, 1);
    pad_begin();
    heapLine("po BLE");
    while (!pad_connected()) { pad_poll(); delay(20); }
    static const char *ok[] = { "Controller", "connected!" };
    oled_text_screen(ok, 2, 3);
    delay(1200);

    // strefa pamieci Dooma: najwiekszy wolny blok z zapasem na reszte (lumpinfo juz we flashu)
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    int zoneKB = ((int) largest - 14 * 1024) / 1024;
    if (zoneKB > 300) zoneKB = 300;
    snprintf(zoneArg, sizeof zoneArg, "%d", zoneKB);
    heapLine("przed Doomem");
    Serial.printf("DOOM: strefa %s KB\n", zoneArg);

    static char *argv[] = { (char *) "doom", (char *) "-iwad", (char *) "doom1.wad", (char *) "-kb", zoneArg,
                            (char *) "-warp", (char *) "1", (char *) "1", (char *) "-skill", (char *) "2", NULL };
    doomgeneric_Create(10, argv);
    heapLine("po inicjalizacji");
}

void loop()
{
    static uint32_t t0 = millis();
    pad_poll();
    doomgeneric_Tick();
    if (millis() - t0 >= 2000) {
        uint32_t avg, fr, err;
        oled_stats(&avg, &fr, &err);
        Serial.printf("DOOM: %u kl/s, flush %u us, bledy I2C %u, pad=%d, free heap=%u min=%u\n",
                      (unsigned) (fr / 2), (unsigned) avg, (unsigned) err, pad_connected(),
                      (unsigned) heap_caps_get_free_size(MALLOC_CAP_8BIT),
                      (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
        t0 = millis();
    }
}
