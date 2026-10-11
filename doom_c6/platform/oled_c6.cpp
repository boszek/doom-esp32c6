// Sterownik SH1106 128x64 po I2C (Arduino Wire). SDA=GPIO22 (D4), SCL=GPIO23 (D5).
#include <Arduino.h>
#include <Wire.h>
#include "esp_timer.h"

#define OLED_SDA 22
#define OLED_SCL 23
#define COL_OFFSET 2   // SH1106: 2, SSD1306: 0
#ifndef OLED_I2C_HZ
#define OLED_I2C_HZ 1000000
#endif
#define CHUNK 64       // bajtow danych na transakcje (bufor Wire w ESP32 to 128 B)

static uint8_t oledAddr = 0x3C;

static uint32_t st_frames, st_errors;
static uint64_t st_us;

static void cmd(uint8_t c)
{
    Wire.beginTransmission(oledAddr);
    Wire.write(0x00);
    Wire.write(c);
    Wire.endTransmission();
}

extern "C" void oled_init(void)
{
    static bool done = false;
    if (done) return;
    done = true;
    Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_HZ);
    Wire.setBufferSize(256);
    static const uint8_t seq[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14,
        0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40,
        0xA4, 0xA6, 0xAF
    };
    for (uint8_t c : seq) cmd(c);
}

// pages: 8 stron po 128 bajtow (uklad stronicowy)
extern "C" void oled_flush(const uint8_t *pages)
{
    uint64_t t0 = esp_timer_get_time();
    for (int page = 0; page < 8; page++) {
        // trzy komendy (strona, kolumna dolna/gorna) w jednej transakcji
        Wire.beginTransmission(oledAddr);
        Wire.write((uint8_t) 0x00);
        Wire.write((uint8_t) (0xB0 + page));
        Wire.write((uint8_t) (0x00 | (COL_OFFSET & 0x0F)));
        Wire.write((uint8_t) (0x10 | (COL_OFFSET >> 4)));
        if (Wire.endTransmission() != 0) st_errors++;
        for (int i = 0; i < 128; i += CHUNK) {
            Wire.beginTransmission(oledAddr);
            Wire.write((uint8_t) 0x40);
            Wire.write(pages + page * 128 + i, CHUNK);
            if (Wire.endTransmission() != 0) st_errors++;
        }
    }
    st_us += esp_timer_get_time() - t0;
    st_frames++;
}

// statystyki od ostatniego wywolania: srednia dlugosc flush (us), liczba klatek i bledow
extern "C" void oled_stats(uint32_t *avg_us, uint32_t *frames, uint32_t *errors)
{
    *frames = st_frames;
    *errors = st_errors;
    *avg_us = st_frames ? (uint32_t) (st_us / st_frames) : 0;
    st_frames = 0; st_errors = 0; st_us = 0;
}
