# BandWatch — Analyseur de spectre Wi-Fi 2.4 GHz

Application Arduino pour ESP32-C6 LCD 1.47". Capture en mode promiscuous avec hopping sur les 13 canaux 2.4 GHz, score d'activité logarithmique et affichage des 3 canaux les plus chargés.

## Fonctionnalités

- Hopping automatique sur les canaux 1 à 13 (260 ms par canal)
- Mesures par canal : paquets/s, bytes/s, ratio signal fort, appareils uniques
- Score d'activité pondéré (logarithmique) :  40% pkt/s, 30% bytes/s, 20% RSSI fort, 10% TX uniques
- LED RGB WS2812 : vert (calme) → rouge (saturé)
- Flash blanc au démarrage (self-test LED)
- Rafraîchissement UI toutes les 120 ms

## Librairies requises

| Librairie | Gestionnaire |
|-----------|-------------|
| lvgl 9.x | Arduino Library Manager |
| Adafruit NeoPixel | Arduino Library Manager |

## Utilisation

1. Ouvrir `bandwatch.ino` dans Arduino IDE
2. Sélectionner **ESP32C6 Dev Module**
3. Installer les librairies ci-dessus
4. Compiler et flasher

## Affichage LVGL

```
╔══════════════════╗
║ BandWatch 2.4 GHz║
║  Activité globale║
║  ██████████░░░░  ║
║     Top 3 canaux ║
║ Ch  1  342 pkts  ║
║ ████████░░░      ║
║ Ch  6  198 pkts  ║
║ █████░░░░░       ║
║ Ch 11   87 pkts  ║
║ ███░░░░░░        ║
╚══════════════════╝
```

## Note légale

L'utilisation du mode promiscuous pour capturer du trafic réseau est réglementée. N'utiliser que sur des réseaux dont vous êtes propriétaire ou pour lesquels vous avez une autorisation explicite.
