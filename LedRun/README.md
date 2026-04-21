# LedRun

Affichage piloté à distance via HTTP/WiFi pour **Spotpear / Waveshare ESP32-C6 LCD 1.47"**.

Inspiré de [led.run](https://github.com/led-run/led.run) (MIT) — les effets sont portés nativement en C++/LVGL, aucun navigateur ni serveur intermédiaire requis.

Trois modes pilotables par simple requête HTTP GET depuis n'importe quel appareil du réseau :

| Mode | Exemple | Effet |
|------|---------|-------|
| **bougie** | `/candle?warmth=8` | 5 couches animées, flicker réglable, LED ambre |
| **texte** | `/text?text=42` | Texte géant centré, police auto-adaptée 20→48 pt |
| **couleur** | `/solid?color=FF2200` | Fond uni, LED synchronisée |

---

## Matériel requis

| Composant | Référence |
|-----------|-----------|
| Microcontrôleur | Spotpear ESP32-C6 LCD 1.47" (ST7789V3, 172×320) |
| Connexion | USB-C (flash + alimentation) |

Pas de câblage supplémentaire.

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

> WiFi et WebServer sont inclus dans le core ESP32, aucune installation supplémentaire nécessaire.

---

## Configuration Arduino IDE

Sélectionner la carte : **Outils → Type de carte → esp32 → ESP32C6 Dev Module**

| Paramètre | Valeur |
|-----------|--------|
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| Upload Speed | 921600 |

---

## Configuration WiFi

Ouvrir `ledrun.h` et renseigner le réseau local :

```cpp
#define WIFI_SSID     "MonReseau"
#define WIFI_PASSWORD "MonMotDePasse"
```

Au démarrage, l'IP attribuée s'affiche 2,5 secondes sur l'écran.

---

## Flasher le projet

### Via Arduino IDE

1. Cloner le repo ou télécharger le dossier `LedRun/`
2. Ouvrir `LedRun/LedRun.ino`
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

# Compiler
cd LedRun
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" LedRun.ino

# Flasher (adapter le port)
arduino-cli upload --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" \
  --port /dev/cu.usbmodem1101 LedRun.ino
```

---

## API HTTP

L'ESP32 expose un serveur HTTP sur le port 80. Toutes les requêtes sont des `GET`.

### `GET /`

Retourne l'état courant en JSON.

```bash
curl http://IP/
# {"mode":"candle","warmth":8,"text":"Hello","color":"FF9329","bg":"000000"}
```

---

### `GET /candle`

Active le mode bougie animé.

| Paramètre | Type | Défaut | Description |
|-----------|------|--------|-------------|
| `warmth` | entier 1–10 | 8 | Intensité du flicker (1 = calme, 10 = agité) |
| `color` | hex RRGGBB | FF9329 | Couleur de base (ambre par défaut) |

```bash
curl "http://IP/candle?warmth=8"
curl "http://IP/candle?warmth=3&color=4488FF"   # bougie bleue calme
```

---

### `GET /text`

Affiche du texte ou un nombre en grand, centré sur l'écran.  
La taille de police est choisie automatiquement selon la longueur :

| Longueur | Police |
|----------|--------|
| ≤ 4 caractères | 48 pt |
| ≤ 7 caractères | 36 pt |
| ≤ 13 caractères | 28 pt |
| > 13 caractères | 20 pt |

| Paramètre | Type | Défaut | Description |
|-----------|------|--------|-------------|
| `text` | chaîne | Hello | Texte à afficher |
| `color` | hex RRGGBB | FF9329 | Couleur du texte |
| `bg` | hex RRGGBB | 000000 | Couleur du fond |

```bash
curl "http://IP/text?text=42"
curl "http://IP/text?text=1337&color=00FF88&bg=000000"
curl "http://IP/text?text=OPEN&color=FFFFFF&bg=008800"
```

Appel répété sans rechargement : si le mode texte est déjà actif, seul le contenu change (pas de clignotement).

---

### `GET /solid`

Fond uni pleine couleur, LED synchronisée.

| Paramètre | Type | Défaut | Description |
|-----------|------|--------|-------------|
| `color` | hex RRGGBB | FF0000 | Couleur du fond |

```bash
curl "http://IP/solid?color=FF2200"
curl "http://IP/solid?color=FFFFFF"   # blanc — lampe de lecture
```

---

## Exemples d'usage

### Compteur en temps réel

```bash
for i in $(seq 1 60); do
  curl -s "http://IP/text?text=$i&color=00FF88" > /dev/null
  sleep 1
done
```

### Chronomètre décroissant

```bash
for i in $(seq 60 -1 0); do
  curl -s "http://IP/text?text=$i&color=FF4400" > /dev/null
  sleep 1
done
curl -s "http://IP/text?text=GO!&color=00FF00&bg=002200"
```

### Score sportif

```bash
curl "http://IP/text?text=3-1&color=FFFFFF&bg=003399"
```

### Alerte rouge

```bash
curl "http://IP/solid?color=FF0000"
# puis revenir à la bougie
curl "http://IP/candle?warmth=5"
```

### Script de mise à jour depuis une API externe

```bash
while true; do
  VALEUR=$(curl -s "https://mon-api.exemple/valeur" | jq -r '.count')
  curl -s "http://IP/text?text=$VALEUR&color=00FFAA" > /dev/null
  sleep 10
done
```

---

## Fonctionnement du mode bougie

5 ellipses LVGL empilées, de l'extérieur vers le noyau :

| Couche | Taille | Couleur | Rôle |
|--------|--------|---------|------|
| 0 | 168×310 | Brun très sombre | Halo de fond |
| 1 | 128×240 | Brun chaud | Lueur externe |
| 2 | 86×170 | Orange | Flamme moyenne |
| 3 | 48×95 | Ambre vif | Flamme interne |
| 4 | 20×42 | Jaune clair | Noyau |

Chaque couche oscille indépendamment (opacité + jitter de position). Le paramètre `warmth` contrôle :
- l'amplitude du flicker (±20 à ±170 en opacité sur 255)
- la fréquence des changements (40 à 120 ms par couche)
- le jitter de position (±2 à ±20 px pour les couches internes)

La LED WS2812 pulse en ambre, synchronisée sur l'opacité du noyau.

---

## LED WS2812

| Mode | Couleur | Signification |
|------|---------|---------------|
| Démarrage | Bleu | Connexion WiFi en cours |
| WiFi OK | Vert bref | IP affichée |
| WiFi KO | Rouge | Pas de réseau — mode local sans HTTP |
| Bougie | Ambre pulsé | Synchronisé sur l'animation |
| Texte | Couleur fg ÷ 4 | Fond doux assorti au texte |
| Solid | Couleur bg ÷ 3 | Assorti au fond |

---

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|---------|
| Erreur "text section exceeds" | Partition trop petite | Sélectionner **Huge APP** |
| Écran reste sur "Connexion WiFi..." | SSID/password incorrects | Vérifier `ledrun.h` |
| Pas de réponse HTTP | IP différente | Lire l'IP affichée au démarrage ou faire `arp -a` |
| Texte coupé | Texte trop long pour la police | Normal — LVGL wrap activé |
| Bougie figée | Timer LVGL non démarré | Vérifier que `lv_timer_handler()` est appelé dans `loop()` |

---

## Structure des fichiers

```
LedRun/
├── LedRun.ino            — point d'entrée Arduino (setup / loop)
├── ledrun.h              — config WiFi, enum modes, prototypes
├── ledrun.cpp            — init, serveur HTTP, dispatch modes, LED
├── mode_candle.h/.cpp    — effet bougie (5 couches LVGL animées)
├── mode_text.h/.cpp      — texte géant centré, police auto-adaptée
├── Display_ST7789.cpp/.h — driver SPI ST7789 (Arduino, 80 MHz)
├── LVGL_Driver.cpp/.h    — intégration LVGL 9.x (flush, tick timer)
└── lv_conf.h             — config LVGL (RGB565, 172×320, polices 14→48 pt)
```
