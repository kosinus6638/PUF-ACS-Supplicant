#include <stdio.h>
#include <time.h>


#include <lwip/sockets.h>

#include <mbedtls/sha256.h>

#include "supplicant.h"
#include "packets.h"
#include "platform.h"
#include "errors.h"
#include "statics.h"

#include "SRAM_PUF.h"
#include "Ethernet_Network.h"


#define delay_us(us) esp_rom_delay_us(us)

TaskHandle_t main_tsk_hdl;
eth_frame eth_buf;


const static puf::MAC switch_mac = {0x04, 0x92, 0x26, 0x87, 0x84, 0x11};


SRAM_PUF sram_puf;
EthernetNetwork net;
puf::Supplicant s(net, sram_puf);


int send_tagged_frame(bool initial_frame = false) {
    using namespace puf;
    static uint8_t hk_mac[32];
    static uint8_t k_mac[36];
    static Payload p;
    static int counter = 0;

    memset(k_mac, 0, sizeof(k_mac));
    size_t k_offset = 0;

    // Copy MAC or last hash into buffer
    if(initial_frame) {
        k_offset = sizeof(s.mac.bytes);
        memcpy(k_mac, s.mac.bytes, k_offset);
    } else {
        k_offset = sizeof(hk_mac);
        memcpy(k_mac, hk_mac, k_offset);
    }

    // Concatenate with k
    memcpy(k_mac+k_offset, (void*)s.k.private_p, 4);

    if( mbedtls_sha256(k_mac, sizeof(k_mac), hk_mac, 0) != ESP_OK) {
        return 1;
    }

    p.load1 = *(reinterpret_cast<uint16_t*>(hk_mac));
    p.load2 = *(reinterpret_cast<uint16_t*>(hk_mac+30));


    PUF_Performance pp;
    pp.src_mac = s.mac;
    pp.dst_mac = switch_mac;
    pp.calc();
    pp.set_payload(p);

    printf("%d Sending payload: 0x%04x 0x%04x\n", counter++, pp.get_payload().load1, pp.get_payload().load2);

    net.send(pp.binary(), pp.header_len());

    s.mac.hash(1);

    return 0;
}


void loop();


extern "C" void app_main() {
    using namespace puf;

    // Init globals
    eth_buf.len = 0;
    main_tsk_hdl = xTaskGetCurrentTaskHandle();

    try {
        s.init();
        s.sign_up();
        s.connect();
    } catch(const Exception& e) {
        puts(e.what());
    }

    printf("%s\n", s.connected() ? "Connected" : "Connection failed" );

    while(true) {
        loop();
    }
}


void loop() {
    // Error insufficient TX buffer size:   (208) esp_eth/src/esp_eth_mac_esp.c: emac_esp32_transmit
    // Calls function: hal/emac_hal:        (407) emac_hal_transmit_frame
    sys_delay_ms(500);
    // send_tagged_frame();
}