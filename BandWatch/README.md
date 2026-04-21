# BandWatch

Analyseur de spectre Wi-Fi 2.4 GHz pour **Spotpear / Waveshare ESP32-C6 LCD 1.47"**.  
Capture le trafic en mode promiscuous sur les 13 canaux et affiche en temps réel les canaux les plus chargés.

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

| Librairie | Version testée |
|-----------|---------------|
| **lvgl** | 9.5.0 |
| **Adafruit NeoPixel** | 1.15.x |

---

## Configuration Arduino IDE

Sélectionner la carte : **Outils → Type de carte → esp32 → ESP32C6 Dev Module**

Paramètres obligatoires :

| Paramètre | Valeur |
|-----------|--------|
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| Upload Speed | 921600 |

---

## Flasher le projet

1. Cloner le repo ou télécharger le dossier `BandWatch/`
2. Ouvrir `BandWatch/BandWatch.ino` dans Arduino IDE
3. Brancher la carte en USB-C
4. Sélectionner le port : **Outils → Port**
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
cd BandWatch
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" BandWatch.ino
arduino-cli upload  --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" \
  --port /dev/cu.usbmodem1101 BandWatch.ino
```

---

## Fonctionnement

### Affichage

```
BandWatch 2.4 GHz
  Activité globale
  ████████████░░░░
     Top 3 canaux
Ch  1  342 pkts  87%
████████████░░░░
Ch  6  198 pkts  61%
█████████░░░░░░░
Ch 11   87 pkts  34%
████░░░░░░░░░░░░
```

- **Barre globale** : activité cumulée sur les 13 canaux
- **Top 3 canaux** : les plus chargés avec nombre de paquets capturés et score d'activité
- Les barres se mettent à jour toutes les **120 ms**

### Mode promiscuous

La radio Wi-Fi est mise en mode promiscuous : elle reçoit tous les paquets 802.11 qui passent dans l'air, sans être associée à un réseau. Le firmware **hoppe automatiquement** entre les canaux 1 à 13, 260 ms par canal.

### Score d'activité (pondération logarithmique)

Chaque canal reçoit un score 0–100 % calculé à partir de 4 métriques :

| Métrique | Poids |
|----------|-------|
| Paquets / seconde | 40 % |
| Octets / seconde | 30 % |
| Ratio paquets à signal fort (≥ −65 dBm) | 20 % |
| Nombre d'émetteurs uniques | 10 % |

La mise à l'échelle est logarithmique pour ne pas saturer sur les canaux très actifs.

### LED WS2812

La LED reflète l'activité globale du spectre :

| Couleur | Signification |
|---------|---------------|
| Vert | Spectre calme |
| Jaune-orange | Activité modérée |
| Rouge | Spectre saturé |

Un **flash blanc** au démarrage confirme que la LED fonctionne.

### Canaux 2.4 GHz

Les canaux non superposés en Europe sont **1, 6 et 11**. Si ces 3 canaux sont tous chargés, le spectre est saturé et les performances Wi-Fi seront dégradées.

| Canal | Fréquence centrale |
|-------|--------------------|
| 1 | 2412 MHz |
| 6 | 2437 MHz |
| 11 | 2462 MHz |
| 13 | 2472 MHz (Europe uniquement) |

---

## Structure des fichiers

```
BandWatch/
├── BandWatch.ino         — point d'entrée Arduino (setup / loop)
├── bandwatch.cpp         — logique promiscuous, hopping, score, UI LVGL
├── bandwatch.h           — déclaration de Bandwatch_Init()
├── Display_ST7789.cpp    — driver SPI ST7789 (Arduino, 80 MHz)
├── Display_ST7789.h      — constantes GPIO et prototypes
├── LVGL_Driver.cpp       — intégration LVGL 9.x (flush, tick timer)
├── LVGL_Driver.h         — macros dimensions et prototypes
└── lv_conf.h             — configuration LVGL (RGB565, 172×320, Montserrat 14/20)
```

---

## Dépannage

| Symptôme | Cause probable | Solution |
|----------|---------------|---------|
| Erreur de compilation "text section exceeds" | Partition trop petite | Sélectionner **Huge APP** |
| `esp_wifi_set_promiscuous` non déclaré | Headers ESP-IDF manquants | Vérifier la présence de `#include <esp_wifi.h>` dans bandwatch.cpp |
| Tous les canaux à 0 | Mode promiscuous non activé | WiFi doit être en mode `WIFI_STA` avant l'activation promiscuous |
| Écran blanc ou noir fixe | Mauvais ordre init | Vérifier `LCD_Init()` avant `Lvgl_Init()` |

---

## Note légale

Le mode promiscuous capture tous les paquets Wi-Fi à portée, y compris ceux destinés à d'autres appareils. Utiliser uniquement sur des réseaux dont vous êtes propriétaire ou pour lesquels vous avez une autorisation explicite. En France, l'interception de communications est encadrée par l'article 226-15 du Code pénal.
