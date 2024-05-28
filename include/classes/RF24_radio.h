
#pragma once
#include "config.h"
class MQTTGateway;


class RF24Radio : public RF24 {
public:
    
    RF24Radio(uint8_t ce = SPI_1_CE, uint8_t csn = SPI_1_CSN) : RF24(ce, csn), _RF24_spi_bus(HSPI) {
        _RF24_spi_bus.begin(SPI_1_SCLK, SPI_1_MISO, SPI_1_MOSI, SPI_1_CE);
    };

    // Radio message structure
    struct EncodedMessage {
        uint16_t combined; // combined = from_who (7bits), to_who (8bits), state (1bit)
        uint16_t data_0;
        uint16_t data_1;
        uint16_t data_2;
    };

    // Used internally
    struct DecodedMessage {
        uint8_t from_who;
        uint8_t to_who;
        bool state;
        uint16_t data_0;
        uint16_t data_1;
        uint16_t data_2;
    };

    bool init(MQTTGateway* mqtt);
    bool send_message(uint8_t to_who, bool state, uint16_t data_0, uint16_t data_1, uint16_t data_2);
    bool send_time_to_node(uint16_t node_ID = 0);
    bool get_available_message(DecodedMessage& message);
    bool wait_for_message(uint16_t ms);
    bool route_incoming_radio(const bool print_msg = false);
    void print_message(const EncodedMessage& data);
    void print_message(const DecodedMessage& data);
    void print_remaining_message(uint16_t data_0, uint16_t data_1, uint16_t data_2);
    void print_P_variant();
    uint16_t test_send_time_to_node(uint8_t sending_node_ID, uint8_t test_node_ID);

private:
    SPIClass _RF24_spi_bus;

    static MQTTGateway* _mqtt_ptr;
};
