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
    static uint8_t concat_buf[36];
    static VLAN_Payload p;
    static PUF_Performance pp;

    size_t k_offset = 0;

    if(initial_frame) {
        memset(hk_mac, 0, sizeof(hk_mac));

        // Build static frame on first send
        pp.src_mac = s.mac;
        pp.dst_mac = switch_mac;
        pp.calc();

        // Copy MAC into concatenation buffer
        k_offset = sizeof(s.mac.bytes);
        memcpy(concat_buf, s.mac.bytes, k_offset);
    } else {
        // Copy last hk_mac into concatenation buffer
        k_offset = sizeof(hk_mac);
        memcpy(concat_buf, hk_mac, k_offset);
    }

    // Concatenate 4 digits of k to concatenation buffer
    memcpy(concat_buf+k_offset, (void*)s.k.private_p, 4);

    if( mbedtls_sha256(concat_buf, sizeof(concat_buf), hk_mac, 0) != ESP_OK) {
        return 1;
    }

    // Set VLAN tags
    p.load1 = *(reinterpret_cast<uint16_t*>(hk_mac));
    p.load2 = *(reinterpret_cast<uint16_t*>(hk_mac+30));
    pp.set_payload(p);

    net.send(pp.binary(), pp.header_len());

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
        // s.sign_up();
        s.connect();
    } catch(const Exception& e) {
        puts(e.what());
    }

    printf("%s\n", s.connected() ? "Connected" : "Connection failed" );
    send_tagged_frame(true);

    while(true) {
        loop();
    }
}


void loop() {
    // Error insufficient TX buffer size:   (208) esp_eth/src/esp_eth_mac_esp.c: emac_esp32_transmit
    // Calls function: hal/emac_hal:        (407) emac_hal_transmit_frame
    sys_delay_ms(5000);
    puts("Sending frame");
    send_tagged_frame();
}