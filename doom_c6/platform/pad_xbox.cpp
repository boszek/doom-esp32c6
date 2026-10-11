// Klient BLE HID dla pada Xbox (Series X|S, model 1914) na NimBLE z rdzenia Arduino.
// Parowanie "Just Works" z zapisem bonda w NVS: pierwszy raz przytrzymaj Sync, potem wystarczy przycisk Xbox.
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include "pad_xbox.h"

static BLEUUID hidServiceUUID((uint16_t) 0x1812);
static BLEUUID reportCharUUID((uint16_t) 0x2A4D);

static volatile bool doConnect = false;
static volatile bool linkUp = false;
static BLEAdvertisedDevice *myDevice = nullptr;
static BLEClient *pClient = nullptr;

static portMUX_TYPE padMux = portMUX_INITIALIZER_UNLOCKED;
static pad_state_t cur;

static void clearState()
{
    portENTER_CRITICAL(&padMux);
    memset(&cur, 0, sizeof cur);
    portEXIT_CRITICAL(&padMux);
}

// Raport wejsciowy (16 B): LX LY RX RY (uint16 LE), LT, RT (10 bit), hat (4 bit), 15 przyciskow, Share
static void notifyCb(BLERemoteCharacteristic *, uint8_t *d, size_t len, bool)
{
    if (len < 15) return;
    pad_state_t s;
    s.lx = (int16_t) ((int) (d[0] | d[1] << 8) - 32768);
    s.ly = (int16_t) ((int) (d[2] | d[3] << 8) - 32768);
    s.rx = (int16_t) ((int) (d[4] | d[5] << 8) - 32768);
    s.ry = (int16_t) ((int) (d[6] | d[7] << 8) - 32768);
    s.lt = (d[8] | d[9] << 8) & 0x3FF;
    s.rt = (d[10] | d[11] << 8) & 0x3FF;
    s.hat = d[12] & 0x0F;
    s.buttons = (d[13] | d[14] << 8) & 0x7FFF;
    portENTER_CRITICAL(&padMux);
    cur = s;
    portEXIT_CRITICAL(&padMux);
}

class ClientCb : public BLEClientCallbacks {
    void onConnect(BLEClient *) {}
    void onDisconnect(BLEClient *) { linkUp = false; clearState(); }
};

class QuietSec : public BLESecurityCallbacks {
    void onAuthenticationComplete(ble_gap_conn_desc *) {}
};

class AdvCb : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice d) {
        if (doConnect || linkUp) return;
        if (!d.haveServiceUUID() || !d.isAdvertisingService(hidServiceUUID)) return;
        if (d.haveAppearance() && d.getAppearance() != 0x03C4) return;   // tylko gamepady
        BLEDevice::getScan()->stop();
        delete myDevice;
        myDevice = new BLEAdvertisedDevice(d);
        doConnect = true;
    }
};

static void scanDone(BLEScanResults) {}

static void startScan()
{
    BLEDevice::getScan()->clearResults();
    BLEDevice::getScan()->start(5, scanDone, false);
}

static bool connectToPad()
{
    if (!pClient) {
        pClient = BLEDevice::createClient();
        pClient->setClientCallbacks(new ClientCb());
    }
    if (!pClient->connect(myDevice)) return false;

    // po bondzie lacze jest juz zaszyfrowane; secureConnection() na zaszyfrowanym lączu wisi w nieskonczonosc
    ble_gap_conn_desc cd;
    bool enc = ble_gap_conn_find(pClient->getConnId(), &cd) == 0 && cd.sec_state.encrypted;
    if (!enc && !pClient->secureConnection()) { pClient->disconnect(); return false; }

    BLERemoteService *svc = pClient->getService(hidServiceUUID);
    if (!svc) { pClient->disconnect(); return false; }

    int subs = 0;
    for (auto const &e : *svc->getCharacteristics()) {
        BLERemoteCharacteristic *ch = e.second;
        if (!ch->getUUID().equals(reportCharUUID) || !ch->canNotify()) continue;
        BLERemoteDescriptor *ref = ch->getDescriptor(BLEUUID((uint16_t) 0x2908));
        int type = -1;
        if (ref) { String v = ref->readValue(); if (v.length() >= 2) type = (uint8_t) v[1]; }
        if (type == 1 || type == -1) { ch->registerForNotify(notifyCb); subs++; }
    }
    if (!subs) { pClient->disconnect(); return false; }
    linkUp = true;
    return true;
}

extern "C" void pad_begin(void)
{
    BLEDevice::init("DoomPad");
    BLESecurity *sec = new BLESecurity();
    sec->setCapability(ESP_IO_CAP_NONE);
    sec->setAuthenticationMode(true, false, true);   // bonding, bez MITM, Secure Connections
    BLEDevice::setSecurityCallbacks(new QuietSec());

    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new AdvCb());
    scan->setInterval(100);
    scan->setWindow(99);
    scan->setActiveScan(true);
    startScan();
}

extern "C" void pad_poll(void)
{
    if (doConnect) {
        doConnect = false;
        if (!connectToPad()) startScan();
    } else if (!linkUp && !BLEDevice::getScan()->isScanning()) {
        startScan();
    }
}

extern "C" int pad_connected(void) { return linkUp; }

extern "C" void pad_read(pad_state_t *s)
{
    portENTER_CRITICAL(&padMux);
    *s = cur;
    portEXIT_CRITICAL(&padMux);
}
