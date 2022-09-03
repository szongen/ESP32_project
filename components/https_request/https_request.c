/* HTTPS GET Example using plain mbedTLS sockets
 *
 * Contacts the howsmyssl.com API via TLS v1.2 and reads a JSON
 * response.
 *
 * Adapted from the ssl_client1 example in mbedtls.
 *
 * Original Copyright (C) 2006-2016, ARM Limited, All Rights Reserved, Apache 2.0 License.
 * Additions Copyright (C) Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD, Apache 2.0 License.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_tls.h"
#include "esp_crt_bundle.h"

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "apiv3.shanbay.com"
#define WEB_PORT "443"
#define WEB_URL "https://apiv3.shanbay.com/weapps/dailyquote/quote/"

static const char *TAG = "example";

static const char REQUEST[] = "GET " WEB_URL " HTTP/1.1\r\n"
                             "Host: "WEB_SERVER"\r\n"
                             "User-Agent: esp-idf/1.0 esp32\r\n"
                             "\r\n";


extern QueueHandle_t QueueHandle_sys,QueueHandle_dis;
/* Root cert for howsmyssl.com, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
void https_get_request(esp_tls_cfg_t cfg)
{
    char buf[512];
    int ret, len;
    int i= 0, num = 0,l1 = 0;
    struct esp_tls *tls = esp_tls_conn_http_new(WEB_URL, &cfg);

    if (tls != NULL) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        goto exit;
    }

    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls,
                                 REQUEST + written_bytes,
                                 sizeof(REQUEST) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto exit;
        }
    } while (written_bytes < sizeof(REQUEST));

    ESP_LOGI(TAG, "Reading HTTP response...");
    char buffer[3000],buffer1[2000];
    memset(buffer,0,sizeof(buffer));
    do {
        len = sizeof(buf) - 1;
        bzero(buf, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ) {
            continue;
        }

        if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        }

        if (ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        len = ret;
        ESP_LOGI(TAG, "%d bytes read", len);
        /* Print response directly to stdout as it is read */
        for (int i = 0; i < len; i++) {
            putchar(buf[i]);
        }
        strcat(buffer,buf);
    } while (1);
    printf("*************************************************************\n");
    printf("buffer:%s\n",buffer);
    printf("buffer len:%d\n",strlen(buffer));
    printf("*************************************************************\n");
    while (buffer[i]!=NULL)
    {
        if(buffer[i++] =='\n')
        {
            num++;
            if(num == 7)
            {
                l1 = i;
            }
        }
    }
    int ii = 0;
    memset(buffer1,0,sizeof(buffer1));
    while (buffer[ii+l1] != NULL ||ii+l1 != strlen(buffer))
    {
        i = ii;
        if(ii <= 2000)
        {
            buffer1[ii++] = buffer[i + l1];
        }
        else
        {
            printf("buffer1 is not enough");
            break;
        }
    }
    printf("*************************************************************\n");
    printf("buffer1:%s\n",buffer1);
    printf("buffer1 strlen %d\n",strlen(buffer1));
    printf("*************************************************************\n");
    cJSON *root = NULL;
    root = cJSON_Parse(buffer1);
    if (!root)
    {
        printf("cJSON_Parse is error Error : [%s]\n", cJSON_GetErrorPtr());
    }
    else
    {
        printf("cJSON_Parse is OK\r\n");
        // snprintf(handler->content,sizeof(handler->content),"%s",cJSON_GetObjectItem(root, "content")->valuestring);
        printf("motto:%s\n",cJSON_GetObjectItem(root, "content")->valuestring);
        xQueueSend(QueueHandle_dis, cJSON_GetObjectItem(root, "content")->valuestring, 0);
    }
    cJSON_Delete(root);
    
exit:
    esp_tls_conn_delete(tls);
}

void https_get_request_using_crt_bundle(void)
{
    ESP_LOGI(TAG, "https_request using crt bundle");
    esp_tls_cfg_t cfg = {
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    https_get_request(cfg);
}


void https_request_task(void *pvparameters)
{
    ESP_LOGI(TAG, "Start https_request example");

    // https_get_request_using_crt_bundle();
    // https_get_request_using_cacert_buf();
    // https_get_request_using_global_ca_store();

    ESP_LOGI(TAG, "Finish https_request example");
    // vTaskDelete(NULL);
}

// void app_main(void)
// {
//     ESP_ERROR_CHECK( nvs_flash_init() );
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());

//     /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
//      * Read "Establishing Wi-Fi or Ethernet Connection" section in
//      * examples/protocols/README.md for more information about this function.
//      */

//     xTaskCreate(&https_request_task, "https_get_task", 8192, NULL, 5, NULL);
// }
