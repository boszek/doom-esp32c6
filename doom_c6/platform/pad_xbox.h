// Pad Xbox (Series X|S) po BLE: klient HID, parowanie i odczyt raportow.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t lx, ly, rx, ry;   // drazki, 0 = srodek (zakres okolo +-32768); ly/ry > 0 = w dol
    uint16_t lt, rt;          // triggery 0..1023
    uint16_t buttons;         // bity jak w raporcie: 0 A, 1 B, 3 X, 4 Y, 6 LB, 7 RB, 10 View, 11 Menu
    uint8_t hat;              // 0 = brak, 1 = gora, kolejno zgodnie z ruchem wskazowek zegara
} pad_state_t;

#define PAD_BTN_A   (1u << 0)
#define PAD_BTN_B   (1u << 1)
#define PAD_BTN_X   (1u << 3)
#define PAD_BTN_Y   (1u << 4)
#define PAD_BTN_LB  (1u << 6)
#define PAD_BTN_RB  (1u << 7)
#define PAD_BTN_VIEW (1u << 10)
#define PAD_BTN_MENU (1u << 11)

void pad_begin(void);                 // uruchamia BLE i skanowanie
void pad_poll(void);                  // wolac regularnie: laczy sie z padem, wznawia skan po utracie
int  pad_connected(void);
void pad_read(pad_state_t *s);        // ostatni znany stan (po utracie pada: zera)

#ifdef __cplusplus
}
#endif
