#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "include/lcd_i2c.h"
#include "wifi_connect.h"
#include "sntp_time.h"
#include "esp_sntp.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
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

#define WIFI_SSID      "********"
#define WIFI_PASSWORD  "********"
#define SHEETS_URL     "https://script.google.com/macros/s/AKfycbwVCsVWalTt9MY9HK7cM-h_VNzw27HN2DovlDa2K9xS_eRbT1yWtXPbR_lUOJ1iyrbf/exec"

#define RELAY_PIN           2
#define UNLOCK_DURATION_MS  500

#define MFRC522_MOSI        23
#define MFRC522_MISO        19
#define MFRC522_CLK         18
#define MFRC522_SDA         15
#define MFRC522_RST         4

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static bool uids_ready = false;
static SemaphoreHandle_t lcd_mutex;

typedef enum { LCD_MSG_NONE, LCD_MSG_GRANTED, LCD_MSG_DENIED } lcd_access_message_t;
static QueueHandle_t lcd_message_queue;

char *message = NULL;
char status = ' ';
static char status_ready = 'D'; 

char response_buffer[1024];
int response_len = 0;

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (esp_http_client_get_status_code(evt->client) == 200) {
                if (response_len + evt->data_len < sizeof(response_buffer)) {
                    memcpy(response_buffer + response_len, evt->data, evt->data_len);
                    response_len += evt->data_len;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            response_buffer[response_len] = '\0';
            break;
        default:
            break;
    }
    return ESP_OK;
}

bool check_uid_in_file(const char* uid_to_check) {
    if (!uids_ready) return false;
    
    FILE* f = fopen("/spiffs/uids.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG_, "Arquivo de UIDs nao encontrado!");
        return false;
    }
    char line[64];
    bool found = false;
    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\r\n")] = 0;
        if (strcasecmp(uid_to_check, line) == 0) {
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

void relay_off_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(UNLOCK_DURATION_MS));
    gpio_set_level(RELAY_PIN, 0);
    vTaskDelete(NULL);
}

static void rfid_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    rc522_event_data_t* event_data = (rc522_event_data_t*) data;
    rc522_tag_t* tag = (rc522_tag_t*) event_data->ptr;
    char uid_str[17] = {0};
    sprintf(uid_str, "%08lX", (uint32_t)(tag->serial_number & 0xFFFFFFFFULL));

    lcd_access_message_t msg_type;

    if (check_uid_in_file(uid_str)) {
        ESP_LOGW(TAG_, "ACESSO LIBERADO: %s", uid_str);
        gpio_set_level(RELAY_PIN, 1);
        xTaskCreate(relay_off_task, "RelayOff", 2048, NULL, 5, NULL);
        msg_type = LCD_MSG_GRANTED;
        xQueueOverwrite(lcd_message_queue, &msg_type);
    } else {
        ESP_LOGE(TAG_, "ACESSO NEGADO: %s", uid_str);
        msg_type = LCD_MSG_DENIED;
        xQueueOverwrite(lcd_message_queue, &msg_type);
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_, "Wi-Fi Conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    wifi_config_t wifi_config = {.sta = {.ssid = WIFI_SSID, .password = WIFI_PASSWORD}};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void data_sync_task(void *pvParameters) {
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG_, "Iniciando sincronização de UIDs...");
    
    response_len = 0;
    memset(response_buffer, 0, sizeof(response_buffer));
    esp_http_client_config_t config = {
        .url = SHEETS_URL,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_redirection(client);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        FILE* f = fopen("/spiffs/uids.txt", "w");
        if (f) {
            fprintf(f, "%s", response_buffer);
            fclose(f);
            ESP_LOGI(TAG_, "--- UIDs Sincronizados ---");
            ESP_LOGI(TAG_, "\n%s", response_buffer);
            ESP_LOGI(TAG_, "--------------------------");
        }
    } else {
        ESP_LOGE(TAG_, "Requisição HTTP falhou: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    uids_ready = true;
    vTaskDelete(NULL);
}

void lcd_scroll_string(const char* text, int row, int delay_ms) {
    if (strlen(text) <= 16) {
        lcd_set_cursor(0, row);
        lcd_send_string(text);
        vTaskDelay(pdMS_TO_TICKS(3000));
        return;
    }
    char buffer[17] = {0};
    for (int i = 0; i <= (int)strlen(text) - 16; i++) {
        strncpy(buffer, text + i, 16);
        lcd_set_cursor(0, row);
        lcd_send_string(buffer);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void ble_store_config_init(void){}

static void on_stack_reset(int reason) {
    ESP_LOGE(TAG_, "NimBLE Stack reiniciado, motivo: %d", reason);
}

static void on_stack_sync(void) {
    adv_init();
}

static void nimble_host_config_init(void) {
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG_, "Tarefa do NimBLE Host iniciada.");
    nimble_port_run(); 
    nimble_port_freertos_deinit();
    vTaskDelete(NULL);
}

void get_full_datetime(char *datetime_str, size_t max_len)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(datetime_str, max_len, "%d/%m/%y  %H:%M", &timeinfo);
}

char *get_scheduled_message() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int hour = timeinfo.tm_hour;
    int min = timeinfo.tm_min;

    if ((hour >= 8 && hour < 10)) { // 08:00 - 09:59
        status_ready = 'O';
        return " Em expediente  ";
    } else if (hour == 10 && min < 20) { // 10:00 - 10:19
        status_ready = 'D';
        return "   Intervalo    ";
    } else if ((hour == 10 && min >= 20) || hour == 11) { // 10:20 - 11:59
        status_ready = 'O';
        return " Em expediente  ";
    } else if (hour == 15 && min < 20) { // 15:00 - 15:19
        status_ready = 'D';
        return "   Intervalo    ";
    } else if ((hour >= 13 && hour < 15) || (hour == 15 && min >= 20) || (hour >= 16 && hour < 17)) { // Tarde pt. 1
        status_ready = 'O';
        return " Em expediente  ";
    } else if (hour >= 18 && hour < 20) { // Noite pt. 1
        status_ready = 'O';
        return " Em expediente  ";
    } else if (hour == 20 && min < 20) { // 20:00 - 20:19
        status_ready = 'D';
        return "   Intervalo    ";
    } else if ((hour == 20 && min >= 20) || (hour == 21)) { // Noite pt. 2 (até 21:59)
        status_ready = 'O';
        return " Em expediente  ";
    } else {
        status_ready = 'D';
        return NULL;
    }
}

void lcd_task(void *pvParameters) {
    char datetime_str[17];
    char status_str[17];
    char msg_header[] = "   Mensagem:    ";
    lcd_access_message_t received_msg;
    int carousel_state = 0;

    while (1) {
        if (xQueueReceive(lcd_message_queue, &received_msg, pdMS_TO_TICKS(3000)) == pdTRUE) {
            if (xSemaphoreTake(lcd_mutex, portMAX_DELAY) == pdTRUE) {
                lcd_init();
                if (received_msg == LCD_MSG_GRANTED) {
                    lcd_set_cursor(0, 0); lcd_send_string(" ACESSO LIBERADO");
                    lcd_set_cursor(0, 1); lcd_send_string("   Bem-vindo!   ");
                } else if (received_msg == LCD_MSG_DENIED) {
                    lcd_set_cursor(0, 0); lcd_send_string("  ACESSO NEGADO  ");
                }
                xSemaphoreGive(lcd_mutex);
            }
            carousel_state = 0;
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            if (xSemaphoreTake(lcd_mutex, portMAX_DELAY) == pdTRUE) {
                lcd_init();
                
                char *scheduled_message = get_scheduled_message();
                switch (carousel_state) {
                    case 0:
                        get_full_datetime(datetime_str, sizeof(datetime_str));
                        lcd_set_cursor(0, 0); lcd_send_string(" AvanteTech Jr. ");
                        lcd_set_cursor(0, 1); lcd_send_string(datetime_str);
                        carousel_state = 1;
                        break;

                    case 1:
                        char status_char = (status != ' ') ? status : status_ready;
                        sprintf(status_str, (status_char == 'O') ? "    Ocupada    " : "   Disponivel   ");
                        lcd_set_cursor(0, 0); lcd_send_string("     Status     ");
                        lcd_set_cursor(0, 1); lcd_send_string(status_str);
                        carousel_state = (message == NULL) ? 0 : 2;
                        break;

                    case 2:
                        if (message != NULL) {
                            lcd_set_cursor(0, 0); lcd_send_string(msg_header);
                            lcd_scroll_string(message, 1, 300);
                        }
                        carousel_state = 0;
                        break;
                }
                xSemaphoreGive(lcd_mutex);
            }
        }
    }
}

void app_main(void) {
    nvs_flash_init();
    esp_vfs_spiffs_conf_t conf = {.base_path = "/spiffs", .partition_label = "storage", .max_files = 5, .format_if_mount_failed = true};
    esp_vfs_spiffs_register(&conf);
    
    gpio_reset_pin(RELAY_PIN);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_PIN, 0);

    lcd_mutex = xSemaphoreCreateMutex();
    lcd_message_queue = xQueueCreate(1, sizeof(lcd_access_message_t));
    
    ESP_ERROR_CHECK(i2c_lcd_driver_init());
    lcd_init();

    nimble_port_init();
    gap_init();
    gatt_svc_init();
    nimble_host_config_init();
    xTaskCreate(nimble_host_task, "NimBLE Host", 4096, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(100));

    wifi_init();
    if (xSemaphoreTake(lcd_mutex, portMAX_DELAY) == pdTRUE) {
        lcd_init();
        lcd_set_cursor(0, 0); lcd_send_string(" Conectando WiFi");
        lcd_set_cursor(0, 1); lcd_send_string("    Aguarde...    ");
        xSemaphoreGive(lcd_mutex);
    }


    
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    
    if (xSemaphoreTake(lcd_mutex, portMAX_DELAY) == pdTRUE) {
        lcd_init();
        lcd_set_cursor(0, 0); lcd_send_string(" Sincronizando..");
        lcd_set_cursor(0, 1); lcd_send_string("      Hora      ");
        xSemaphoreGive(lcd_mutex);
    }
    sntp_time_init();
    sntp_wait_for_sync();

    xTaskCreate(&data_sync_task, "data_sync_task", 8192, NULL, 5, NULL);

    rc522_config_t config_rc522 = {.transport = RC522_TRANSPORT_SPI, .spi = {.host = SPI3_HOST, .miso_gpio = MFRC522_MISO, .mosi_gpio = MFRC522_MOSI, .sck_gpio = MFRC522_CLK, .sda_gpio = MFRC522_SDA}};
    rc522_handle_t rc522_handle;
    rc522_create(&config_rc522, &rc522_handle);
    rc522_register_events(rc522_handle, RC522_EVENT_TAG_SCANNED, rfid_handler, NULL);
    rc522_start(rc522_handle);
    
    xTaskCreate(lcd_task, "LCD", 4096, NULL, 5, NULL);
}