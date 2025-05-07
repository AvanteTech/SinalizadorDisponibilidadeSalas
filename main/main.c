#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "include/esp_lcd.h"
#include "wifi_connect.h"
#include "sntp_time.h"
#include "esp_sntp.h"

lcd_t lcd;
char *message = NULL;
char status = ' ';
char status_ready = 'D';
int payload_index = 0;
int page = 0;

void ble_store_config_init(void);

static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);

static void on_stack_reset(int reason)
{
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

static void on_stack_sync(void)
{
    adv_init();
}

static void nimble_host_config_init(void)
{
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_store_config_init();
}

static void nimble_host_task(void *param)
{
    ESP_LOGI(TAG, "nimble host task has been started!");

    nimble_port_run();

    vTaskDelete(NULL);
}

void get_full_datetime(char *datetime_str)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    strftime(datetime_str, 20, "%d/%m/%Y-%H:%M", &timeinfo);
}
char *get_scheduled_message()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int hour = timeinfo.tm_hour;
    int min = timeinfo.tm_min;

    // Verifica os intervalos
    if ((hour == 8 && min >= 0) || (hour == 9) || (hour == 10 && min < 20))
    {
        status_ready = 'O';
        return " Em expediente  ";
    }
    else if ((hour == 10 && min >= 0 && min < 20))
    {
        status_ready = 'D';
        return "   Intervalo    ";
    }
    else if ((hour == 10 && min >= 20) || (hour == 11) || (hour == 12 && min < 0))
    {
        status_ready = 'O';
        return " Em expediente  ";
    }
    else if ((hour == 15 && min >= 0 && min < 20))
    {
        status_ready = 'D';
        return "   Intervalo    ";
    }
    else if ((hour >= 13 && hour < 15) || (hour == 15 && min >= 20) || (hour >= 16 && hour < 17))
    {
        status_ready = 'O';
        return " Em expediente  ";
    }
    else if ((hour >= 18 && hour < 20))
    {
        status_ready = 'O';
        return " Em expediente  ";
    }
    else if ((hour == 20 && min >= 0 && min < 20))
    {
        status_ready = 'D';
        return "   Intervalo    ";
    }
    else if ((hour >= 20 && min >= 20) || (hour >= 21 && hour < 22))
    {
        status_ready = 'O';
        return " Em expediente  ";
    }
    else
    {
        status_ready = 'D';
    }

    return NULL;
}

void lcd_task(void *pvParameters)
{
    char time_date[20];
    char *message_ready = NULL;
    char status_to_show = ' ';
    char *message_to_show = NULL;

    while (1)
    {
        get_full_datetime(time_date);
        message_ready = get_scheduled_message();
        status_to_show = (status != ' ') ? status : status_ready;
        message_to_show = (message != NULL) ? message : message_ready;

        lcdClear(&lcd);
        vTaskDelay(2 / portTICK_PERIOD_MS);
        lcdSetText(&lcd, " AvanteTech Jr. ", 0, 0);
        lcdSetText(&lcd, time_date, 0, 1);
        vTaskDelay(1500 / portTICK_PERIOD_MS);

        lcdClear(&lcd);
        vTaskDelay(2 / portTICK_PERIOD_MS);
        lcdSetText(&lcd, "     Status     ", 0, 0);
        if (status_to_show == 'O')
        {
            lcdSetText(&lcd, "    Ocupada    ", 0, 1);
        }
        else if (status_to_show == 'D')
        {
            lcdSetText(&lcd, "   Desocupada   ", 0, 1);
        }

        vTaskDelay(1500 / portTICK_PERIOD_MS);
        if (message_to_show != NULL)
        {
            lcdClear(&lcd);
            vTaskDelay(2 / portTICK_PERIOD_MS);
            lcdSetText(&lcd, "   Mensagem:    ", 0, 0);
            if (strlen(message_to_show) > 16)
            {
                lcdScrollText(&lcd, message_to_show, 1, LCD_SCROLL_LEFT, 300);
            }
            else
            {
                lcdSetText(&lcd, message_to_show, 0, 1);
                vTaskDelay(1500 / portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    int rc;
    esp_err_t ret;

    lcdDefault(&lcd);
    lcdInit(&lcd);
    ESP_LOGI(TAG_LCD, "Initialized successfully.");

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    rc = gap_init();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    rc = gatt_svc_init();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d", rc);
        return;
    }

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_connect_init("AvanteTech_WIFI", "@@avante2025@@");

    sntp_time_init();
    lcdClear(&lcd);
    vTaskDelay(2 / portTICK_PERIOD_MS);
    lcdSetText(&lcd, " CONECTANDO...  ", 0, 0);
    sntp_wait_for_sync();

    print_current_time();

    nimble_host_config_init();

    xTaskCreate(lcd_task, "LCD", 4 * 1024, NULL, 5, NULL);
    xTaskCreate(nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);

    return;
}
