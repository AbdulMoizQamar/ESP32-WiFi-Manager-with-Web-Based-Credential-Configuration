#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "nvs_flash.h"
#include "esp_err.h"

void app_main(void)
{
    esp_err_t ret = nvs_flash_init(); //esp_err_t --- error code type hai jo batata hai k func succes howa ya error arha hai

    // Agar NVS corrupt ho ya old version ho toh usay erase karke dobara init karo
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("APP", "NVS corrupted or old version, erasing and re-initializing..."); 
        ESP_ERROR_CHECK(nvs_flash_erase()); // NVS flash ko erase karte hain
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI("APP", "Starting Wi-Fi Manager...");
    wifi_manager_init();
}

//nvs is non volatile storage memory
//flash memory --- non volatile type memory means power off hone k bd bhi data save rhy
//RAM temporary hai, flash permenant
//flash is liye kiya ta k device reboot hone k bd important data yd rkhy jaise wifi credentials

/*#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WiFiManager";

const char *html_page =
"<!DOCTYPE html><html><body><h2>ESP32 WiFi Manager</h2>"
"<form action=\"/connect\" method=\"post\">"
"SSID: <input name=\"ssid\"><br>"
"Password: <input name=\"password\" type=\"password\"><br>"
"<input type=\"submit\" value=\"Connect\">"
"</form></body></html>";

esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t connect_post_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) return ESP_FAIL;

    char ssid[32] = {0}, pass[32] = {0};
    sscanf(buf, "ssid=%31[^&]&password=%31s", ssid, pass);

    ESP_LOGI(TAG, "SSID: %s | PASSWORD: %s", ssid, pass);

    wifi_config_t wifi_config = {0};
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, pass);

    esp_wifi_disconnect();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect();

    httpd_resp_sendstr(req, "Connecting to WiFi...");
    return ESP_OK;
}

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    httpd_start(&server, &config);

    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler
    };

    httpd_uri_t uri_connect = {
        .uri = "/connect",
        .method = HTTP_POST,
        .handler = connect_post_handler
    };

    httpd_register_uri_handler(server, &uri_root);
    httpd_register_uri_handler(server, &uri_connect);
}

void wifi_init_softap() {
    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP32_Config",
            .ssid_len = strlen("ESP32_Config"),
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "AP started. Connect to ESP32_Config");
}

void app_main(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_softap();
    start_webserver();
}*/
