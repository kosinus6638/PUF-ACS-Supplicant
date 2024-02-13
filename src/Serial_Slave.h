#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


typedef struct SerialOptions {
    int speedtest_seconds;
    int delay_us;
    int frame_size;
} SerialOptions;


extern SerialOptions opts;
extern TaskHandle_t main_tsk_hdl;
extern int global_err;


enum CMD {
    REGISTER = (1<<0),
    CONNECT = (1<<1),
    SPEEDTEST = (1<<2),
    ESP_STATUS = (1<<3)
};


/**
 * This task is responsible for UART communication. Compatible
 * with xTaskCreate and xTaskCreatePinnedToCore
*/
void cmd_task(void *pvParamter);