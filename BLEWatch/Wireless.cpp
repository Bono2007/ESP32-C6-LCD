#include "Wireless.h"

bool WIFI_Connection = false;
int  WIFI_NUM        = 0;
int  BLE_NUM         = 0;
bool Scan_finish     = false;

/* WARNING: Running concurrent WiFi + BLE scans causes RAM allocation failure
   on ESP32-C6 in this configuration. Functions below are kept for reference
   but are NOT called from the main sketch. Use BLE scanning in blewatch.cpp. */

#if 0
int wifi_scan_number(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    int n = WiFi.scanNetworks();
    WiFi.scanDelete();
    return n;
}

int ble_scan_number(void)
{
    BLEDevice::init("ESP32");
    BLEScan *scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    BLEScanResults results = scan->start(5, false);
    int n = results.getCount();
    scan->clearResults();
    BLEDevice::deinit(true);
    return n;
}

void Wireless_Test1(void)
{
    WIFI_NUM = wifi_scan_number();
    BLE_NUM  = ble_scan_number();
    Scan_finish = true;
}

void Wireless_Test2(void)
{
    xTaskCreate([](void *) {
        WIFI_NUM = wifi_scan_number();
        BLE_NUM  = ble_scan_number();
        Scan_finish = true;
        vTaskDelete(NULL);
    }, "wireless_test", 4096, NULL, 1, NULL);
}
#endif
