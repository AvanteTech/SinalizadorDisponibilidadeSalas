#ifndef SNTP_TIME_H
#define SNTP_TIME_H
#pragma once
#include "esp_sntp.h"
#include <time.h>

void sntp_time_init(void);
void sntp_wait_for_sync(void);
void print_current_time(void);
#endif