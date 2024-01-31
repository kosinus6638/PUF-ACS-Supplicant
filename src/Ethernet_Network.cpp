#include "Ethernet_Network.h"
#include "errors.h"

#include <esp_eth_phy.h>
#include <esp_eth_mac.h>
#include <esp_eth_com.h>
#include <esp_event.h>
#include <lwip/prot/ethernet.h>


#define delay_ms(ms) vTaskDelay(ms/portTICK_PERIOD_MS)

using namespace puf;


/**
 * Receives ethernet frame and stores its contents in a global buffer. Frames
 * that are too long for the buffer are discarded. The main task is notified 
 * after a successfull receive.
*/
esp_err_t on_frame(esp_eth_handle_t hdl, uint8_t *buffer, uint32_t length, void *priv) {

    // Check for errors
    if( length >= sizeof(eth_buf) ) return ESP_FAIL;
    if( !buffer )                   return ESP_FAIL;

    // Update global buffer
    eth_buf.len = length;
    memcpy(eth_buf.buf, buffer, eth_buf.len);

    // Notify main task
    xTaskNotify(main_tsk_hdl, 0, eNoAction);
    return ESP_OK;
}


EthernetNetwork::EthernetNetwork() : eth_handle(NULL), initialised(false) {}


void EthernetNetwork::install_ethernet() {

    // Create event loop
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        throw EthernetNetworkException("esp_event_loop_create_default failed", err);
    }

    // Create MAC config
    esp_eth_mac_t *eth_mac = NULL;
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    esp32_emac_config.clock_config.rmii.clock_gpio = EMAC_CLK_IN_GPIO;
    esp32_emac_config.smi_mdc_gpio_num = 23;
    esp32_emac_config.smi_mdio_gpio_num = 18;
    mac_config.sw_reset_timeout_ms = 1000;
    eth_mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    if(eth_mac == NULL){
        throw EthernetNetworkException("esp_eth_mac_new_esp32 failed");
    }

    // Create PHY config
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = 16;
    esp_eth_phy_t *eth_phy = NULL;
    eth_phy = esp_eth_phy_new_lan87xx(&phy_config);
    if(eth_phy == NULL){
        throw EthernetNetworkException("esp_eth_phy_new failed");
    }

    // Glue MAC and PHY together and install driver
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(eth_mac, eth_phy);
    if(esp_eth_driver_install(&eth_config, &eth_handle) != ESP_OK || eth_handle == NULL){
        throw EthernetNetworkException("esp_eth_driver_install failed");
    }

    // Start driver
    if(esp_eth_start(eth_handle) != ESP_OK){
        throw EthernetNetworkException("esp_eth_start failed");
    }

    delay_ms(50);
}


void EthernetNetwork::init() {

    if(initialised) return;

    // Init Ethernet
    install_ethernet();

    // Set to promiscuous mode
    esp_err_t err = ESP_OK;
    bool t = true;
    if( (err = esp_eth_ioctl(eth_handle, ETH_CMD_S_PROMISCUOUS, static_cast<void*>(&t))) != ESP_OK) {
        throw EthernetNetworkException("Error setting Ethernet to promiscuous mode", err);
    }

    // Register callback on Ethernet frame arrival
    if( esp_eth_update_input_path(eth_handle, on_frame, NULL) != ESP_OK) {
        throw EthernetNetworkException("Error registering Ethernet callback", err);
    }

    initialised = true;
}


void EthernetNetwork::send (uint8_t *buf, size_t bufSize) {
    esp_err_t err = ESP_OK;
    if( (err = esp_eth_transmit(eth_handle, static_cast<void*>(buf), bufSize)) != ESP_OK) {
        throw EthernetNetworkException("Error transmitting Ethernet frame", err);
    }
}


int EthernetNetwork::receive(uint8_t *buf, size_t bufSize) {
    if(!buf) {
        throw EthernetNetworkException("Buffer is null");
    }

    struct eth_hdr *hdr = reinterpret_cast<struct eth_hdr*>(&eth_buf.buf);
    uint32_t ulNotifiedValue = 0;
    bool timeout = false;

    // Wait for ethernet frame arrival of correct type
    do {
        timeout = xTaskNotifyWait( 0x00,
                        ULONG_MAX,
                        &ulNotifiedValue,
                        NETWORK_TIMEOUT_MS / portTICK_PERIOD_MS ) == pdFALSE;
    } while(!timeout && (hdr->type != ETH_TYPE && hdr->type != ETH_EX) );

    // Not excpetional cases so no exception is thrown
    if(timeout) return -1;
    if(eth_buf.len > bufSize) return -2;

    memcpy(buf, eth_buf.buf, eth_buf.len);
    return eth_buf.len;
}