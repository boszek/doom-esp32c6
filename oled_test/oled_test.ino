// Test wyświetlacza SSD1306 128x64 na XIAO ESP32-C6 (bez zewnętrznych bibliotek)
// SDA = D4 (GPIO22), SCL = D5 (GPIO23)

#include <Wire.h>

#define OLED_SDA 22
#define OLED_SCL 23
#define I2C_FREQ 400000
#define COL_OFFSET 2  // SH1106: 2, prawdziwy SSD1306: 0

static const int W = 128, H = 64;
static uint8_t fb[W * H / 8];  // bufor klatki, układ stronicowy SSD1306
static uint8_t oledAddr = 0;
static uint32_t i2cErrors = 0;

static void cmd(uint8_t c) {
  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(c);
  Wire.endTransmission();
}

static uint8_t scan() {
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("I2C: urzadzenie pod 0x%02X\n", a);
      if (!found && (a == 0x3C || a == 0x3D)) found = a;
    }
  }
  return found;
}

static void oledInit() {
  static const uint8_t seq[] = {
    0xAE,        // display off
    0xD5, 0x80,  // zegar
    0xA8, 0x3F,  // multiplex 64
    0xD3, 0x00,  // offset
    0x40,        // start line
    0x8D, 0x14,  // charge pump on
    0xA1,        // segment remap
    0xC8,        // com scan odwrocony
    0xDA, 0x12,  // com pins
    0x81, 0xCF,  // kontrast
    0xD9, 0xF1,  // precharge
    0xDB, 0x40,  // vcomh
    0xA4,        // z RAM
    0xA6,        // normalny (nie inverted)
    0xAF         // display on
  };
  for (uint8_t c : seq) cmd(c);
}

static void flush() {
  // tryb stronicowy - dziala na SH1106 i SSD1306
  for (int page = 0; page < 8; page++) {
    cmd(0xB0 + page);
    cmd(0x00 | (COL_OFFSET & 0x0F));         // kolumna - dolny polbajt
    cmd(0x10 | (COL_OFFSET >> 4));           // kolumna - gorny polbajt
    const int CH = 16;  // bufor Wire w ESP32 to 128 B
    for (int i = 0; i < W; i += CH) {
      Wire.beginTransmission(oledAddr);
      Wire.write(0x40);
      Wire.write(fb + page * W + i, CH);
      if (Wire.endTransmission() != 0) i2cErrors++;
    }
  }
}

static inline void px(int x, int y, bool on = true) {
  if ((unsigned)x >= W || (unsigned)y >= H) return;
  if (on) fb[x + (y >> 3) * W] |= 1 << (y & 7);
  else    fb[x + (y >> 3) * W] &= ~(1 << (y & 7));
}

static void line(int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    px(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\nTest OLED SSD1306 na XIAO ESP32-C6");

  Wire.begin(OLED_SDA, OLED_SCL, I2C_FREQ);
  oledAddr = scan();
  if (!oledAddr) {
    Serial.println("Nie znaleziono wyswietlacza pod 0x3C/0x3D - sprawdz kable SDA=D4, SCL=D5, zasilanie 3V3");
    return;
  }
  Serial.printf("Wyswietlacz: 0x%02X\n", oledAddr);
  oledInit();

  // 1) wszystkie piksele zapalone - 1 s
  memset(fb, 0xFF, sizeof(fb)); flush(); delay(1000);
  // 2) szachownica - 1 s
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) px(x, y, ((x / 8) + (y / 8)) & 1);
  flush(); delay(1000);
}

void loop() {
  if (!oledAddr) { delay(1000); return; }

  static int t = 0;
  static uint32_t last = millis(), frames = 0;

  memset(fb, 0, sizeof(fb));
  // ramka
  for (int x = 0; x < W; x++) { px(x, 0); px(x, H - 1); }
  for (int y = 0; y < H; y++) { px(0, y); px(W - 1, y); }
  // przekatne
  line(0, 0, W - 1, H - 1);
  line(0, H - 1, W - 1, 0);
  // skaczacy kwadrat
  int bx = 4 + abs((t * 2) % (2 * (W - 16)) - (W - 16));
  int by = 4 + abs(t % (2 * (H - 16)) - (H - 16));
  for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) px(bx + x, by + y);

  flush();
  t++;
  frames++;
  if (millis() - last >= 1000) {
    Serial.printf("%u fps (I2C %d Hz), bledy I2C: %u\n", (unsigned)frames, I2C_FREQ, (unsigned)i2cErrors);
    frames = 0;
    last = millis();
  }
}
