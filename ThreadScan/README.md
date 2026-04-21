# ThreadScan

Détecteur de réseaux **Thread / Matter** pour **Spotpear / Waveshare ESP32-C6 LCD 1.47"**.

Effectue deux scans en boucle :
1. **Energy scan 802.15.4** sur les 16 canaux (ch 11–26) — détecte tout réseau Thread actif, même si les appareils sont déjà appairés et silencieux
2. **Scan BLE** filtré sur l'UUID `0xFFF6` — détecte les appareils Matter en cours de commissioning (appairage)

---

## Matériel requis

| Composant | Référence |
|-----------|-----------|
| Microcontrôleur | Spotpear ESP32-C6 LCD 1.47" (ST7789V3, 172×320) |
| Connexion | USB-C (flash + alimentation) |

Pas de câblage supplémentaire. L'ESP32-C6 intègre nativement les radios Wi-Fi, BLE **et** IEEE 802.15.4 (Thread/Zigbee) sur la même antenne onboard.

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

> L'API `esp_ieee802154` est fournie directement par le core ESP32 3.x, aucune librairie supplémentaire n'est nécessaire pour le scan 802.15.4.

---

## Configuration Arduino IDE

Sélectionner la carte : **Outils → Type de carte → esp32 → ESP32C6 Dev Module**

| Paramètre | Valeur |
|-----------|--------|
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| Upload Speed | 921600 |

---

## Flasher le projet

1. Cloner le repo ou télécharger le dossier `ThreadScan/`
2. Ouvrir `ThreadScan/ThreadScan.ino` dans Arduino IDE
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
cd ThreadScan
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" ThreadScan.ino
arduino-cli upload  --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" \
  --port /dev/cu.usbmodem1101 ThreadScan.ino
```

---

## Fonctionnement

### Affichage

```
ThreadScan                   802.15.4 scan...
802.15.4  ch 11 ──────────── 26
 ░░░░▂▂▅▅▇▇▅▅▂▂░░
11  13  15  17  19  21  23  25
Meilleur: ch 15 (-82 dBm)
─────────────────────────────
Matter BLE  (UUID 0xFFF6)
Eve Energy      -48dB
Aucun autre appareil Matter
```

### Section 802.15.4 — barres de canal

16 barres verticales représentent les canaux 11 à 26 de la bande 802.15.4 (2405–2480 MHz) :

| Couleur | Niveau | Signification |
|---------|--------|---------------|
| Gris | < −92 dBm | Rien / bruit de fond |
| Vert | −92 à −85 dBm | Activité Wi-Fi ou Zigbee faible |
| Orange | −85 à −75 dBm | Réseau actif probable |
| Rouge | ≥ −75 dBm | Réseau Thread/Zigbee fort |

La ligne **"Meilleur canal"** indique le canal avec le plus d'énergie détectée et son niveau en dBm.

### Section Matter BLE

Un appareil Matter diffuse l'UUID de service `0xFFF6` **uniquement pendant son commissioning** (phase d'appairage initial). Une fois appairé, il disparaît de cette liste — ce qui est normal.

Pour faire apparaître un appareil ici, le mettre en mode appairage (reset usine, bouton dédié selon le fabricant).

### Cycle de scan

| Phase | Durée | Description |
|-------|-------|-------------|
| 802.15.4 | ~350 ms | 16 canaux × 20 ms chacun |
| BLE Matter | 5 s | Scan actif, filtre UUID 0xFFF6 |
| Pause | 2 s | Affichage des résultats |
| **Total** | **~7,5 s** | Puis recommence |

### LED WS2812

| Couleur | Signification |
|---------|---------------|
| Rouge | Réseau Thread/Zigbee détecté (énergie ≥ −85 dBm) |
| Vert | Spectre 802.15.4 libre |
| Bleu (démarrage) | Initialisation en cours |

---

## Comprendre les canaux 802.15.4

Thread et Zigbee utilisent tous deux la bande 2.4 GHz via le standard IEEE 802.15.4, sur les canaux 11 à 26 :

| Canal 802.15.4 | Fréquence | Proche du Wi-Fi |
|----------------|-----------|-----------------|
| 11 | 2405 MHz | Wi-Fi ch 1 (2412 MHz) |
| 15 | 2425 MHz | — |
| 20 | 2450 MHz | Wi-Fi ch 8 |
| 25 | 2475 MHz | Wi-Fi ch 13 |
| 26 | 2480 MHz | — |

Thread choisit typiquement les canaux **15, 20 ou 25** pour éviter les interférences Wi-Fi. Si une barre est rouge sur l'un de ces canaux, il y a très probablement un réseau Thread actif à portée.

---

## Ce que ce projet ne fait pas

- Il ne décode pas les trames Thread (pas de sniffing protocole)
- Il ne liste pas les appareils Thread déjà appairés (ils n'émettent pas l'UUID Matter en BLE)
- Il ne rejoint pas le réseau Thread

Pour aller plus loin vers un border router Thread complet, voir le projet officiel Espressif [`esp-thread-br`](https://github.com/espressif/esp-thread-br).

---

## Structure des fichiers

```
ThreadScan/
├── ThreadScan.ino        — point d'entrée Arduino (setup / loop)
├── threadscan.cpp        — scan 802.15.4 + BLE Matter + UI LVGL
├── threadscan.h          — déclaration de ThreadScan_Init()
├── Display_ST7789.cpp    — driver SPI ST7789 (Arduino, 80 MHz)
├── Display_ST7789.h      — constantes GPIO et prototypes
├── LVGL_Driver.cpp       — intégration LVGL 9.x (flush, tick timer)
├── LVGL_Driver.h         — macros dimensions et prototypes
└── lv_conf.h             — configuration LVGL (RGB565, 172×320, Montserrat 14)
```

---

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|---------|
| Erreur "text section exceeds" | Partition trop petite | Sélectionner **Huge APP** |
| `esp_ieee802154_enable` non déclaré | Core trop ancien | Mettre à jour esp32 core ≥ 3.x |
| Barres toutes grises | Normal sans réseau Thread à portée | Vérifier en présence d'appareils Zigbee/Thread |
| Aucun appareil Matter BLE | Appareils déjà appairés | Mettre l'appareil en mode commissioning (reset usine) |
| Crash au démarrage | NVS non initialisé | `nvs_flash_init()` est appelé dans la tâche — vérifier l'ordre d'init |
