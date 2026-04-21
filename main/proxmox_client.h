#pragma once

#include <stdint.h>

typedef struct {
    float   cpu_percent;
    float   mem_percent;
    uint64_t net_tx_bytes;
    uint64_t net_rx_bytes;
    uint64_t uptime_seconds;
} proxmox_node_status_t;

/**
 * @brief Fetch node status from the Proxmox API.
 *
 * @param[out] status  Populated on success.
 * @return             0 on success, non-zero on error.
 */
int proxmox_client_get_status(proxmox_node_status_t *status);
