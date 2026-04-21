# ESP32-C6 LCD 1.47" — Collection d'applications

Projets embarqués pour cartes **Spotpear / Waveshare ESP32-C6 LCD 1.47"** équipées d'un écran ST7789 (172×320 px, RGB565) et d'une LED RGB WS2812 intégrée.

| Broche | Signal |
|--------|--------|
| GPIO 6 | MOSI   |
| GPIO 7 | SCLK   |
| GPIO 14| CS     |
| GPIO 15| DC     |
| GPIO 21| RST    |
| GPIO 22| BL     |
| GPIO 8 | WS2812 |

---

## Projets

### [Proxmox Dashboard](./Proxmox%20Dashboard/) — ESP-IDF
Dashboard temps réel affichant CPU, RAM, réseau et uptime d'un serveur Proxmox via son API REST HTTPS.  
**Framework :** ESP-IDF v5.4.1 · **Librairie :** LVGL 8.x (composant géré)

### [BLEWatch](./BLEWatch/) — Arduino
Scanner Bluetooth Low Energy qui détecte les appareils à proximité, affiche leur force de signal (RSSI) et effectue une vérification de vulnérabilité par OUI constructeur.  
**Framework :** Arduino IDE · **Librairies :** LVGL 9.x, NimBLE-Arduino, Adafruit NeoPixel

### [BandWatch](./BandWatch/) — Arduino
Analyseur de spectre Wi-Fi 2.4 GHz en mode promiscuous — hopping sur les 13 canaux, score d'activité logarithmique, top 3 des canaux les plus chargés.  
**Framework :** Arduino IDE · **Librairies :** LVGL 9.x, Adafruit NeoPixel

---

## Prérequis communs (Arduino)

### 1. Carte ESP32-C6 dans Arduino IDE

Dans **Fichier → Préférences → URL gestionnaire de cartes**, ajouter :
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Puis **Outils → Type de carte → Gestionnaire de cartes** : installer **esp32 by Espressif Systems**.

Sélectionner la carte : **ESP32C6 Dev Module**.

### 2. Librairies à installer (Gestionnaire de librairies)

| Librairie | Version |
|-----------|---------|
| **lvgl** | 9.x |
| **Adafruit NeoPixel** | dernière |
| **NimBLE-Arduino** | dernière (pour BLEWatch) |

### 3. Paramètres de flash recommandés

| Paramètre | Valeur |
|-----------|--------|
| Flash Size | 4MB |
| Partition Scheme | Default 4MB with spiffs |
| Upload Speed | 921600 |

---

## Structure du repo

```
.
├── README.md                  ← ce fichier
├── Proxmox Dashboard/         ← projet ESP-IDF
│   ├── main/
│   ├── CMakeLists.txt
│   └── ...
├── BLEWatch/                  ← sketch Arduino
│   ├── blewatch.ino
│   ├── blewatch.cpp / .h
│   ├── Display_ST7789.cpp / .h
│   ├── LVGL_Driver.cpp / .h
│   ├── Wireless.cpp / .h
│   └── lv_conf.h
└── BandWatch/                 ← sketch Arduino
    ├── bandwatch.ino
    ├── bandwatch.cpp / .h
    ├── Display_ST7789.cpp / .h
    ├── LVGL_Driver.cpp / .h
    └── lv_conf.h
```

---

## Référence

- Projet d'origine BLEWatch / BandWatch : [PierreGode/WaveshareESP32C6LCD](https://github.com/PierreGode/WaveshareESP32C6LCD)
- Documentation ESP-IDF : [docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/latest/)
- LVGL : [lvgl.io](https://lvgl.io)
