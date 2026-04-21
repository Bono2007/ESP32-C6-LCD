# BLEWatch

Scanner Bluetooth Low Energy pour **Spotpear / Waveshare ESP32-C6 LCD 1.47"**.  
Affiche en temps réel la liste des appareils BLE détectés à proximité, triés par force de signal.

---

## Matériel requis

| Composant | Référence |
|-----------|-----------|
| Microcontrôleur | Spotpear ESP32-C6 LCD 1.47" (ST7789V3, 172×320) |
| Connexion | USB-C (flash + alimentation) |

Pas de câblage supplémentaire — l'écran et la LED RGB sont intégrés au PCB.

### Brochage interne (référence)

| Signal | GPIO |
|--------|------|
| SPI MOSI | 6 |
| SPI SCLK | 7 |
| LCD CS | 14 |
| LCD DC | 15 |
| LCD RST | 21 |
| Backlight | 22 |
| WS2812 LED | 8 |

---

## Prérequis logiciels

### 1. Arduino IDE 2.x

Télécharger depuis [arduino.cc](https://www.arduino.cc/en/software).

### 2. Support ESP32-C6

Dans **Fichier → Préférences → URL gestionnaire de cartes**, ajouter :

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

**Outils → Type de carte → Gestionnaire de cartes** → rechercher **esp32** → installer **esp32 by Espressif Systems** (version 3.x).

### 3. Librairies (Outils → Gérer les librairies)

| Librairie | Version testée | Notes |
|-----------|---------------|-------|
| **lvgl** | 9.5.0 | Chercher "lvgl" |
| **Adafruit NeoPixel** | 1.15.x | Chercher "NeoPixel" |

> **Important :** NE PAS installer la librairie externe "NimBLE-Arduino". Le core ESP32 3.x inclut la pile BLE Bluedroid qui est utilisée ici ; une deuxième lib NimBLE crée des symboles dupliqués au link.

---

## Configuration Arduino IDE

Sélectionner la carte : **Outils → Type de carte → esp32 → ESP32C6 Dev Module**

Paramètres obligatoires :

| Paramètre | Valeur |
|-----------|--------|
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| Upload Speed | 921600 |
| Flash Size | 4MB (ou 8MB selon ta carte) |

> Le schéma **Huge APP** est indispensable : BLE + LVGL 9.x dépassent 1.6 MB de code, ce qui excède le schéma par défaut (1.2 MB).

---

## Flasher le projet

1. Cloner le repo ou télécharger le dossier `BLEWatch/`
2. Ouvrir `BLEWatch/BLEWatch.ino` dans Arduino IDE
3. Brancher la carte en USB-C
4. Sélectionner le port : **Outils → Port → /dev/cu.usbmodem…** (macOS) ou `COM…` (Windows)
5. **Croquis → Téléverser** (`Ctrl+U`)

### Via arduino-cli (terminal)

```bash
# Installation une seule fois
brew install arduino-cli
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "lvgl@9.5.0" "Adafruit NeoPixel"

# Compiler + flasher
cd BLEWatch
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" BLEWatch.ino
arduino-cli upload  --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" \
  --port /dev/cu.usbmodem1101 BLEWatch.ino
```

---

## Fonctionnement

### Affichage

L'écran montre jusqu'à **8 appareils BLE** triés du plus proche au plus loin :

```
BLEWatch  [21]
────────────────
Apple      -38dB
████████████░░░░
Samsung    -52dB
████████░░░░░░░░
Garmin     -63dB
██████░░░░░░░░░░
OUI A4:CF  -71dB
████░░░░░░░░░░░░
...
```

Chaque ligne affiche :
- **Nom** : nom diffusé par l'appareil, ou marque déduite de l'OUI, ou les 3 premiers octets du MAC si inconnu
- **RSSI** en dBm
- **`!`** rouge si l'OUI du fabricant a des CVEs BLE documentés
- **`?`** si la vérification est en cours (appareil à portée depuis moins de 3 s)
- **Barre de signal** colorée selon la proximité

### Couleurs de la barre

| Couleur | Niveau | RSSI indicatif |
|---------|--------|---------------|
| Bleu | Très proche | ≥ −40 dBm |
| Cyan | Proche | ≥ −55 dBm |
| Jaune | À portée | ≥ −67 dBm |
| Gris | Loin | < −67 dBm |

### LED WS2812

La LED reflète le statut de l'appareil **le plus fort détecté** :

| Couleur LED | Signification |
|-------------|---------------|
| Bleu | Très proche, OUI sain |
| Cyan | Proche |
| Orange | À portée |
| Vert très faible | Appareils détectés mais loin |
| Rouge | OUI du fabricant marqué vulnérable |
| Éteinte | Aucun appareil |

### Détection de vulnérabilités OUI

Quand un appareil reste à portée **TRÈS PROCHE** (RSSI ≥ −40 dBm) pendant **3 secondes**, son OUI (3 premiers octets du MAC) est comparé à une liste de 39 fabricants dont des puces BLE ont des CVEs documentés (BlueBorne, SweynTooth, BIAS…).

Un `!` rouge indique que le **fabricant** a livré des firmwares vulnérables historiquement. L'appareil spécifique a peut-être reçu un patch depuis.

### Identification des fabricants

La table OUI embarquée couvre les marques courantes : Apple, Samsung, Google, Fitbit, Garmin, Polar, Xiaomi, Amazfit, OnePlus, Sony, Bose, Jabra, Harman/JBL, Beats, Texas Instruments, Nordic Semi, Espressif, Raspberry Pi, Tile.

Pour un OUI non reconnu, les 3 premiers octets du MAC sont affichés bruts (`OUI XX:XX:XX`). La base OUI complète est consultable sur [maclookup.app](https://maclookup.app).

### Scan BLE

- Scan actif en continu (cycles de 5 secondes)
- Suivi de **64 appareils** simultanément
- Les appareils non vus depuis **15 secondes** sont retirés de la liste
- Rafraîchissement de l'écran toutes les **300 ms**

---

## Structure des fichiers

```
BLEWatch/
├── BLEWatch.ino          — point d'entrée Arduino (setup / loop)
├── blewatch.cpp          — logique BLE, détection OUI, UI LVGL
├── blewatch.h            — déclaration de Blewatch_Init()
├── Display_ST7789.cpp    — driver SPI ST7789 (Arduino, 80 MHz)
├── Display_ST7789.h      — constantes GPIO et prototypes
├── LVGL_Driver.cpp       — intégration LVGL 9.x (flush, tick timer)
├── LVGL_Driver.h         — macros dimensions et prototypes
├── Wireless.cpp          — fonctions WiFi/BLE de test (désactivées, #if 0)
├── Wireless.h            — déclarations
└── lv_conf.h             — configuration LVGL (RGB565, 172×320, Montserrat 14/20)
```

---

## Dépannage

| Symptôme | Cause probable | Solution |
|----------|---------------|---------|
| Erreur de compilation "text section exceeds" | Partition trop petite | Sélectionner **Huge APP** |
| Symboles dupliqués `ble_gap_*` au link | NimBLE-Arduino externe installée | La désinstaller |
| Écran blanc ou noir fixe | Mauvais ordre init | Vérifier `LCD_Init()` avant `Lvgl_Init()` |
| `lv_conf.h` non trouvé | LVGL ne le détecte pas | Placer `lv_conf.h` dans le même dossier que le sketch |
| Aucun appareil détecté | BLE non initialisé | `BLEDevice::init("")` doit s'exécuter dans la tâche FreeRTOS |

---

## Référence

- Projet d'origine : [PierreGode/WaveshareESP32C6LCD](https://github.com/PierreGode/WaveshareESP32C6LCD)
- CVEs BLE référencés : BlueBorne (Armis 2017), SweynTooth (2020), BIAS (2020)
