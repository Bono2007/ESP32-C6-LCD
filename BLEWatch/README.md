# BLEWatch — Scanner BLE avec détection de vulnérabilités OUI

Application Arduino pour ESP32-C6 LCD 1.47". Scan continu des annonces BLE, classification par proximité RSSI, et vérification des préfixes MAC constructeur contre une liste de puces ayant des CVEs documentés.

## Fonctionnalités

- Détection jusqu'à 64 appareils BLE simultanément
- 5 niveaux de proximité basés sur le RSSI (TRÈS PROCHE → TRÈS LOIN)
- Vérification OUI après 3 secondes à portée TRÈS PROCHE (39 constructeurs référencés)
- LED RGB WS2812 : couleur selon la proximité (vert → cyan → bleu → rouge si vulnérable)
- Sélection "sticky" : évite le scintillement de l'affichage en restant sur un appareil sauf si un autre est bien plus fort (+10 dB)
- Purge automatique des appareils non vus depuis 10 secondes

## Librairies requises

| Librairie | Gestionnaire |
|-----------|-------------|
| lvgl 9.x | Arduino Library Manager |
| NimBLE-Arduino | Arduino Library Manager |
| Adafruit NeoPixel | Arduino Library Manager |

## Utilisation

1. Ouvrir `blewatch.ino` dans Arduino IDE
2. Sélectionner **ESP32C6 Dev Module**
3. Installer les librairies ci-dessus
4. Compiler et flasher

## Affichage LVGL

```
╔══════════════╗
║   BLEWatch   ║
║ Appareils: 4 ║
║ ████████░░░░ ║  ← barre RSSI
║   TRÈS PROCHE║
║ Pixel Watch  ║
║ AA:BB:CC:... ║
║ RSSI: -38dBm ║
║              ║
║ ! OUI VULNÉR!║  ← si détecté
╚══════════════╝
```

## Avertissement sécurité

L'indicateur rouge signifie que le **constructeur** de la puce a livré du firmware vulnérable historiquement — l'appareil spécifique a peut-être reçu un patch. Les faux positifs incluent notamment le Flipper Zero (puce STMicroelectronics avec mises à jour de sécurité actives).
