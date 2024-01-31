#pragma once

#include <esp_eth.h>

#include "platform.h"
#include "errors.h"
#include "packets.h"


class EthernetNetworkException : public puf::NetworkException {
public:
    using puf::NetworkException::NetworkException;

    EthernetNetworkException(const char* err_msg, esp_err_t err_code) {
        snprintf(err_msg_buffer, sizeof(err_msg_buffer), "%s: %s", err_msg, esp_err_to_name(err_code));
    }
};


typedef struct {
    uint8_t buf[1518+8];  // Eight extra bytes for 802.1ad
    size_t len;
} eth_frame;


extern TaskHandle_t main_tsk_hdl;
extern eth_frame eth_buf;


class EthernetNetwork : public puf::Network {
private:
    esp_eth_handle_t eth_handle;
    bool initialised;

    void install_ethernet();

public:
    EthernetNetwork();
    void init() override;
    void send(uint8_t *buf, size_t bufSize) override;
    int receive(uint8_t *buf, size_t bufSize) override;
};