#pragma once
#include "esp_err.h"
#include <stdint.h>

typedef struct {
    float    cpu;     /* 0.0–1.0 */
    uint64_t mem;     /* bytes utilisés */
    uint64_t maxmem;  /* bytes totaux */
    uint64_t netout;  /* TX cumulatif en bytes — caller doit diff pour obtenir le débit */
    uint64_t netin;   /* RX cumulatif en bytes — caller doit diff pour obtenir le débit */
    uint32_t uptime;  /* secondes */
} proxmox_data_t;

esp_err_t proxmox_parse_response(const char *json, proxmox_data_t *out);
esp_err_t proxmox_client_init(void);
esp_err_t proxmox_client_fetch(proxmox_data_t *out);
