#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "include/esp_lcd.h"

lcd_t lcd;

char *message = NULL;
char status = 'D';
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

void lcd_task(void *pvParameters) {
    while (1) {
        lcdClear(&lcd);
        int totalPages = 2; 
        if (message != NULL && strlen(message) > 0) {
            totalPages += (strlen(message) + 15) / 16; 
        }

        if (page == 0) {
            lcdSetText(&lcd, "   Escritorio   ", 0, 0);
            lcdSetText(&lcd, "   AvanteTech   ", 0, 1);
        } else if (page == 1) {
            lcdSetText(&lcd, "     Status     ", 0, 0);
            if (status == 'O') {
                lcdSetText(&lcd, "     Ocupado    ", 0, 1);
            } else if (status == 'D') {
                lcdSetText(&lcd, "   Disponivel   ", 0, 1);
            }
        } else if (message != NULL && strlen(message) > 0) {
            int startIdx = (page - 2) * 16; 
            if (startIdx < strlen(message)) {
                char message_block[17] = {0}; 
                strncpy(message_block, message + startIdx, 16);
                lcdSetText(&lcd, "   Mensagem:    ", 0, 0);
                lcdSetText(&lcd, message_block, 0, 1);
            }
        }

        page = (page + 1) % totalPages; 
        vTaskDelay(1500 / portTICK_PERIOD_MS);
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

    nimble_host_config_init();

    xTaskCreate(lcd_task, "LCD", 4 * 1024, NULL, 5, NULL);
    xTaskCreate(nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);

    return;
}
