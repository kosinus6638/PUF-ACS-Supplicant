#include <stdio.h>
#include <time.h>


#include <lwip/sockets.h>
#include <driver/uart.h>
#include <esp_vfs_dev.h>

#include "supplicant.h"
#include "packets.h"
#include "platform.h"
#include "errors.h"
#include "statics.h"

#include "SRAM_PUF.h"
#include "Ethernet_Network.h"

#include <rom/ets_sys.h>

// #define delay_us(us) esp_rom_delay_us(us)
#define delay_us(us) ets_delay_us(us)

TaskHandle_t main_tsk_hdl;
eth_frame eth_buf;


const static puf::MAC switch_mac = {0x04, 0x92, 0x26, 0x87, 0x84, 0x11};


SRAM_PUF sram_puf;
EthernetNetwork net;
puf::Supplicant s(net, sram_puf);


void loop();


unsigned long net_cpu_time_used = 0;
unsigned long delay_cpu_time_used = 0;


extern "C" void app_main() {
    using namespace puf;

    // Init globals
    eth_buf.len = 0;
    main_tsk_hdl = xTaskGetCurrentTaskHandle();


    /* Install UART driver for interrupt-driven reads and writes */
    uart_driver_install(CONFIG_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_CONSOLE_UART_NUM);    // This causes everything to be slower

    try {
        s.init();
        // s.sign_up();
        s.connect();
    } catch(const Exception& e) {
        puts(e.what());
    }

    int ctr = 0;

    if(s.connected()) {
        s.transmit(NULL, 0, true);

        while(ctr < 500000) {
            loop();
            ++ctr;
        }
    }
}


void loop() {

    static char msg[] = "Hello World";
    // Error insufficient TX buffer size:   (208) esp_eth/src/esp_eth_mac_esp.c: emac_esp32_transmit
    // Calls function: hal/emac_hal:        (407) emac_hal_transmit_frame

    delay_us(20000);   // As of now 90 is the minimum
    s.transmit(reinterpret_cast<uint8_t*>(msg), sizeof(msg));
}