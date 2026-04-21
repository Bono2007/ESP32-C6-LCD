# ClaudeBot

Robot-mascotte animé pour **Spotpear / Waveshare ESP32-C6 LCD 1.47"**, piloté en temps réel par les hooks de **Claude Code**.

L'appareil devient un indicateur physique de l'état de Claude Code posé sur ton bureau. Les animations sont calquées sur [clawd-on-desk](https://github.com/rullerzhou-afk/clawd-on-desk) et utilisent le même mapping d'événements.

---

## États et animations

| État | Déclencheur hook | Animation | LED |
|------|-----------------|-----------|-----|
| **idle** | Démarrage / repos | Oreilles pulsent orange, clignement des yeux | Orange doux pulsé |
| **thinking** | `UserPromptSubmit` | Pupils dérivent, oreilles oscillantes, `...` | Jaune pulsé |
| **working** | `PreToolUse` | Bras tapent en alternance rapide, `...` | Orange actif pulsé |
| **juggling** | `SubagentStart` | Bras en opposition de phase (jonglage), pupils qui suivent | Violet pulsé |
| **sweeping** | `PreCompact` | Corps qui balaie de gauche à droite | Cyan pulsé |
| **sleeping** | `SessionEnd` | Respiration lente, yeux fermés, `z` | Bleu très doux |
| **waiting** | `Stop` | Corps qui bounce, bras alternés, `?` rouge | Rouge pulsé |
| **done** | manuel | Flash vert, bras levés, grand sourire | Vert bref |
| **alert** | manuel | Strobe rouge, bras alternés, `!` | Rouge stroboscope |

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

# Flasher (adapter le port — trouver avec : arduino-cli board list)
arduino-cli upload --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" \
  --port /dev/cu.usbmodem1101 ClaudeBot.ino
```

---

## Intégration Claude Code (hooks)

Les hooks Claude Code déclenchent automatiquement les animations. Le mapping est identique à celui de **clawd-on-desk**.

### Principe

```
Événement Claude Code          État ESP32        Animation
──────────────────────         ──────────        ─────────
UserPromptSubmit          →    thinking          pupils dérivent
PreToolUse                →    working           bras tapent
SubagentStart             →    juggling          bras en jonglage
PreCompact                →    sweeping          corps balaie
SessionEnd                →    sleeping          respiration lente
Stop                      →    waiting           bounce + "?"
```

Tous les hooks sont configurés avec `"async": true` — le `curl` part en tâche de fond, timeout 1 s maximum, sans jamais bloquer Claude Code.

### Configuration dans `~/.claude/settings.json`

Ajouter dans la section `"hooks"` (remplacer l'IP par celle affichée au démarrage) :

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "curl -s --max-time 1 'http://10.0.0.175/state?mode=thinking' >/dev/null 2>&1 || true",
            "async": true
          }
        ]
      }
    ],
    "PreToolUse": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "curl -s --max-time 1 'http://10.0.0.175/state?mode=working' >/dev/null 2>&1 || true",
            "async": true
          }
        ]
      }
    ],
    "SubagentStart": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "curl -s --max-time 1 'http://10.0.0.175/state?mode=juggling' >/dev/null 2>&1 || true",
            "async": true
          }
        ]
      }
    ],
    "PreCompact": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "curl -s --max-time 1 'http://10.0.0.175/state?mode=sweeping' >/dev/null 2>&1 || true",
            "async": true
          }
        ]
      }
    ],
    "SessionEnd": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "curl -s --max-time 1 'http://10.0.0.175/state?mode=sleeping' >/dev/null 2>&1 || true",
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

> Si tu as déjà des hooks existants, ajouter ces entrées dans les tableaux correspondants sans écraser les entrées existantes.

### Trouver l'IP du robot

```bash
# Option 1 : affichée 2,5 s à l'écran au démarrage
# Option 2 : arp
arp -a | grep -i "58:8c:81"
# Option 3 : ping broadcast
ping -c1 255.255.255.255 && arp -a | grep esp
```

---

## API HTTP

L'ESP32 expose un serveur HTTP sur le port 80.

### `GET /state?mode=<état>`

| Valeur `mode` | Description |
|---------------|-------------|
| `idle` | Repos, oreilles pulsent |
| `thinking` | Pupils dérivent, oreilles oscillent, `...` |
| `working` | Bras tapent alternés, `...` |
| `juggling` | Bras en jonglage (opposition de phase) |
| `sweeping` | Corps qui balaie gauche/droite |
| `sleeping` | Respiration lente, yeux fermés, `z` |
| `waiting` | Bounce + frown + `?` rouge |
| `done` | Flash vert, bras levés (→ idle après 1,75 s) |
| `alert` | Strobe rouge, `!` |

```bash
curl "http://10.0.0.175/state?mode=working"
curl "http://10.0.0.175/state?mode=juggling"
curl "http://10.0.0.175/state?mode=sweeping"
curl "http://10.0.0.175/state?mode=sleeping"
curl "http://10.0.0.175/state?mode=waiting"
curl "http://10.0.0.175/state?mode=done"
curl "http://10.0.0.175/state?mode=alert"
curl "http://10.0.0.175/state?mode=idle"
```

### `GET /`

Retourne un message de statut et la liste des modes disponibles.

---

## Design du robot

Entièrement dessiné avec les primitives LVGL — aucune image embarquée. 17 objets LVGL sur fond `#06080F` :

```
 (●)   (●)        ← oreilles rondes (orange clair, pulsent en idle)
 ┌─────────────┐  ← tête (#D4501C orange Claude, radius 26)
 │ ▬       ▬  │  ← sourcils brun (s'inclinent selon l'humeur)
 │  ◉       ◉  │  ← yeux blanc chaud + pupilles (mobiles)
 │      ⌣     │  ← bouche arc LVGL (change selon état)
 └─────────────┘
   [   corps   ]  ← body (#B43E12 orange foncé)
 [bras]   [bras]  ← bras (se lèvent / s'agitent)
   [leg] [leg]    ← jambes
       status     ← label texte ("...", "?", "z", "!")
   [  overlay  ]  ← rectangle plein écran (flashs done/alert uniquement)
```

### Palette de couleurs

| Élément | Couleur hex | Description |
|---------|------------|-------------|
| Fond | `#06080F` | Bleu nuit |
| Tête | `#D4501C` | Orange Claude |
| Corps / bras / jambes | `#B43E12` | Orange foncé |
| Oreilles | `#F06E37` | Orange clair |
| Yeux | `#FFF0E6` | Blanc chaud |
| Pupilles | `#140500` | Brun-noir |
| Sourcils | `#782305` | Brun sourcil |
| Bouche | `#FFC8AA` | Beige peau |

### Détail des animations

**idle** — repos  
- Oreilles : couleur pulsée (sinusoïde ~6 s)  
- Yeux : clignement aléatoire toutes les 3–5 s  
- LED : orange très doux

**thinking** — `UserPromptSubmit`  
- Pupils : dérive gauche + haut (regard concentré)  
- Sourcils : froncés vers le bas  
- Oreilles : oscillation horizontale ±3 px  
- Status : `"."` → `".."` → `"..."` jaune  
- LED : jaune pulsé

**working** — `PreToolUse`  
- Bras gauche/droit alternent rapidement (période 300 ms)  
- Pupils : regard vers le bas (concentré sur le travail)  
- Status : `"."` → `".."` → `"..."` orange  
- LED : orange actif pulsé

**juggling** — `SubagentStart`  
- Bras en opposition de phase sinusoïdale ±16 px  
- Pupils qui suivent les bras (gauche/droite)  
- Corps qui oscille légèrement  
- LED : violet pulsé (multi-agents)

**sweeping** — `PreCompact`  
- Tête + oreilles + yeux basculent latéralement ±8 px  
- Bras bougent en opposition (balayage)  
- LED : cyan pulsé

**sleeping** — `SessionEnd`  
- Corps monte/descend lentement ±3 px (respiration)  
- Yeux fermés en permanence (pupils écrasés)  
- Status : `"z"` bleu clignotant lent  
- LED : bleu très doux

**waiting** — `Stop`  
- Corps entier bounce ±4 px (sinusoïde)  
- Bras alternés ±8 px (appel)  
- Bouche : petit frown  
- Status : `"?"` rouge clignotant (Montserrat 36)  
- LED : rouge pulsé

**done** — manuel  
- Overlay vert décroissant sur 12 frames (~600 ms)  
- Bras levés de 16 px  
- Bouche : grand sourire (185°→355°)  
- Sourcils levés (joyeux)  
- Retour automatique vers **idle** après 1,75 s  
- LED : vert décroissant

**alert** — manuel  
- Overlay strobe rouge (2 frames on / 2 off)  
- Sourcils froncés, bouche frown  
- Bras alternés  
- Status : `"!"` blanc clignotant (Montserrat 36)  
- LED : rouge stroboscopique

---

## Architecture du code

```
ClaudeBot/
├── ClaudeBot.ino         — setup() + loop() Arduino
├── claudebot.h           — config WiFi, enum bot_state_t (9 états), prototypes
├── claudebot.cpp         — init WiFi, serveur HTTP, LED NeoPixel, handlers HTTP
├── robot.h               — prototypes Robot_Init / Robot_SetState / Robot_Tick
├── robot.cpp             — 17 objets LVGL, enter_state(), 9 tick functions
├── Display_ST7789.cpp/.h — driver SPI ST7789 (Arduino 80 MHz)
├── LVGL_Driver.cpp/.h    — intégration LVGL 9.x (flush callback, esp_timer tick)
└── lv_conf.h             — config LVGL (RGB565, 172×320, polices Montserrat 14/36)
```

### Flux d'exécution

```
setup()
  └── LCD_Init()           initialise SPI ST7789
  └── Lvgl_Init()
        └── lv_init()
        └── ClaudeBot_Init()
              └── WiFi.begin() + WebServer
              └── Robot_Init()  ← crée les 17 objets LVGL + lv_timer 50 ms
        └── esp_timer (lv_tick_inc toutes les 5 ms)

loop()
  ├── Timer_Loop()              lv_timer_handler() → Robot_Tick() toutes 50 ms
  └── ClaudeBot_HandleClient()  traite les requêtes HTTP (non bloquant)

HTTP GET /state?mode=X
  └── Robot_SetState(BOT_X)
        └── enter_state()  ← propriétés statiques de l'état
        └── prochain Tick  ← propriétés animées
```

### Performances LVGL

LVGL redessine uniquement les zones modifiées (partial render). Pour éviter les ralentissements :
- L'**overlay plein écran** n'est animé qu'en `done` (12 frames) et `alert` (toggle 2×/s)
- En `waiting`, `working`, `juggling`, `sweeping` : uniquement les objets locaux bougent
- Les propriétés statiques (bouche, sourcils, texte) sont écrites une seule fois dans `enter_state()`, pas à chaque tick

### Timer LVGL vs FreeRTOS

Tout tourne sur le core principal sans tâche FreeRTOS. Le timer d'animation est un `lv_timer_t` (50 ms) appelé par `lv_timer_handler()` dans `loop()`. Le serveur HTTP est traité dans le même `loop()` via `handleClient()` (non-bloquant). Aucun mutex nécessaire.

---

## Dépannage

| Symptôme | Cause probable | Solution |
|----------|---------------|---------|
| Erreur "text section exceeds" | Partition trop petite | Sélectionner **Huge APP** dans Arduino IDE |
| Écran bloqué sur "Connexion WiFi..." | SSID/password incorrects | Vérifier `claudebot.h` |
| Robot figé après quelques secondes | `lv_timer_handler()` non appelé | Vérifier que `Timer_Loop()` est dans `loop()` |
| Hooks sans effet sur le robot | IP incorrecte | Lire l'IP affichée au démarrage ou `arp -a \| grep "58:8c:81"` |
| Hooks bloquent Claude Code | `"async": true` manquant | Ajouter le flag dans `settings.json` |
| LED ne s'allume pas | Mauvais GPIO | Vérifier que `LED_PIN 8` correspond à ta carte |
| Bouche invisible ou mauvais sens | Angles LVGL | 0° = Est, sens horaire. Sourire = indicateur en bas (195°→345°) |
| Lag / écran qui rame | Overlay animé trop fréquent | Ne pas animer l'overlay dans les états continus |

---

## Étendre le projet

### Ajouter un état personnalisé

1. Ajouter une valeur dans `bot_state_t` (`claudebot.h`)
2. Ajouter un `case` dans `enter_state()` pour les propriétés statiques (`robot.cpp`)
3. Ajouter une fonction `tick_monétat()` pour les propriétés animées (`robot.cpp`)
4. Ajouter le `case` dans `Robot_Tick()` (`robot.cpp`)
5. Ajouter le parsing du mode HTTP dans `handle_state()` (`claudebot.cpp`)

### Déclencher depuis un script externe

```bash
# Monitoring : alert si service down
ping -c 1 mon-serveur.local > /dev/null 2>&1 \
  && curl -s "http://10.0.0.175/state?mode=idle" \
  || curl -s "http://10.0.0.175/state?mode=alert"

# CI/CD
curl -s "http://10.0.0.175/state?mode=working" && make build \
  && curl -s "http://10.0.0.175/state?mode=done" \
  || curl -s "http://10.0.0.175/state?mode=alert"
```

### Déclencher depuis GitHub Actions

```yaml
- name: ClaudeBot thinking
  run: curl -s "http://10.0.0.175/state?mode=working" || true

- name: Build
  run: make -j4

- name: ClaudeBot done
  run: curl -s "http://10.0.0.175/state?mode=done" || true
```
