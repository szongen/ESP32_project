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
#include "cJSON.h"
#include "E2213JS0C1.h"
#include "esp_http_client.h"
#include "esp_task_wdt.h"
#include "user_main.h"

#ifdef CONFIG_ESP_CONSOLE_USB_CDC
#error This example is incompatible with USB CDC console. Please try "console_usb" example instead.
#endif // CONFIG_ESP_CONSOLE_USB_CDC

static const char *TAG = "main";
#define PROMPT_STR CONFIG_IDF_TARGET

QueueHandle_t QueueHandle_sys,QueueHandle_dis;



variable *handler;
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
        handler->network_ConnetFlag = 1;
        /* Station got IP. That means configuration is successful.
         * Schedule timer to stop provisioning app after 30 seconds. */

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGE(TAG, "STA Disconnected");
        handler->network_ConnetFlag = 0;
        /* Station couldn't connect to configured host SSID */

        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGE(TAG, "Disconnect reason : %d", disconnected->reason);
        esp_wifi_connect();
    }
}

void simple_ota_example_task(char* url);


//画点函数接口
void gfx_draw_pixel(int x, int y, unsigned int rgb)
{
    // OLED_DrawPoint(x,y,rgb);
    if(rgb == 0)
    {
        E2213JS0C1_DrawPoint(x,y,WHITE);
    }
    else
    {
        E2213JS0C1_DrawPoint(x,y,BLACK);
    }

}
//画面函数(未使用)
void gfx_draw_fill(int x, int y,int w, int q, unsigned int rgb)
{	
}
//创建一个函数指针结构体
struct EXTERNAL_GFX_OP
{
	void (*draw_pixel)(int x, int y, unsigned int rgb);
	void (*fill_rect)(int x0, int y0, int x1, int y1, unsigned int rgb);
} my_gfx_op;
extern void startHelloCircle(void* phy_fb, int width, int height, int color_bytes, struct EXTERNAL_GFX_OP* gfx_op);

//设定延时函数接口
void delay_ms(int milli_seconds)
{
    vTaskDelay(milli_seconds/portTICK_PERIOD_MS);
}


//事件回调
static esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    uint32_t ret;
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR: //错误事件
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED: //连接成功事件
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT: //发送头事件
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER: //接收头事件
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
        printf("%.*s", evt->data_len, (char *)evt->data);
        break;
    case HTTP_EVENT_ON_DATA: //接收数据事件
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            printf("%.*s\r\n", evt->data_len, (char *)evt->data);
            ret = xQueueSend(QueueHandle_sys, evt->data, 0);
            if (ret == pdPASS)
            {
                ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA xQueueSend\r\n");
            }
        }

        break;
    case HTTP_EVENT_ON_FINISH: //会话完成事件
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED: //断开事件
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

// http client配置
esp_http_client_config_t config = {
    .method = HTTP_METHOD_POST,                                                                                          // post请求
    .url = "http://api.seniverse.com/v3/weather/now.json?key=SYpTEyjFxwWJdSuAd&location=fujian%20fuzhou&language=zh-Hans&unit=c", //请求url
    .host = "api.seniverse.com",
    .skip_cert_common_name_check = true,
    .keep_alive_enable = false,
    .port = 80,
    .buffer_size = 512,
    .transport_type = HTTP_TRANSPORT_OVER_TCP,
    .event_handler = _http_event_handle, //注册时间回调
};


//测试入口
void http_client_test(void)
{
    int ret;
    char pReadBuf[512];
    esp_http_client_handle_t client = esp_http_client_init(&config); //初始化配置
    esp_err_t err = esp_http_client_perform(client);                 //执行请求

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),     //状态码
                 esp_http_client_get_content_length(client)); //数据长度
        ret = esp_http_client_read(client, pReadBuf, 512);    //读取512数据内容
        if (ret > 0)
        {
            ESP_LOGI(TAG, "recv data = %d %s", ret, pReadBuf); //打印数据
        }
        else
        {
            ESP_LOGI("esp_http_client_perform is failed\r\n", );
        }
    }

    esp_http_client_cleanup(client); //断开并释放资源
}

void cJSON_task()
{
    cJSON *root = NULL;
    cJSON *array = NULL;
    cJSON *item = NULL;
    cJSON *item_location = NULL;
    cJSON *item_now = NULL;
    while (1)
    {
        char buffer[512],temp[27];
        BaseType_t ret;
        ret = xQueueReceive(QueueHandle_sys, buffer, 100);
        if (ret != pdFAIL)
        {
            ESP_LOGI("cJSON_task", "buffer:%s", buffer);
            root = cJSON_Parse(buffer);
            if (!root)
            {
                printf("cJSON_Parse is error Error : [%s]\n", cJSON_GetErrorPtr());
            }
            else
            {
                printf("cJSON_Parse is OK\r\n");
                array = cJSON_GetObjectItem(root, "results");
                item = cJSON_GetArrayItem(array, 0);
                item_location = cJSON_GetObjectItem(item, "location");
                item_now = cJSON_GetObjectItem(item, "now");
                printf("---------------------------------------------------------\r\n");
                ESP_LOGI("sys_var", "temperature:%d", handler->temperature);
                if ((handler->temperature) != atoi((cJSON_GetObjectItem(item_now, "temperature")->valuestring)))
                {
                    char s[20];
                    handler->temperature = atoi((cJSON_GetObjectItem(item_now, "temperature")->valuestring));
                    E2213JS0C1_ShowCharStr(0, 0, "Temper:", FONT_1608, BLACK, WHITE);
                    itoa(handler->temperature, s, 10); 
                    E2213JS0C1_ShowCharStr(7*8, 0, s, FONT_1608, BLACK, WHITE);
                    E2213JS0C1_SendImageData();
                    E2213JS0C1_SendUpdateCmd();
                    E2213JS0C1_TurnOffDCDC();
                    printf("change sys_var\r\n");
                }
                printf("timezone:%s\r\n", cJSON_GetObjectItem(item_location, "timezone")->valuestring);
                printf("name:%s\r\n", cJSON_GetObjectItem(item_location, "name")->valuestring);
                printf("id:%s\r\n", cJSON_GetObjectItem(item_location, "id")->valuestring);
                printf("text:%s\r\n", cJSON_GetObjectItem(item_now, "text")->valuestring);
                printf("temperature:%s\r\n", cJSON_GetObjectItem(item_now, "temperature")->valuestring);
                printf("---------------------------------------------------------\r\n");
            }
            cJSON_Delete(root);
        }
        ret = xQueueReceive(QueueHandle_dis, buffer, 100);
        if (ret != pdFAIL)
        {
            int n = 0; 
            for(int i = 0;i<= strlen(buffer)/26;i++)
            {
                memset(temp,0,sizeof(temp));
                while(buffer[n+i*26]!=NULL && n < 26)
                {
                    temp[n] = buffer[n+i*26];
                    n++;
                }
                temp[n] ='\0';
                E2213JS0C1_ShowCharStr(0, 16+16*i, temp, FONT_1608, BLACK, WHITE);
                n = 0;
            }
                E2213JS0C1_SendImageData();
                E2213JS0C1_SendUpdateCmd();
                E2213JS0C1_TurnOffDCDC();
        }

    }
}


void http_request_task(void *pvparameters)
{
    // ESP_LOGI(TAG, "Start https_request example");

    // https_get_request_using_crt_bundle();
    // // https_get_request_using_cacert_buf();
    // // https_get_request_using_global_ca_store();

    // ESP_LOGI(TAG, "Finish https_request example");
    // vTaskDelete(NULL);
    uint32_t uiNow = 0;
    char buf[512];
    while (1)
    {
        if(handler->network_ConnetFlag == 1)
        {   
            if(uiNow == 0 || (uiNow + 240) <= (xTaskGetTickCount() / 100) )
            {
                http_client_test();

                extern void https_get_request_using_crt_bundle();
                https_get_request_using_crt_bundle();
                uiNow = xTaskGetTickCount() / 100;
                esp_task_wdt_reset();
           }
       }
    }
    
}

void app_main(void)
{
    esp_task_wdt_init(30, true);

    handler = (var)malloc(sizeof(variable));
    handler->temperature = 0;
    handler->network_ConnetFlag = 0;
    memset(handler->content,0,sizeof(handler->content));
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
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    }
    esp_wifi_start();
    ESP_LOGI("wifi_init", "ssid:%s,password:%s", (char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password);
    QueueHandle_sys = xQueueCreate(2, 512);
    QueueHandle_dis = xQueueCreate(2, 512);
    E2213JS0C1_Init();
    E2213JS0C1_ClearFullScreen(WHITE);
    E2213JS0C1_SendImageData();
    E2213JS0C1_SendUpdateCmd();
    E2213JS0C1_TurnOffDCDC();

    // xTaskCreate(netmanage, "net", 3 * 1024, NULL, 5, NULL);

    // my_gfx_op.draw_pixel = gfx_draw_pixel;
    // my_gfx_op.fill_rect = NULL;//gfx_fill_rect; 
    // startHelloCircle(NULL, 128, 64, 1, &my_gfx_op);
    
    

    // extern void https_request_task(void *pvparameters);
    xTaskCreate(&cJSON_task, "cJSON_task", 8 * 1024, NULL, 5, NULL);
    xTaskCreate(blink, "blink", 2 * 1024, NULL, 5, NULL);
    xTaskCreate(&http_request_task, "https_get_task", 16 *1024, NULL, 5, NULL);


}
