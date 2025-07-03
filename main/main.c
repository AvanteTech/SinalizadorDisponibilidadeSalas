#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "include/esp_lcd.h"
#include "wifi_connect.h"
#include "sntp_time.h"
#include "esp_sntp.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_spiffs.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "driver/gpio.h"
#include "mfrc522.h"

static const char *TAG_ = "ACCESS_CONTROL";

#define WIFI_SSID       "AvanteTech_WIFI"
#define WIFI_PASSWORD  "@@avante2025@@"
#define SHEETS_URL     "https://script.google.com/macros/s/AKfycbwlbImvv-6Rg32jxj9_TAPYhgJkgIq_YXCHmB5JNY4NufmzsdrlBTk4YkqXaxkVjrYm/exec"

#define LED_PIN             2

#define MFRC522_MOSI        23
#define MFRC522_MISO        19
#define MFRC522_CLK         18
#define MFRC522_SDA         5
#define MFRC522_RST         4

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static bool uids_ready = false;

lcd_t lcd;
char *message = NULL;
char status = ' ';
char status_ready = 'D';
int payload_index = 0;
int page = 0;

#include "esp_timer.h"

void delay_busy_us(uint32_t microseconds) {
    uint64_t start = esp_timer_get_time(); 
    while ((esp_timer_get_time() - start) < microseconds) {
        
    }
}


void delay_busy_ms(uint32_t milliseconds) {
    delay_busy_us(milliseconds * 1000);
}


bool check_uid_in_file(const char* uid_to_check) {
    if (!uids_ready) {
        ESP_LOGW(TAG_, "Lista de UIDs ainda não está pronta. Acesso negado por padrão.");
        return false;
    }
    ESP_LOGI(TAG, "Verificando UID: %s", uid_to_check);
    FILE* f = fopen("/spiffs/uids.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG_, "Arquivo de UIDs não encontrado! Negando acesso.");
        return false;
    }
    char line[64];
    bool found = false;
    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\n")] = 0;
        if (strcasecmp(uid_to_check, line) == 0) { 
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

static void rfid_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    rc522_event_data_t* event_data = (rc522_event_data_t*) data;
    rc522_tag_t* tag = (rc522_tag_t*) event_data->ptr;
    uint32_t uid_real = (uint32_t)(tag->serial_number & 0xFFFFFFFFULL);
    char uid_str[17] = {0};
    sprintf(uid_str, "%lX", uid_real);

    if (check_uid_in_file(uid_str)) {
        lcdClear(&lcd);
        lcdSetText(&lcd, " ACESSO. ", 0, 0);
        lcdSetText(&lcd, "LIBERADO", 0, 1);
        ESP_LOGW(TAG_, "ACESSO LIBERADO para o UID: %s", uid_str);
        gpio_set_level(LED_PIN, 1);
        delay_busy_ms(3000);
         lcdClear(&lcd);
        gpio_set_level(LED_PIN, 0);
    } else {
        lcdClear(&lcd);
        lcdSetText(&lcd, " ACESSO. ", 0, 0);
        lcdSetText(&lcd, "NEGADO", 0, 1);
        ESP_LOGE(TAG_, "ACESSO NEGADO para o UID: %s", uid_str);
        for (int i = 0; i < 3; i++) {
            gpio_set_level(LED_PIN, 1); vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_PIN, 0); vTaskDelay(pdMS_TO_TICKS(100));
        }
        delay_busy_ms(2800);
         lcdClear(&lcd);
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) { esp_wifi_connect(); } 
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED) { ESP_LOGI(TAG, "Reconectando..."); vTaskDelay(pdMS_TO_TICKS(5000)); esp_wifi_connect(); }
    else if (event_id == IP_EVENT_STA_GOT_IP) { ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data; ESP_LOGI(TAG, "Wi-Fi Conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip)); xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT); }
}

void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));
    wifi_config_t wifi_config = {.sta = {.ssid = WIFI_SSID, .password = WIFI_PASSWORD}};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void data_sync_task(void *pvParameters) {
    ESP_LOGI(TAG, "Tarefa de sincronização iniciada. Aguardando conexão Wi-Fi...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    uids_ready = true;
    ESP_LOGI(TAG, "Sincronização concluída. Lista de UIDs está pronta.");
    vTaskDelete(NULL);
}
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

    nvs_flash_init();
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs", .partition_label = "storage", .max_files = 5, .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    

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

    wifi_init();
    xTaskCreate(&data_sync_task, "data_sync_task", 8192, NULL, 5, NULL);

    rc522_config_t config = {
        .transport = RC522_TRANSPORT_SPI,
        .spi = {
            .host = SPI3_HOST,
            .miso_gpio = MFRC522_MISO,
            .mosi_gpio = MFRC522_MOSI,
            .sck_gpio = MFRC522_CLK,
            .sda_gpio = MFRC522_SDA,
            .bus_is_initialized = false, 
        },
    };

    rc522_handle_t rc522_handle;
    ESP_ERROR_CHECK(rc522_create(&config, &rc522_handle));
    ESP_ERROR_CHECK(rc522_register_events(rc522_handle, RC522_EVENT_TAG_SCANNED, rfid_handler, NULL));
    ESP_ERROR_CHECK(rc522_start(rc522_handle));

    ESP_LOGI(TAG, "app_main finalizada. Tarefas de RFID e Sincronização em execução.");

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
