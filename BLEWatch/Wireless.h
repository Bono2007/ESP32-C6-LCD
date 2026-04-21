#pragma once

#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>

extern bool WIFI_Connection;
extern int  WIFI_NUM;
extern int  BLE_NUM;
extern bool Scan_finish;

int  wifi_scan_number(void);
int  ble_scan_number(void);
void Wireless_Test1(void);
void Wireless_Test2(void);
