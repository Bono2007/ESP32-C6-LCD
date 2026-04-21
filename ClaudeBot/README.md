# ClaudeBot

Robot-mascotte animé pour **Spotpear / Waveshare ESP32-C6 LCD 1.47"**, piloté en temps réel par les hooks de **Claude Code**.

L'appareil devient un indicateur physique de l'état de Claude Code posé sur ton bureau : il s'anime différemment selon que Claude réfléchit, attend ta réponse, vient de terminer une action, ou est en alerte.

---

## Démonstration des états

| État | Déclencheur | Animation | LED |
|------|-------------|-----------|-----|
| **idle** | Démarrage / repos | Oreilles pulsent orange, clignement des yeux toutes ~4 s | Orange doux pulsé |
| **thinking** | Claude utilise un outil | Sourcils froncés, pupils qui bougent, oreilles oscillantes, `...` en bas | Jaune pulsé |
| **done** | Fin de tâche | Flash vert, bras levés, grand sourire, sourcils relevés | Vert bref |
| **waiting** | Claude attend ta réponse | **Corps entier qui saute**, bras alternés, overlay rouge pulsé, `?` blanc | Rouge stroboscope |
| **alert** | Alerte manuelle | Strobe rouge plein écran, bras alternés, `!` clignotant | Rouge stroboscope |

---

## Matériel requis

| Composant | Référence |
|-----------|-----------|
| Microcontrôleur | Spotpear ESP32-C6 LCD 1.47" (ST7789V3, 172×320) |
| Connexion | USB-C (flash + alimentation) |

Pas de câblage supplémentaire. L'ESP32-C6 intègre Wi-Fi, BLE et 802.15.4 sur la même antenne.

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

> WiFi et WebServer sont inclus dans le core ESP32, aucune installation supplémentaire.

---

## Configuration Arduino IDE

Sélectionner la carte : **Outils → Type de carte → esp32 → ESP32C6 Dev Module**

| Paramètre | Valeur |
|-----------|--------|
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| Upload Speed | 921600 |

---

## Configuration WiFi

Ouvrir `claudebot.h` et renseigner le réseau local :

```cpp
#define WIFI_SSID     "MonReseau"
#define WIFI_PASSWORD "MonMotDePasse"
```

Au démarrage, l'IP attribuée s'affiche 2,5 secondes sur l'écran (ex. `10.0.0.175`). Noter cette IP — elle sera nécessaire pour les hooks Claude Code.

---

## Flasher le projet

### Via Arduino IDE

1. Cloner le repo ou télécharger le dossier `ClaudeBot/`
2. Ouvrir `ClaudeBot/ClaudeBot.ino`
3. Brancher la carte en USB-C
4. **Outils → Port** : sélectionner le port série
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
cd ClaudeBot
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" ClaudeBot.ino

# Flasher (adapter le port)
arduino-cli upload --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" \
  --port /dev/cu.usbmodem1101 ClaudeBot.ino
```

---

## Intégration Claude Code (hooks)

L'intégration repose sur le système de **hooks** de Claude Code : des commandes shell déclenchées automatiquement à chaque événement du cycle de vie de Claude. Un simple `curl` vers l'ESP32 suffit à changer son état.

### Principe

Seulement 2 hooks sont nécessaires — `PostToolUse` est volontairement absent pour éviter les flashs verts parasites entre chaque outil :

```
Claude Code                            ESP32-C6 ClaudeBot
──────────                             ──────────────────
PreToolUse  ──→ curl /state?mode=thinking  ──→ 🤔 thinking
Stop        ──→ curl /state?mode=waiting   ──→ ❓ waiting (rouge)
```

Les hooks sont configurés avec `"async": true` pour ne **jamais bloquer** Claude Code — le `curl` part en tâche de fond avec un timeout de 1 seconde maximum.

### Configuration dans `~/.claude/settings.json`

Ajouter les entrées suivantes dans la section `"hooks"` (remplacer l'IP par celle affichée au démarrage) :

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "curl -s --max-time 1 'http://10.0.0.175/state?mode=thinking' >/dev/null 2>&1 || true",
            "async": true
          }
        ]
      }
    ],
    "Stop": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "curl -s --max-time 1 'http://10.0.0.175/state?mode=waiting' >/dev/null 2>&1 || true",
            "async": true
          }
        ]
      }
    ]
  }
}
```

> Si tu as déjà des hooks existants, ajouter ces entrées dans les tableaux correspondants — ne pas remplacer les entrées existantes.

> **Pourquoi pas `PostToolUse` ?** Claude utilise souvent 10–20 outils d'affilée. Déclencher un flash vert après chaque outil crée un clignotement confus. L'état **waiting** (rouge) est l'indicateur principal : il s'active uniquement quand Claude a fini et attend ta réponse.

### Vérifier que les hooks sont actifs

Envoyer un message à Claude Code et observer le robot : il doit passer en **thinking** (oreilles oscillantes, sourcils froncés) pendant l'exécution, puis **waiting** (rouge + bounce) quand Claude attend ta réponse.

---

## API HTTP

L'ESP32 expose un serveur HTTP sur le port 80. Les changements d'état peuvent aussi être déclenchés manuellement depuis n'importe quel appareil du réseau.

### `GET /state?mode=<état>`

| Valeur `mode` | Effet |
|---------------|-------|
| `idle` | Repos, respiration lente |
| `thinking` | Réflexion active |
| `waiting` | Attente de décision |
| `done` | Succès (retour auto vers idle après 2 s) |
| `alert` | Alerte rouge stroboscopique |

```bash
# Exemples
curl "http://10.0.0.175/state?mode=thinking"
curl "http://10.0.0.175/state?mode=done"
curl "http://10.0.0.175/state?mode=waiting"
curl "http://10.0.0.175/state?mode=alert"
curl "http://10.0.0.175/state?mode=idle"
```

### `GET /`

Retourne un message de statut et le rappel des endpoints disponibles.

---

## Design du robot

Le robot est entièrement dessiné avec les primitives LVGL — aucune image embarquée. Il est composé de 17 objets LVGL superposés sur un fond noir :

```
 (●)   (●)        ← oreilles rondes (orange clair, pulsent en idle)
 ┌─────────────┐  ← tête (orange Claude #D4501C, radius 26)
 │ ▬       ▬  │  ← sourcils (brun, s'inclinent selon l'humeur)
 │  ◉       ◉  │  ← yeux blancs chauds + pupilles (mobiles)
 │      ⌣     │  ← bouche (arc LVGL, change selon état)
 └─────────────┘
   [   corps   ]  ← body (orange foncé #B43E12)
 [bras]   [bras]  ← bras (se lèvent / s'agitent)
   [leg] [leg]    ← jambes
       status     ← label texte en bas ("...", "?", "!")
   [  overlay  ]  ← rectangle plein écran pour les flashs colorés
```

### Palette de couleurs (Claude orange)

| Élément | Couleur |
|---------|---------|
| Fond écran | `#06080F` (bleu nuit) |
| Tête | `#D4501C` (orange Claude) |
| Corps / bras / jambes | `#B43E12` (orange foncé) |
| Oreilles | `#F06E37` (orange clair) |
| Yeux | `#FFF0E6` (blanc chaud) |
| Pupilles | `#140500` (brun-noir) |
| Sourcils | `#782305` (brun sourcil) |
| Bouche | `#FFC8AA` (beige peau) |

### Animations par état

**idle**
- Oreilles : couleur pulsée doucement (sinusoïde ~6 s)
- Yeux : clignement toutes les 3–5 s (pupils écrasés 2 frames)
- LED : orange pulsé doucement

**thinking**
- Pupils : déplacement gauche + haut (regard concentré)
- Sourcils : froncés (rapprochés vers le bas)
- Oreilles : oscillation horizontale rapide ±3 px
- Status : `"."` → `".."` → `"..."` jaune
- LED : jaune pulsé

**waiting** ← état prioritaire
- **Corps entier** (oreilles, tête, yeux, bouche, corps, jambes) : bounce vertical ±6 px (sinusoïde)
- Bras : agitation alternée ±14 px (comme quelqu'un qui appelle)
- Overlay : rouge intense pulsé (opacité 140–215)
- Sourcils : levés (sourcils surpris)
- Bouche : grande ouverte (surprise)
- Status : grand `"?"` blanc (Montserrat 36) clignotant
- LED : rouge stroboscopique (2 ticks on / 2 ticks off)

**done**
- Overlay : flash vert décroissant sur 12 frames
- Bras : remontés de 16 px
- Bouche : grand sourire (185°→355°)
- Sourcils : levés (joyeux)
- Retour automatique vers **idle** après 1,75 s (35 frames)
- LED : vert décroissant

**alert**
- Overlay : strobe rouge (2 frames on / 2 off)
- Sourcils : froncés
- Bouche : frown
- Bras : alternance haut-bas (4 frames chacun)
- Status : `"!"` blanc clignotant (Montserrat 36)
- LED : rouge stroboscopique

---

## Architecture du code

```
ClaudeBot/
├── ClaudeBot.ino         — setup() + loop() Arduino
├── claudebot.h           — config WiFi, enum bot_state_t, prototypes
├── claudebot.cpp         — init WiFi, serveur HTTP, LED NeoPixel
├── robot.h               — prototypes Robot_Init / SetState / Tick
├── robot.cpp             — dessin LVGL et animations (12 objets, 5 états)
├── Display_ST7789.cpp/.h — driver SPI ST7789 (Arduino 80 MHz)
├── LVGL_Driver.cpp/.h    — intégration LVGL 9.x (flush, tick timer)
└── lv_conf.h             — config LVGL (RGB565, 172×320, polices 14→48 pt)
```

### Flux d'exécution

```
setup()
  └── LCD_Init()           initialise SPI ST7789
  └── Lvgl_Init()
        └── lv_init()
        └── ClaudeBot_Init()
              └── WiFi.begin() + serveur HTTP
              └── Robot_Init()  ← crée les 12 objets LVGL + timer 50 ms
        └── esp_timer (tick LVGL toutes les 5 ms)

loop()
  ├── Timer_Loop()          lv_timer_handler() → Robot_Tick() toutes 50 ms
  └── ClaudeBot_HandleClient()  traite les requêtes HTTP entrantes

HTTP GET /state?mode=X
  └── Robot_SetState(BOT_X)  ← change l'état, le prochain Tick l'anime
```

### Timer LVGL vs FreeRTOS

Tout tourne sur le **core principal** sans tâche FreeRTOS dédiée. Le timer d'animation est un `lv_timer_t` (50 ms) appelé par `lv_timer_handler()` dans `loop()`. Le serveur HTTP est traité dans le même `loop()` de façon non-bloquante (`handleClient()`). Aucune synchronisation mutex nécessaire.

---

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|---------|
| Erreur "text section exceeds" | Partition trop petite | Sélectionner **Huge APP** |
| Écran reste sur "Connexion WiFi..." | SSID/password incorrects | Vérifier `claudebot.h` |
| Robot figé | Timer LVGL non appelé | Vérifier que `Timer_Loop()` est dans `loop()` |
| Hooks sans effet | IP incorrecte | Lire l'IP affichée au démarrage ou faire `arp -a \| grep esp` |
| Hooks bloquent Claude Code | `async` manquant | Ajouter `"async": true` dans `settings.json` |
| Bouche invisible | Arc LVGL angle | Les angles sont en degrés, 0° = Est, sens horaire |
| LED ne s'allume pas | GPIO 8 | Vérifier que `LED_PIN 8` correspond à ta carte |

---

## Étendre le projet

### Ajouter un état personnalisé

1. Ajouter une valeur dans `bot_state_t` (`claudebot.h`)
2. Ajouter le `case` dans `Robot_Tick()` (`robot.cpp`)
3. Ajouter le handler HTTP dans `claudebot.cpp`
4. Ajouter l'entrée dans le tableau de l'API ci-dessus

### Déclencher depuis un script externe

```bash
# Alerte depuis un cron ou un webhook
curl "http://10.0.0.175/state?mode=alert"

# Script de monitoring : rouge si service down
ping -c 1 mon-serveur.local > /dev/null 2>&1 \
  && curl -s "http://10.0.0.175/state?mode=idle" \
  || curl -s "http://10.0.0.175/state?mode=alert"
```

### Déclencher depuis d'autres outils CLI

Le robot n'est pas limité à Claude Code — n'importe quelle CI/CD, script, ou outil peut envoyer un état :

```bash
# GitHub Actions
- name: Notify ClaudeBot
  run: curl -s "http://10.0.0.175/state?mode=thinking"

# Makefile
build:
    curl -s "http://10.0.0.175/state?mode=thinking" || true
    make -j4
    curl -s "http://10.0.0.175/state?mode=done" || true
```
