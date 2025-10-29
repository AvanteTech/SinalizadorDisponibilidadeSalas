#include "sntp_time.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <string.h>

#define TIME_ZONE "BRT3"
static const char *TAG = "SNTP";

void sntp_time_init(void) {
    // Configuração do modo de operação
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    // Configura servidores NTP
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "br.pool.ntp.org");
    
    // Inicializa o SNTP
    esp_sntp_init();

    // Configura fuso horário
    setenv("TZ", TIME_ZONE, 1);
    tzset();
}

void sntp_wait_for_sync(void) {
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) {
        ESP_LOGI(TAG, "Aguardando sincronização...");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void print_current_time(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Hora atual: %s", asctime(&timeinfo));
}