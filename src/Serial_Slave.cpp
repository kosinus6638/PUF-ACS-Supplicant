#include "Serial_Slave.h"

#include <driver/uart.h>
#include <esp_vfs_dev.h>


constexpr int NOTIFY_DELAY_MS = 50;
constexpr char EOL = '\x04';


size_t serial_send(const char* buffer) {
    size_t n = 0, n_written = 0;
    size_t bufSize = strlen(buffer);

    do {
        n = write(STDOUT_FILENO, static_cast<const void*>(buffer+n_written), 1);
        n_written += n;
    } while(n>0 && n_written<bufSize);

    write(STDOUT_FILENO, &EOL, 1);  // Send EOL

    fflush(stdout);
    return n_written;
}


size_t serial_receive(char* buffer, size_t bufSize) {
    char buf = '\0';
    size_t n = 0, n_written = 0;

    do {
        if( (n = read(STDIN_FILENO, &buf, 1)) == 1 ) {
            buffer[n_written++] = buf;
            n_written %= bufSize;   // Dirty little hack to prevent buffer overflows
        }
    } while(n>0 && buf != EOL);

    --n_written;

    buffer[n_written] = '\0';   // Replace EOL with null
    return n_written;  // Returns no nullbyte but includes EOL
}


void register_handler() {
    serial_send("register_ok");
    fflush(stdout);

    vTaskDelay( NOTIFY_DELAY_MS/portTICK_PERIOD_MS);

    // Notify main task
    xTaskNotify(main_tsk_hdl, CMD::REGISTER, eSetValueWithOverwrite);
}


void connect_handler() {
    serial_send("connect_ok");
    fflush(stdout);

    vTaskDelay( NOTIFY_DELAY_MS/portTICK_PERIOD_MS);

    // Notify main task
    xTaskNotify(main_tsk_hdl, CMD::CONNECT, eSetValueWithOverwrite);
}


void status_handler() {
    switch(global_err) {
        case 0:
            serial_send("no_err");
            break;
        case 1:
            serial_send("signup_err");
            break;
        case 2:
            serial_send("connect_err");
            break;
        case 3:
            serial_send("not_connected_err");
            break;
        default:
            serial_send("unkown_err");
    }

    fflush(stdout);
}


void speedtest_handler() {
    char cmd_buf[512];
    memset(cmd_buf, 0, sizeof(cmd_buf));
    int seconds=0, delay_us=0, frame_size=0;
    char delim;

    /* Send ack */
    serial_send("speedtest_ok");
    fflush(stdout);

    /* Wait for values */
    serial_receive(cmd_buf, sizeof(cmd_buf));
    if( sscanf(cmd_buf, "%c%d%c%d%c%d", &delim, &seconds, &delim, &delay_us, &delim, &frame_size) == EOF) {
        serial_send("err");
        fflush(stdout);
        return;
    }

    /* If no error occured apply values */
    opts.speedtest_seconds = seconds;
    opts.delay_us = delay_us;
    opts.frame_size = frame_size;

    /* Send ack */
    serial_send("values_ok");
    fflush(stdout);
    
    vTaskDelay( NOTIFY_DELAY_MS/portTICK_PERIOD_MS);

    /* Notify main task */
    xTaskNotify(main_tsk_hdl, CMD::SPEEDTEST, eSetValueWithOverwrite);
}


void cmd_task(void *pvParamter) {
    char cmd_buf[512];

    snprintf(cmd_buf, sizeof(cmd_buf), "Executing cmd on core: %d with EOL: %02x\n", xPortGetCoreID(), EOL);
    serial_send(cmd_buf);

    fflush(stdout);
    fflush(stdin);

    while(true) {
        memset(cmd_buf, 0, sizeof(cmd_buf));
        serial_receive(cmd_buf, sizeof(cmd_buf));

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
