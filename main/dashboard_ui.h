#pragma once

#include "proxmox_client.h"

/**
 * @brief Create and initialize the LVGL dashboard layout.
 */
void dashboard_ui_init(void);

/**
 * @brief Refresh all dashboard widgets with fresh data.
 *
 * @param status  Latest node status from Proxmox.
 */
void dashboard_ui_update(const proxmox_node_status_t *status);
