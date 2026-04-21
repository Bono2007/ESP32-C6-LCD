#include "unity.h"
#include "proxmox_client.h"

static const char *MOCK_JSON =
    "{\"data\":{"
    "\"cpu\":0.67,"
    "\"mem\":13314744320,"
    "\"maxmem\":34359738368,"
    "\"netout\":2411724800,"
    "\"netin\":8715444224,"
    "\"uptime\":1234567"
    "}}";

TEST_CASE("parse: cpu correct", "[proxmox]")
{
    proxmox_data_t d;
    TEST_ASSERT_EQUAL(ESP_OK, proxmox_parse_response(MOCK_JSON, &d));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.67f, d.cpu);
}

TEST_CASE("parse: mem correct", "[proxmox]")
{
    proxmox_data_t d;
    proxmox_parse_response(MOCK_JSON, &d);
    TEST_ASSERT_EQUAL_UINT64(13314744320ULL, d.mem);
    TEST_ASSERT_EQUAL_UINT64(34359738368ULL, d.maxmem);
}

TEST_CASE("parse: réseau correct", "[proxmox]")
{
    proxmox_data_t d;
    proxmox_parse_response(MOCK_JSON, &d);
    TEST_ASSERT_EQUAL_UINT64(2411724800ULL, d.netout);
    TEST_ASSERT_EQUAL_UINT64(8715444224ULL, d.netin);
}

TEST_CASE("parse: uptime correct", "[proxmox]")
{
    proxmox_data_t d;
    proxmox_parse_response(MOCK_JSON, &d);
    TEST_ASSERT_EQUAL_UINT32(1234567, d.uptime);
}

TEST_CASE("parse: json invalide retourne ESP_FAIL", "[proxmox]")
{
    proxmox_data_t d;
    TEST_ASSERT_EQUAL(ESP_FAIL, proxmox_parse_response("{invalid}", &d));
}

void app_main(void)
{
    unity_run_menu();
}
