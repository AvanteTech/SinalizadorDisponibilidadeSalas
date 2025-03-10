#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "driver/gpio.h"

#define TAG_LCD "LCD"
#define TAG "NimBLE_GATT_Server"
#define DEVICE_NAME "SIG_BOARD"

#endif 
