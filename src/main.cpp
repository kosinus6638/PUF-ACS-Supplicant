#include <stdio.h>
#include <time.h>

#include "supplicant.h"
#include "packets.h"
#include "platform.h"
#include "errors.h"
#include "statics.h"

#include "SRAM_PUF.h"
#include "Ethernet_Network.h"
#include "Serial_Slave.h"

#include <rom/ets_sys.h>
#include <esp_timer.h>

// #define delay_us(us) esp_rom_delay_us(us)
#define delay_us(us) ets_delay_us(us)


TaskHandle_t main_tsk_hdl;
eth_frame eth_buf;
int global_err = 0;
SerialOptions opts;


SRAM_PUF sram_puf;
EthernetNetwork net;
puf::Supplicant s(net, sram_puf);

bool keepGoing = true;


void timer_isr(void *args) {
    keepGoing = false;
}


void speedtest() {
    const esp_timer_create_args_t timer_args = {
        .callback = &timer_isr,
        .arg = NULL,
        .name = "one-shot"
    };

    esp_timer_handle_t timer;
    int duration = opts.speedtest_seconds * 1000*1000;
    esp_timer_create(&timer_args, &timer);

    char msg[] = "Hello World";
    keepGoing = true;

    esp_timer_start_once(timer, duration);   // Start for 5 seconds
    s.transmit(NULL, 0, true);  // First frame is a special case
    while(keepGoing) {
        s.transmit( reinterpret_cast<uint8_t*>(msg), sizeof(msg) );
        delay_us(opts.delay_us);
    }

    keepGoing = true;
}


extern "C" void app_main() {
    using namespace puf;

    // Init globals
    eth_buf.len = 0;
    main_tsk_hdl = xTaskGetCurrentTaskHandle();

    try {
        s.init();
    } catch(const Exception &e) {
        puts(e.what());
    }

    /* Start UART task on second core */
    xTaskCreatePinnedToCore(&cmd_task, "cmd_task", 4096, NULL, 5, NULL, APP_CPU_NUM);

    uint32_t ulNotifiedValue = 0;

    while(true) {
        xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY );
        switch(ulNotifiedValue) {
            case CMD::REGISTER:
                try {
                    s.sign_up();
                } catch(const Exception &e) {
                    global_err = 1;
                }
                break;
            case CMD::CONNECT:
                try {
                    s.connect();
                } catch(const Exception &e) {
                    global_err = 2;
                }
                break;
            case CMD::SPEEDTEST:
                if( !s.connected() ) {
                    global_err = 3;
                    break;
                }
                speedtest();
                break;

            default:
                ;
        }
    }
}