/* Console example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "cmd_decl.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"

#ifdef CONFIG_ESP_CONSOLE_USB_CDC
#error This example is incompatible with USB CDC console. Please try "console_usb" example instead.
#endif // CONFIG_ESP_CONSOLE_USB_CDC

static const char *TAG = "main";
#define PROMPT_STR CONFIG_IDF_TARGET

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_STORE_HISTORY

#define MOUNT_PATH "/data"

static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true};
    esp_err_t err = esp_vfs_fat_spiflash_mount(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_STORE_HISTORY

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void vShellInit();
void blink();

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    ESP_LOGI("wifi", "event_handler %s %s %d", __func__, event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
        ESP_LOGI("wifi", "wifi_connect");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI("wifi", "STA Got IP");
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        switch (event_id)
        {
            case IP_EVENT_STA_GOT_IP:   
                ESP_LOGI(TAG, "----> IP_EVENT_STA_GOT_IP ip:");
                ESP_LOGI(TAG, "----> sta ip:" IPSTR, IP2STR(&event->ip_info.ip));
                ESP_LOGI(TAG, "----> mask:" IPSTR, IP2STR(&event->ip_info.netmask));
                ESP_LOGI(TAG, "----> gw:" IPSTR, IP2STR(&event->ip_info.gw));
                break;
            default:
                break;
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI("wifi", "STA Start");

    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        // ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        // switch (event_id)
        // {
        //     case IP_EVENT_STA_GOT_IP:   
        //         ESP_LOGI(TAG, "----> IP_EVENT_STA_GOT_IP ip:");
        //         ESP_LOGI(TAG, "----> sta ip:" IPSTR, IP2STR(&event->ip_info.ip));
        //         ESP_LOGI(TAG, "----> mask:" IPSTR, IP2STR(&event->ip_info.netmask));
        //         ESP_LOGI(TAG, "----> gw:" IPSTR, IP2STR(&event->ip_info.gw));
        //         break;
        //     default:
        //         break;
        // }
    }
}

static void app_prov_event_handler(void* handler_arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "event_handler %s %s %d", __func__, event_base, event_id);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "STA Start");
        /* Once configuration is received through protocomm,
         * device is started as station. Once station starts,
         * wait for connection to establish with configured
         * host SSID and password */
        // esp_wifi_start();
        esp_wifi_connect();
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA Got IP");
        ESP_LOGI(TAG, "----> IP_EVENT_STA_GOT_IP ip:");
        ESP_LOGI(TAG, "----> sta ip:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "----> mask:" IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "----> gw:" IPSTR, IP2STR(&event->ip_info.gw));
        
        /* Station got IP. That means configuration is successful.
         * Schedule timer to stop provisioning app after 30 seconds. */

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGE(TAG, "STA Disconnected");
        /* Station couldn't connect to configured host SSID */

        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGE(TAG, "Disconnect reason : %d", disconnected->reason);
        esp_wifi_connect();
    }
}

void simple_ota_example_task(char* url);


void app_main(void)
{

    initialize_filesystem();
    initialize_nvs();
    ESP_LOGI(TAG, "version:1.0.0");

    vShellInit();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta(); // 创建一个默认得station esp_netif_destroy 这个调用必须在上个后面。。。。不然不能获取ip IP_EVENT_STA_GOT_IP
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_err_t err;

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, app_prov_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler");
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, app_prov_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler");
    }

    wifi_config_t wifi_config;
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    if (strlen((char *)wifi_config.sta.ssid) == 0)
    {
        memset(&wifi_config, 0, sizeof(wifi_config_t));
        strcpy((char *)wifi_config.sta.ssid, "VC5105Golden");
        strcpy((char *)wifi_config.sta.password, "12345678");
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    // esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    
    ESP_LOGI("wifi_init", "ssid:%s,password:%s", (char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password);
    // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, wifi_cfg));

    // xTaskCreate(netmanage, "net", 3 * 1024, NULL, 5, NULL);
    xTaskCreate(blink, "blink", 1 * 1024, NULL, 5, NULL);

}
