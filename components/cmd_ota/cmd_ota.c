#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "cmd_ota.h"


static struct {
    struct arg_str *url;
    struct arg_end *end;
} ota_args;

void simple_ota_example_task(void * url);
static int ota(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &ota_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ota_args.end, argv[0]);
        return 1;
    }
    ESP_LOGI(__func__, "ota url is '%s'",
             ota_args.url->sval[0]);
    simple_ota_example_task(ota_args.url->sval[0]);
    /* set default value*/
    // if (ota_args.timeout->count == 0) {
    //     ota_args.timeout->ival[0] = JOIN_TIMEOUT_MS;
    // }

    // bool connected = wifi_join(join_args.ssid->sval[0],
    //                            join_args.password->sval[0],
    //                            join_args.timeout->ival[0]);
    // if (!connected) {
    //     ESP_LOGW(__func__, "Connection timed out");
    //     return 1;
    // }
    ESP_LOGI(__func__, "ota");
    return 0;
}


void register_ota(void)
{
    ota_args.url = arg_str1(NULL, NULL, "<url>", "ota url");
    ota_args.end = arg_end(1);

    const esp_console_cmd_t ota_cmd = {
        .command = "ota",
        .help = "ota <url>",
        .hint = NULL,
        .func = &ota,
        .argtable = &ota_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&ota_cmd) );
}