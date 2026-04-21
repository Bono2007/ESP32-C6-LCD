# ESP32-C6 Proxmox Dashboard

Dashboard temps réel sur écran LCD 1.47" (ST7789, 172×320 px) affichant les métriques d'un serveur Proxmox via son API REST HTTPS.

![Board](https://spotpear.com/wiki/images/thumb/e/e3/ESP32-C6-1.47-LCD-1.jpg/400px-ESP32-C6-1.47-LCD-1.jpg)

## Matériel requis

| Composant | Référence |
|-----------|-----------|
| Microcontrôleur | Spotpear ESP32-C6 LCD 1.47" (ST7789V3, 172×320) |
| Connexion | USB-C (flashing + alimentation) |

> **Note GPIO** : CLK=GPIO7, MOSI=GPIO6, CS=GPIO14, DC=GPIO15, RST=GPIO21, BL=GPIO22.
> Ces broches correspondent au câblage physique du PCB Spotpear — **ne pas les inverser**.

## Fonctionnalités

- CPU Proxmox (%)
- RAM utilisée / totale (Go)
- Trafic réseau TX/RX (Mbps, calculé entre deux fetches)
- Uptime du nœud (jours/heures/minutes)
- Indicateur de connexion et horodatage de la dernière mise à jour
- Rafraîchissement configurable (défaut : 15 s)

## Prérequis logiciels

### 1. Dépendances système (macOS)

```bash
brew install cmake ninja dfu-util python3
```

### 2. ESP-IDF v5.4.1

```bash
mkdir -p ~/esp
cd ~/esp
git clone --recursive --branch v5.4.1 https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c6
```

Ajouter à `~/.zshrc` (ou lancer manuellement avant chaque session) :

```bash
source ~/esp/esp-idf/export.sh
```

### 3. Cloner ce dépôt

```bash
git clone https://github.com/Bono2007/ESP32-C6-LCD.git
cd ESP32-C6-LCD
```

## Configuration

### Créer `sdkconfig.defaults.local` (jamais commité)

Ce fichier contient les credentials. Il n'est **pas** versionné (voir `.gitignore`).

```ini
CONFIG_WIFI_SSID="NomDeTonReseau"
CONFIG_WIFI_PASSWORD="MotDePasse"
CONFIG_PROXMOX_HOST="192.168.1.10"
CONFIG_PROXMOX_PORT=8006
CONFIG_PROXMOX_NODE="pve"
CONFIG_PROXMOX_API_TOKEN="root@pam!tokenid=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
CONFIG_REFRESH_INTERVAL_MS=15000
```

### Créer un token API Proxmox

Dans l'interface web Proxmox : **Datacenter → API Tokens → Add**

- User : `root@pam` (ou un utilisateur dédié)
- Token ID : `esp32` (par exemple)
- Privilege Separation : **désactivé** (sinon ajouter les droits manuellement)

Le token s'affiche une seule fois : `root@pam!esp32=xxxxxxxx-...`

### Patcher le sdkconfig avec les credentials

ESP-IDF ne charge pas automatiquement `sdkconfig.defaults.local`. Appliquer les valeurs manuellement :

```bash
source ~/esp/esp-idf/export.sh
idf.py set-target esp32c6   # génère sdkconfig initial

# Appliquer les credentials depuis .local
while IFS='=' read -r key val; do
  [[ "$key" =~ ^# ]] && continue
  [[ -z "$key" ]] && continue
  val="${val%\"}"
  val="${val#\"}"
  sed -i '' "s|${key}=.*|${key}=\"${val}\"|" sdkconfig 2>/dev/null || true
done < sdkconfig.defaults.local

# Les valeurs numériques (PORT, INTERVAL) n'ont pas de guillemets
sed -i '' 's/CONFIG_PROXMOX_PORT="8006"/CONFIG_PROXMOX_PORT=8006/' sdkconfig
sed -i '' 's/CONFIG_REFRESH_INTERVAL_MS="15000"/CONFIG_REFRESH_INTERVAL_MS=15000/' sdkconfig
```

## Build, Flash, Monitor

```bash
source ~/esp/esp-idf/export.sh

idf.py build
idf.py -p /dev/cu.usbmodem1101 flash

# Lire les logs série (adapter le port)
python3 -c "
import serial, time
s = serial.Serial('/dev/cu.usbmodem1101', 115200, timeout=3)
end = time.time() + 60
while time.time() < end:
    line = s.readline().decode('utf-8', errors='replace').rstrip()
    if line: print(line)
s.close()
"
```

> Trouver le port USB : `ls /dev/cu.*`

## Architecture du code

```
main/
├── main.c              # app_main : orchestration des tâches FreeRTOS
├── display_driver.c/h  # Init SPI + ST7789 + LVGL
├── dashboard_ui.c/h    # Widgets LVGL (CPU, RAM, réseau, uptime)
├── proxmox_client.c/h  # Requête HTTPS API Proxmox + parsing JSON
├── wifi_manager.c/h    # Connexion WiFi STA avec scan diagnostic
├── proxmox_ca.h        # Certificat auto-signé embarqué (à regénérer si expiré)
├── lv_conf.h           # Configuration LVGL (résolution, couleurs, fonts)
├── Kconfig.projbuild   # Paramètres menuconfig (GPIO, credentials, intervalle)
└── idf_component.yml   # Dépendances managed components (lvgl ~8.3)
```

### Tâches FreeRTOS

| Tâche | Priorité | Stack | Rôle |
|-------|----------|-------|------|
| `lvgl_task` | 5 | 4096 | `lv_timer_handler()` toutes les 5 ms |
| `ui_update_task` | 3 | 4096 | Met à jour les widgets depuis la queue |
| `fetch_task` | 3 | 8192 | Requête HTTPS Proxmox toutes les N secondes |

### Flux de données

```
fetch_task ──HTTP──▶ proxmox_client_fetch()
                          │ JSON parsing
                          ▼
                     QueueOverwrite(s_data_queue)
                          │
ui_update_task ◀──────────┘
      │ dashboard_ui_update()
      ▼
   LVGL widgets
      │ flush_cb (DMA SPI)
      ▼
   ST7789 LCD
```

## Paramètres LVGL et affichage

- Résolution : 172×320 px (LCD_H_RES × LCD_V_RES dans `display_driver.h`)
- Double buffer : 172×20 px × 2 (dans IRAM)
- Police : Montserrat 12, 16, 24 (activées dans `sdkconfig.defaults`)
- Inversion couleur active (`esp_lcd_panel_invert_color(true)`) — propre au ST7789V3
- Offset colonne : 34 px (`esp_lcd_panel_set_gap(34, 0)`) — contrôleur interne 240 px pour un panel 172 px

## Certificat Proxmox (TLS)

Proxmox utilise un certificat auto-signé. Le certificat est embarqué dans `proxmox_ca.h`.

**Renouveler le certificat** (si Proxmox est régénéré ou à l'expiration) :

```bash
python3 -c "
import ssl, socket, base64
ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE
with socket.create_connection(('IP_PROXMOX', 8006)) as s:
    with ctx.wrap_socket(s) as ss:
        der = ss.getpeercert(binary_form=True)
        pem = '-----BEGIN CERTIFICATE-----\n' + base64.encodebytes(der).decode() + '-----END CERTIFICATE-----'
        print(pem)
"
```

Copier le résultat dans `main/proxmox_ca.h` entre les guillemets de `PROXMOX_CA_PEM`.

## Adapter à un autre projet

Ce projet est une base réutilisable. Pour l'adapter :

1. **Autre source de données** : remplacer `proxmox_client.c` par n'importe quel client HTTP ESP-IDF. La `ui_data_t` dans `dashboard_ui.h` définit le contrat avec l'UI.

2. **Autre dashboard** : modifier `dashboard_ui.c` — les widgets LVGL sont créés dans `dashboard_ui_init()` et mis à jour dans `dashboard_ui_update()`.

3. **Autre board ST7789** : vérifier les GPIO dans `Kconfig.projbuild` (defaults) et ajuster `LCD_H_RES`/`LCD_V_RES` + l'offset `set_gap()` selon le panneau.

4. **LVGL v9** : l'API de `lv_disp_draw_buf` a changé en v9 — mettre à jour `idf_component.yml` (`version: "~9.0.0"`) et adapter `display_driver.c`.

## Dépannage

| Symptôme | Cause probable | Solution |
|----------|---------------|----------|
| Écran noir avec backlight | CLK/MOSI inversés | Vérifier GPIO7=CLK, GPIO6=MOSI |
| Couleurs inversées | Inversion ST7789 | `esp_lcd_panel_invert_color(true/false)` |
| Image décalée | Offset colonne incorrect | Ajuster `set_gap(34, 0)` |
| Fetch échoue | Certificat TLS expiré | Regénérer `proxmox_ca.h` |
| Fetch échoue | Isolation WiFi client | Vérifier que le router autorise la communication entre clients |
| `CONFIG_LCD_*` indéfini | `Kconfig.projbuild` au mauvais endroit | Doit être dans `main/`, pas à la racine |
| Binaire trop grand | Partition factory trop petite | Ajuster `partitions.csv` |

## Licence

MIT
