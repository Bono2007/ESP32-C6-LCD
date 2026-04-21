#include "proxmox_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "sdkconfig.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "proxmox";

#define RESPONSE_BUF_SIZE 2048

esp_err_t proxmox_parse_response(const char *json, proxmox_data_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return ESP_FAIL;

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data) { cJSON_Delete(root); return ESP_FAIL; }

    cJSON *cpu    = cJSON_GetObjectItem(data, "cpu");
    cJSON *mem    = cJSON_GetObjectItem(data, "mem");
    cJSON *maxmem = cJSON_GetObjectItem(data, "maxmem");
    cJSON *netout = cJSON_GetObjectItem(data, "netout");
    cJSON *netin  = cJSON_GetObjectItem(data, "netin");
    cJSON *uptime = cJSON_GetObjectItem(data, "uptime");

    if (!cpu || !mem || !maxmem || !netout || !netin || !uptime) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    out->cpu    = (float)cJSON_GetNumberValue(cpu);
    out->mem    = (uint64_t)cJSON_GetNumberValue(mem);
    out->maxmem = (uint64_t)cJSON_GetNumberValue(maxmem);
    out->netout = (uint64_t)cJSON_GetNumberValue(netout);
    out->netin  = (uint64_t)cJSON_GetNumberValue(netin);
    out->uptime = (uint32_t)cJSON_GetNumberValue(uptime);

    cJSON_Delete(root);
    return ESP_OK;
}

static char s_response_buf[RESPONSE_BUF_SIZE];
static int  s_response_len = 0;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int copy = evt->data_len;
        if (s_response_len + copy >= RESPONSE_BUF_SIZE - 1)
            copy = RESPONSE_BUF_SIZE - 1 - s_response_len;
        memcpy(s_response_buf + s_response_len, evt->data, copy);
        s_response_len += copy;
        s_response_buf[s_response_len] = '\0';
    }
    return ESP_OK;
}

esp_err_t proxmox_client_init(void) { return ESP_OK; }

esp_err_t proxmox_client_fetch(proxmox_data_t *out)
{
    char url[128];
    snprintf(url, sizeof(url), "https://%s:%d/api2/json/nodes/%s/status",
             CONFIG_PROXMOX_HOST, CONFIG_PROXMOX_PORT, CONFIG_PROXMOX_NODE);

    char auth_header[192];
    snprintf(auth_header, sizeof(auth_header),
             "PVEAPIToken=%s", CONFIG_PROXMOX_API_TOKEN);

    s_response_len = 0;
    memset(s_response_buf, 0, sizeof(s_response_buf));

    esp_http_client_config_t cfg = {
        .url                         = url,
        .event_handler               = http_event_handler,
        .skip_cert_common_name_check = true,
        .transport_type              = HTTP_TRANSPORT_OVER_SSL,
        .timeout_ms                  = 15000,
        .crt_bundle_attach           = NULL,
        .use_global_ca_store         = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Authorization", auth_header);

    ESP_LOGI(TAG, "Fetch URL: %s", url);
    esp_err_t ret = esp_http_client_perform(client);
    int status    = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (ret != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "HTTP error: ret=0x%x (%s) status=%d", ret, esp_err_to_name(ret), status);
        return ESP_FAIL;
    }

    return proxmox_parse_response(s_response_buf, out);
}
