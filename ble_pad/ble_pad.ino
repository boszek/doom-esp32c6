// Etap A: klient BLE HID dla pada Xbox (Series X|S) na XIAO ESP32-C6.
// Czeka na pad w trybie parowania, paruje (bez zapisu bonda), wypisuje Report Map i raporty,
// mierzy wolna sterte przed/po BLE. Ekran OLED pokazuje stan po angielsku.
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include "esp_heap_caps.h"

extern "C" {
    void oled_init(void);
    void oled_text_screen(const char *const *lines, int n, int first_row);
}

static BLEUUID hidServiceUUID((uint16_t) 0x1812);
static BLEUUID reportCharUUID((uint16_t) 0x2A4D);
static BLEUUID reportMapUUID((uint16_t) 0x2A4B);

static volatile bool doConnect = false, connected = false;
static BLEAdvertisedDevice *myDevice = nullptr;
static BLEClient *pClient = nullptr;
static volatile uint32_t nReports = 0;
static uint8_t lastReport[32];
static volatile size_t lastLen = 0;
static volatile uint8_t lastId = 0;

static void heapInfo(const char *tag)
{
    Serial.printf("HEAP %s: free=%u largest=%u min=%u\n", tag,
                  (unsigned) heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
}

static void scanDone(BLEScanResults) {}
static void startScan() { BLEDevice::getScan()->clearResults(); BLEDevice::getScan()->start(5, scanDone, false); }

static void showWait()
{
    static const char *l[] = { "Turn on your Xbox", "controller and hold", "the Sync button", "until the logo", "flashes fast..." };
    oled_text_screen(l, 5, 1);
}

static void showConnected()
{
    static const char *l[] = { "Controller", "connected!" };
    oled_text_screen(l, 2, 3);
}

static void notifyCb(BLERemoteCharacteristic *c, uint8_t *d, size_t len, bool isNotify)
{
    lastLen = len < sizeof lastReport ? len : sizeof lastReport;
    memcpy(lastReport, d, lastLen);
    nReports++;
}

class SecCb : public BLESecurityCallbacks {
    void onAuthenticationComplete(ble_gap_conn_desc *d) {
        Serial.printf("SEC: auth complete encrypted=%d authenticated=%d bonded=%d keysize=%d\n",
                      d->sec_state.encrypted, d->sec_state.authenticated, d->sec_state.bonded, d->sec_state.key_size);
    }
};

class ClientCb : public BLEClientCallbacks {
    void onConnect(BLEClient *) { Serial.println("CB: connected"); }
    void onDisconnect(BLEClient *) { connected = false; Serial.println("CB: disconnected"); }
};

static bool connectToServer()
{
    Serial.printf("Connecting to %s\n", myDevice->getAddress().toString().c_str());
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCb());
    if (!pClient->connect(myDevice)) { Serial.println("connect failed"); return false; }
    heapInfo("po connect");
    ble_gap_conn_desc cd;
    bool enc = ble_gap_conn_find(pClient->getConnId(), &cd) == 0 && cd.sec_state.encrypted;
    Serial.printf("link encrypted already: %d\n", enc);
    if (!enc) {
        bool sc = pClient->secureConnection();
        Serial.printf("secureConnection() -> %d\n", sc);
    }

    Serial.println("step: getService");
    BLERemoteService *svc = pClient->getService(hidServiceUUID);
    if (!svc) { Serial.println("no HID service"); pClient->disconnect(); return false; }

    BLERemoteCharacteristic *map = svc->getCharacteristic(reportMapUUID);
    if (map) {
        String m = map->readValue();
        Serial.printf("REPORT MAP (%u bytes):\n", (unsigned) m.length());
        for (size_t i = 0; i < m.length(); i++) Serial.printf("%02X%s", (uint8_t) m[i], (i % 16 == 15) ? "\n" : " ");
        Serial.println();
    }

    int subs = 0;
    for (auto const &e : *svc->getCharacteristics()) {
        BLERemoteCharacteristic *ch = e.second;
        if (!ch->getUUID().equals(reportCharUUID) || !ch->canNotify()) continue;
        BLERemoteDescriptor *ref = ch->getDescriptor(BLEUUID((uint16_t) 0x2908));
        int id = -1, type = -1;
        if (ref) { String v = ref->readValue(); if (v.length() >= 2) { id = (uint8_t) v[0]; type = (uint8_t) v[1]; } }
        Serial.printf(" report char handle 0x%04X id=%d type=%d\n", ch->getHandle(), id, type);
        if (type == 1 || type == -1) { ch->registerForNotify(notifyCb); subs++; }
    }
    Serial.printf("subscribed to %d input reports\n", subs);
    connected = subs > 0;
    return connected;
}

class AdvCb : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice d) {
        if (doConnect) return;
        if (d.haveServiceUUID() && d.isAdvertisingService(hidServiceUUID)) {
            Serial.printf("HID device: %s appearance=0x%04X name=%s\n", d.getAddress().toString().c_str(),
                          d.haveAppearance() ? d.getAppearance() : 0, d.haveName() ? d.getName().c_str() : "?");
            if (d.haveAppearance() && d.getAppearance() != 0x03C4) return;   // tylko gamepady
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(d);
            doConnect = true;
        }
    }
};

void setup()
{
    Serial.begin(115200);
    delay(1500);
    oled_init();
    showWait();
    heapInfo("start");
    BLEDevice::init("DoomPad");
    heapInfo("po BLEDevice::init");

    BLESecurity *sec = new BLESecurity();
    sec->setCapability(ESP_IO_CAP_NONE);
    sec->setAuthenticationMode(true, false, true);   // bonding, bez MITM, Secure Connections
    BLEDevice::setSecurityCallbacks(new SecCb());

    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new AdvCb());
    scan->setInterval(100);
    scan->setWindow(99);
    scan->setActiveScan(true);
    startScan();
    heapInfo("skanowanie");
}

void loop()
{
    static uint32_t tPrint = 0, lastN = 0;
    if (doConnect) {
        doConnect = false;
        if (connectToServer()) { showConnected(); heapInfo("po polaczeniu"); }
        else { showWait(); startScan(); }
    }
    if (!connected && !doConnect && !BLEDevice::getScan()->isScanning()) {
        startScan();
    }
    if (connected && millis() - tPrint > 250) {
        tPrint = millis();
        if (nReports != lastN) {
            lastN = nReports;
            Serial.printf("RPT n=%u len=%u:", (unsigned) lastN, (unsigned) lastLen);
            for (size_t i = 0; i < lastLen; i++) Serial.printf(" %02X", lastReport[i]);
            Serial.println();
        }
    }
    static uint32_t tHeap = 0;
    if (millis() - tHeap > 5000) { tHeap = millis(); heapInfo(connected ? "loop(connected)" : "loop(waiting)"); }
    delay(20);
}
