#include "proxmox_client.h"
#include "proxmox_ca.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "sdkconfig.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static const char *TAG = "proxmox";

#define RESPONSE_BUF_SIZE 2048

static char s_buf[RESPONSE_BUF_SIZE];
static int  s_buf_len = 0;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int copy = evt->data_len;
        if (s_buf_len + copy >= RESPONSE_BUF_SIZE - 1)
            copy = RESPONSE_BUF_SIZE - 1 - s_buf_len;
        memcpy(s_buf + s_buf_len, evt->data, copy);
        s_buf_len += copy;
        s_buf[s_buf_len] = '\0';
    }
    return ESP_OK;
}

static esp_http_client_handle_t client_init(const char *url, const char *auth)
{
    esp_http_client_config_t cfg = {
        .url                         = url,
        .event_handler               = http_event_handler,
        .cert_pem                    = PROXMOX_CA_PEM,
        .skip_cert_common_name_check = true,
        .transport_type              = HTTP_TRANSPORT_OVER_SSL,
        .timeout_ms                  = 15000,
        .use_global_ca_store         = false,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_http_client_set_header(c, "Authorization", auth);
    return c;
}

static esp_err_t http_get(const char *url, const char *auth)
{
    s_buf_len = 0;
    s_buf[0]  = '\0';
    esp_http_client_handle_t c = client_init(url, auth);
    esp_err_t ret = esp_http_client_perform(c);
    int status    = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);
    if (ret != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "%s → ret=0x%x status=%d", url, ret, status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* Parse /nodes/{node}/status (Proxmox 9) */
static esp_err_t parse_status(const char *json, proxmox_data_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return ESP_FAIL;
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data) { cJSON_Delete(root); return ESP_FAIL; }

    cJSON *cpu    = cJSON_GetObjectItem(data, "cpu");
    cJSON *uptime = cJSON_GetObjectItem(data, "uptime");
    cJSON *memory = cJSON_GetObjectItem(data, "memory");

    if (!cpu || !uptime || !memory) { cJSON_Delete(root); return ESP_FAIL; }

    out->cpu    = (float)cJSON_GetNumberValue(cpu);
    out->uptime = (uint32_t)cJSON_GetNumberValue(uptime);
    out->mem    = (uint64_t)cJSON_GetNumberValue(cJSON_GetObjectItem(memory, "used"));
    out->maxmem = (uint64_t)cJSON_GetNumberValue(cJSON_GetObjectItem(memory, "total"));

    cJSON_Delete(root);
    return ESP_OK;
}

static uint64_t cjson_to_u64(cJSON *item)
{
    if (!item) return 0;
    if (cJSON_IsNumber(item)) return (uint64_t)item->valuedouble;
    if (cJSON_IsString(item) && item->valuestring) return strtoull(item->valuestring, NULL, 10);
    return 0;
}

/* Parse /nodes/{node}/netstat — somme de tous les in/out de VMs */
static esp_err_t parse_netstat(const char *json, proxmox_data_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return ESP_FAIL;
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data) { cJSON_Delete(root); return ESP_FAIL; }

    uint64_t total_in = 0, total_out = 0;
    cJSON *entry;
    cJSON_ArrayForEach(entry, data) {
        total_in  += cjson_to_u64(cJSON_GetObjectItem(entry, "in"));
        total_out += cjson_to_u64(cJSON_GetObjectItem(entry, "out"));
    }
    out->netin  = total_in;
    out->netout = total_out;

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t proxmox_client_init(void) { return ESP_OK; }

esp_err_t proxmox_client_fetch(proxmox_data_t *out)
{
    char url[160];
    char auth[192];
    snprintf(auth, sizeof(auth), "PVEAPIToken=%s", CONFIG_PROXMOX_API_TOKEN);

    /* 1. /status → cpu, memory, uptime */
    snprintf(url, sizeof(url), "https://%s:%d/api2/json/nodes/%s/status",
             CONFIG_PROXMOX_HOST, CONFIG_PROXMOX_PORT, CONFIG_PROXMOX_NODE);
    if (http_get(url, auth) != ESP_OK) return ESP_FAIL;
    if (parse_status(s_buf, out) != ESP_OK) {
        ESP_LOGE(TAG, "parse_status failed");
        return ESP_FAIL;
    }

    /* 2. /netstat → netin, netout (cumulatifs par VM, sommés) */
    snprintf(url, sizeof(url), "https://%s:%d/api2/json/nodes/%s/netstat",
             CONFIG_PROXMOX_HOST, CONFIG_PROXMOX_PORT, CONFIG_PROXMOX_NODE);
    if (http_get(url, auth) != ESP_OK) return ESP_FAIL;
    if (parse_netstat(s_buf, out) != ESP_OK) {
        ESP_LOGE(TAG, "parse_netstat failed");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "cpu=%.1f%% mem=%lluMB/%lluMB uptime=%lus",
             out->cpu * 100.0f,
             out->mem    / (1024*1024),
             out->maxmem / (1024*1024),
             (unsigned long)out->uptime);
    return ESP_OK;
}
