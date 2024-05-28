
#include "classes/RF24_radio.h"
#include "classes/MQTT_gateway.h"
#include "classes/hardware.h"

MQTTGateway* RF24Radio::_mqtt_ptr;


bool RF24Radio::init(MQTTGateway* mqtt) {
    _mqtt_ptr = mqtt;

    // Begin
    if (!RF24::begin(&_RF24_spi_bus, SPI_1_CE, SPI_1_CSN) && !RF24::isChipConnected()) {
        log("NRF24L01+ Offline! \n");
        return false; 
    }

    // Which address to listen on 
    constexpr byte pipe_address[1][3] = {0x1, 0x2, (byte)RF24_THIS_DEV_ADDRESS};

    // Setup
    setPALevel(RF24_OUTPUT, true);                  // Transmitter strength   
    setChannel(RF24_CHANNEL);                       // Channel 
    setDataRate(RF24_DATARATE);                     // Datarate (RF24_1MBPS, RF24_2MBPS, RF24_250KBPS)
    setAddressWidth(3);                             // Address size in bytes
    setRetries(5, 15);                              // Acknowledgement package, retry up to 15 times.
    setPayloadSize(8);                              // Payload size = RF24Radio::EncodedMessage
    setCRCLength(rf24_crclength_e::RF24_CRC_8);     // CRC size
    openReadingPipe(RF24_PIPE, pipe_address[0]);    // Address to listen on
    flush_rx();                                     // Flush buffer

    // Info
    #if SERIAL_ENABLED
        log("\n---------------------- NRF24L01 ----------------------\n");
        printPrettyDetails();
    #endif

    return true;
}


bool RF24Radio::send_message(uint8_t to_who, bool state, uint16_t data_0, uint16_t data_1, uint16_t data_2) {
    WDT_FEED();
    stopListening();
    log("Sending to Node: %i ", to_who);

    // Node address  
    byte pipe_address[1][3] = {0x1, 0x2, (byte)to_who};
    openWritingPipe(pipe_address[0]);
    
    // Populate msg
    EncodedMessage msg = {0, 0, 0, 0};
    msg.combined    |= (RF24_THIS_DEV_ADDRESS & 0x7F);  // Set the first 7 bits to RF24_THIS_DEV_ADDRESS
    msg.combined    |= ((uint16_t) to_who << 7);        // Set the next 8 bits to "to_who"
    msg.combined    |= ((uint16_t) state << 15);        // Set the last bit to "state"
    msg.data_0      = data_0;                           // Data 0
    msg.data_1      = data_1;                           // Data 1
    msg.data_2      = data_2;                           // Data 2

    // Nodes are toggling their radios on for 50ms and off for 950ms
    // write() takes about 33ms if there are no response (Nodes radios off)
    // Add a 26ms delay and we get a nice interval that falls inside a Node's listening window
    // Send up to 17 times with 59ms delay between messages
    bool message_sent = false;
    uint8_t retries = 0;
    uint32_t start_time = millis();

    // Send!
    for (size_t i = 0; i < 17; i++) {
        retries = i;
        if (write(&msg, sizeof(msg))) {
            message_sent = true;
            break;
        }
        WDT_FEED();
        delay(26);
    }

    // Prints
    uint32_t total_time = millis() - start_time;
    message_sent ? log("[SUCCESS]\n") : log("[FAILED]\n");
    log("- retries: %i\n- total time: %ims\n", retries, total_time);

    startListening();
    return message_sent;
} 


bool RF24Radio::send_time_to_node(uint16_t node_ID) {
    uint16_t current_time;
    WDT_FEED();

    #if DEBUG_TIME_TESTS
        int node_to_test = 1;
        current_time = test_send_time_to_node(node_ID, node_to_test);
    #else 
        // Send mocked or current time to nodes, in "hhmm" format
        if (_mqtt_ptr->debug_time_enabled) {
            log("- mocked time: ");
            current_time = 1200;
        } else {
            current_time = (hour() * 100 + minute());
            log("- current time: ");
        }
    #endif
    log("%i\n", current_time);

    // Send. Short delay to allow nodes to be ready for receiving
    delay(10);
    return send_message(node_ID, 0, 0, 0, current_time);
}


bool RF24Radio::get_available_message(DecodedMessage& message) {
    WDT_FEED();

    // Read message from nrf24l01+ buffer
    EncodedMessage raw;
    read(&raw, sizeof(raw));

    // DecodedMessage
    message.from_who    = raw.combined & 0x7F;                              // Extract first 7 bits of combined
    message.to_who      = (raw.combined >> 7) & 0xFF;                       // Extract next 8 bits
    message.state       = static_cast<bool>((raw.combined >> 15) & 0x01);   // Last bit as bool
    message.data_0      = raw.data_0;                                       // Data 0
    message.data_1      = raw.data_1;                                       // Data 1 
    message.data_2      = raw.data_2;                                       // Data 2

    // Verify   
    if (message.to_who == RF24_THIS_DEV_ADDRESS 
        && message.from_who > 0 
        && message.from_who < 255) {
        return true;
    }

    return false;
}


bool RF24Radio::wait_for_message(const uint16_t timeout_ms) {
    uint32_t end_time = millis() + timeout_ms;
    uint8_t pipe;

    while (millis() < end_time) {
        if (available(&pipe) && pipe == RF24_PIPE) {
            return route_incoming_radio(); // CRC is handle internally, won't get here if message is corrupted
        }
        WDT_FEED();
        delay(1);    
    }

    return false;
}


bool RF24Radio::route_incoming_radio(const bool print_msg) {
    log("\n----------------------- Radio -----------------------\n");
    DecodedMessage message;

    if (!get_available_message(message)) { 
        return false; 
    }

    if (print_msg) { 
        print_message(message); 
    }

    // Node: time request or data update?
    bool success = true;
    if (message.data_2 == 0xFFFF) {
        log("Node %i requests the time!\n", message.from_who);
        success = send_time_to_node(message.from_who);
    } else {
        log("Node %i data update\n", message.from_who);
    }

    // Send Node data to InfluxDB
    if (success) {
        _mqtt_ptr->radio_data_to_DB(message);
    }

    return true;
}


void RF24Radio::print_message(const EncodedMessage& msg) {
    log("from_who:\t%i\n", msg.combined & 0x7F);
    log("to_who:\t\t%i\n", (msg.combined >> 7) & 0xFF);
    log("state:\t\t%d\n", static_cast<bool>((msg.combined >> 15) & 0x01));
    print_remaining_message(msg.data_0, msg.data_1, msg.data_2);
}


void RF24Radio::print_message(const DecodedMessage& msg) {
    log("from_who:\t%i\n", msg.from_who);
    log("to_who:\t\t%i\n", msg.to_who);
    log("state:\t\t%d\n", msg.state);
    print_remaining_message(msg.data_0, msg.data_1, msg.data_2);
}


void RF24Radio::print_remaining_message(uint16_t data_0, uint16_t data_1, uint16_t data_2) {
    log("data_0:\t\t%i\n", data_0);
    log("data_1:\t\t%i\n", data_1);
    log("data_2:\t\t%i", data_2);

    if (data_2 == 0XFFFF) {
        log(" - request time");
    } else if (data_2 == 0XFFFE) {
        log(" - resend message");
    }
    log("\n\n");
}


void RF24Radio::print_P_variant() {
    // Real NRF24L01+ variant or not. NRF24L01 (non +)
    log("NRF24L01+: %s\n", RF24::isPVariant() ? "true" : "false"); 
}


uint16_t RF24Radio::test_send_time_to_node(uint8_t sending_node_ID, uint8_t test_node_ID) {
    if (sending_node_ID != test_node_ID) {
        return 1200; // Dummy time for other nodes while testing
    }
    static int count = 0;
    count++;
    log("test case [%i] = ", count);

    // hh:mm (Tests for 9am wakeup) 
    switch (count) {
    case 1:
        return 1200;
    case 2:
        return 2059;
    case 3:
        return 2300;
    case 4:
        return 2400;
    case 5:
        return 0000;
    case 6:
        return 859;
    case 7:
        return 900; 
    case 8:
        WDT_FEED();
        log("delay(5000)... ");
        delay(5000);
        return 1200;
    case 9:
        return 709;
    case 10:
        count = 0;
        return 5555;
    default:
        return 0;
    }
}
