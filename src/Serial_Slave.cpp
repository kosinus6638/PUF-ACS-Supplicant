#include "Serial_Slave.h"

#include <driver/uart.h>
#include <esp_vfs_dev.h>


constexpr int NOTIFY_DELAY_MS = 50;


void register_handler() {
    printf("register_ok\r");
    fflush(stdout);

    vTaskDelay( NOTIFY_DELAY_MS/portTICK_PERIOD_MS);

    // Notify main task
    xTaskNotify(main_tsk_hdl, CMD::REGISTER, eSetValueWithOverwrite);
}


void connect_handler() {
    printf("connect_ok\r");
    fflush(stdout);

    vTaskDelay( NOTIFY_DELAY_MS/portTICK_PERIOD_MS);

    // Notify main task
    xTaskNotify(main_tsk_hdl, CMD::CONNECT, eSetValueWithOverwrite);
}


void status_handler() {
    switch(global_err) {
        case 0:
            printf("no_err\r");
            break;
        case 1:
            printf("signup_err\r");
            break;
        case 2:
            printf("connect_err\r");
            break;
        case 3:
            printf("not_connected_err\r");
            break;
        default:
            printf("unkown_err\r");
    }

    fflush(stdout);
}


void speedtest_handler() {
    char cmd_buf[512];
    memset(cmd_buf, 0, sizeof(cmd_buf));
    int seconds=0, delay_us=0, frame_size=0;
    char delim;

    /* Send ack */
    printf("speedtest_ok\r");
    fflush(stdout);

    /* Wait for values */
    scanf("%s", cmd_buf);
    if( sscanf(cmd_buf, "%c%d%c%d%c%d\r", &delim, &seconds, &delim, &delay_us, &delim, &frame_size) == EOF) {
        printf("err\r");
        fflush(stdout);
        return;
    }

    /* If no error occured apply values */
    opts.speedtest_seconds = seconds;
    opts.delay_us = delay_us;
    opts.frame_size = frame_size;

    /* Send ack */
    printf("values_ok\r");
    fflush(stdout);
    
    vTaskDelay( NOTIFY_DELAY_MS/portTICK_PERIOD_MS);

    /* Notify main task */
    xTaskNotify(main_tsk_hdl, CMD::SPEEDTEST, eSetValueWithOverwrite);
}


void cmd_task(void *pvParamter) {
    printf("Executing cmd on core: %d\n", xPortGetCoreID());
    fflush(stdout);

    /* Install UART driver for interrupt-driven reads and writes */
    uart_driver_install(CONFIG_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_CONSOLE_UART_NUM);    // This causes everything to be slower

    char cmd_buf[512];
    while(true) {
        memset(cmd_buf, 0, sizeof(cmd_buf));

        fflush(stdout);
        scanf("%s", cmd_buf);

        if(      strcmp(cmd_buf, "register") == 0 )     register_handler();
        else if( strcmp(cmd_buf, "status") == 0 )       status_handler();
        else if( strcmp(cmd_buf, "connect") == 0 )      connect_handler();
        else if( strcmp(cmd_buf, "speedtest") == 0 )    speedtest_handler();
        else { 
            printf("Unknown_command:_0x%02x\r", cmd_buf[0]);
            fflush(stdout);
        }

    }
}
