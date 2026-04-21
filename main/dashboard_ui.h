#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float    cpu_pct;
    float    mem_gb;
    float    maxmem_gb;
    float    tx_mbps;
    float    rx_mbps;
    uint32_t uptime_s;
    bool     connected;
    uint32_t last_update_s;
} ui_data_t;

void dashboard_ui_init(void);
void dashboard_ui_update(const ui_data_t *data);
