#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

void wifi_connect_init(const char *ssid, const char *password);


#endif