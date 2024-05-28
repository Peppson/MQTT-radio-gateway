
#include "classes/MQTT_gateway.h"
#include "classes/hardware.h"
#include "secrets/secrets.h"

bool MQTTGateway::_is_time_set = false;
RF24Radio* MQTTGateway::_radio_ptr;


bool MQTTGateway::init(RF24Radio* radio) {
    _radio_ptr = radio;

    // Ethernet
    WiFi.onEvent(ETH_event);
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
    ETH.config(THIS_DEV_IP, ROUTER_IP, IPAddress(255, 255, 255, 0));
    WiFi.mode(WIFI_OFF);

    // Test ethernet connection
    if (!test_ETH_connection(ROUTER_IP_TEST, ROUTER_PORT_TEST)) {
        log("Router \tOffline! \n");  
    }

    // MQTT init. Lambda to get around "onMessage()" static func* parameter
    MQTTClient::begin(SECRET_MQTT_BROKER_IP, SECRET_MQTT_BROKER_PORT, _ETH_client); // void
    MQTTClient::onMessage([this](const String& topic, const String& payload) { 
        this->route_incoming_MQTT(topic, payload);
    });

    return connect_to_MQTT_broker();
}


bool MQTTGateway::connect_to_MQTT_broker() {
    uint32_t end_time = millis() + 15*1000;

    // Wait for connection
    while (!connect(SECRET_MQTT_CLIENT_ID, SECRET_MQTT_BROKER_NAME, SECRET_MQTT_BROKER_PASSWORD)) {
        WDT_FEED();
        log(".");
        delay(500);
        if (millis() > end_time) {
            return false;
        }
    }

    // Subscribe to MQTT topics
    bool subscribed = true;
    int QOS_level = 2;
    for (const auto& topic : _subscribed_MQTT_topics) {
        if (!subscribe(topic, QOS_level)) {
            subscribed = false;
        }
    }

    return subscribed;
}


void MQTTGateway::route_incoming_MQTT(const String& topic, const String& payload) {
    log("\n------------------------ MQTT ------------------------\n");
    log("Topic:\t\t%s\nPayload:\t\t%s\n", topic.c_str(), payload.c_str());

    try {
        uint16_t length = payload.length();

        // Radio node
        if (topic.substring(0, 9) == "/to/rf24/" && length > 5 && length < 100) {         
            MQTT_to_radio_nodes(topic, payload);
        // 433Mhz radio
        } else if (topic.substring(0, 10) == "/to/rf433/" && length > 0 && length <= 100) {
            MQTT_to_RF433_radio(topic);
        // Master (this dev) 
        } else if (topic.substring(0, 10) == "/to/master" && length <= 20) {
            MQTT_to_master(payload);
        // Set current time
        } else if (topic.substring(0, 18) == "/from/current_time" && length == 10) {
            set_current_time(payload);
        }

    } catch (...) {
        log("Error! Caught in %s() \n", __func__); 
    }
} 


void MQTTGateway::radio_data_to_DB(RF24Radio::DecodedMessage& message) { 
    uint8_t node_ID = message.from_who;
    log("Sending Node %i data to database!\n", node_ID);
    
    // Prepare JSON
    StaticJsonDocument<128> JSON_doc;
    if (!create_JSON_document(node_ID, message, JSON_doc)) {
        log("Failed to create JSON doc\n");
        return;
    }

    String topic = "/to/db/from/rf24/" + String(node_ID);
    String payload;

    // Send JSON over MQTT to database (Influx DB)
    size_t bytes_written = serializeJson(JSON_doc, payload);
    if (bytes_written == 0) {
        log("Failed to serialize JSON\n");
        return;
    }
    publish(topic.c_str(), payload.c_str(), false, 2);
} 


bool MQTTGateway::get_node_ID(const String& topic, uint16_t& node_ID) {
    static_assert(std::char_traits<char>::length(MQTTGateway::_subscribed_MQTT_topics[1]) == 10, "/to/rf24/#");
    const uint16_t length = topic.length();
    String num;

    // Extract ID from String "/to/rf24/<num>"
    if (length >= 10 && length <= 12) {
        num = topic.substring(9, length);
    } else {
        return false;
    }

    // Check that num is digit(s)
    for (size_t i = 0; i < (length - 9); i++) {
        if (!isdigit(num[i])) {
            return false;
        }
    }
    
    node_ID = num.toInt();
    if (node_ID == 0 || node_ID > 255) {
        return false;
    }
    return true;
}


void MQTTGateway::get_time_from_broker(const bool await_response) {
    publish("/to/current_time", "get", false, 2);

    if (await_response) {
        uint32_t end_time = millis() + 10*1000;
        WDT_FEED();

        while (!_is_time_set && millis() < end_time) {
            loop();
            delay(1);
        }
    }
}


void MQTTGateway::connect_or_reboot() {
    log("\nMQTT not connected\n");
    _radio_ptr->stopListening();

    // Try connecting
    for (size_t i = 0; i < 3; i++) {
        if (connect_to_MQTT_broker()) {
            log("\nMQTT connected\n");
            _radio_ptr->startListening();
            return;
        }
        WDT_FEED();
        delay(5000);
    } 

    // No connection
    ESP.restart(); 
}



////// Private //////



void MQTTGateway::MQTT_to_radio_nodes(const String& topic, const String& payload) { 
    uint16_t node_ID;
    
    // Get node ID 
    if (!get_node_ID(topic, node_ID)) {
        log("Invalid Node ID!\n");
        return;
    }

    // Reboot command
    if (payload == "restart" || payload == "reboot") {
        _radio_ptr->send_message(node_ID, 0, 0, 0, 0xFFFF);
        return;
    }

    // Parse the JSON data from MQTT
    StaticJsonDocument<128> JSON_doc;
    DeserializationError error = deserializeJson(JSON_doc, payload);
    if (error) {
        log("Failed to parse JSON");
        return;
    }

    // Radio package
    uint16_t state = JSON_doc["state"];          // Bool
    uint16_t data_0 = JSON_doc["data_0"];        // Data 0
    uint16_t data_1 = JSON_doc["data_1"];        // Data 1
    uint16_t data_2 = JSON_doc["data_2"];        // 0xFFFE = resend last message. 0xFFFF = reboot

    // Send
    _radio_ptr->send_message(node_ID, state, data_0, data_1, data_2);
}


void MQTTGateway::MQTT_to_master(const String& payload) {
    if (payload == "reboot" || payload == "restart") {
        publish("/from/master", "Rebooting...", false, 2); 
        ESP.restart();

    // What time to send to Nodes. Real or mocked
    } else if (payload == "1" || payload == "true" || payload == "True") {
        publish("/from/master", "Debug time set to 12:00", false, 2);
        log("Debug time set to 12:00\n");
        debug_time_enabled = true;

    } else if (payload == "0" || payload == "false" || payload == "False") {
        publish("/from/master", "Back to current time", false, 2);
        log("Back to current time: %02i:%02i:%02i \n", hour(), minute(), second());
        debug_time_enabled = false;
     
    } else {
        publish("/from/master", "Unknown command", false, 2);
        log("Unknown command\n");
    }
}


void MQTTGateway::MQTT_to_RF433_radio(const String& topic) {
    uint8_t code;

    if (!Hardware::get_RF433_code(code, topic)) {
        return;
    }
    log("Sending RF433 code: %i\n", code);
    Hardware::send_RF433_code(code);
}


void MQTTGateway::set_current_time(const String& payload) {
    // Verify 10 digits
    for (size_t i = 0; i < 10; ++i) {
        if (!isdigit(payload[i])) {
            log("Bad payload: expected 10-digit num, got '%s'\n", payload.c_str());
            return;
        }
    }
    // Set time
    setTime(payload.toInt());
    prev_sync_time = millis();
    _is_time_set = true;
    log("Time set:\t%02i:%02i:%02i \n", hour(), minute(), second());
}


bool MQTTGateway::test_ETH_connection(const char* host, const uint16_t router_port) {
    log("\n---------------------- Ethernet ----------------------\n");
    log("Connecting to %s \n", host);

    if (!_ETH_client.connect(host, router_port)) {
        log("Connection failed \n");
        return false;
    }

    _ETH_client.printf("GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
    WDT_FEED();

    while (_ETH_client.connected() && !_ETH_client.available()) { }
        while (_ETH_client.available()) {
            Serial.write(_ETH_client.read());
        }
    _ETH_client.stop();
    return true;
}


void MQTTGateway::ETH_event(WiFiEvent_t event) {
    // Copy pasta
    switch (event) {
        case SYSTEM_EVENT_ETH_START:
            log("ETH Started \n");
            ETH.setHostname("esp32-master");
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            log("ETH Connected \n");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            log("ETH MAC: ");
            log("%s", ETH.macAddress().c_str());
            if (ETH.fullDuplex()) { log(", FULL_DUPLEX \n"); }
            log(", ");
            log("%i", ETH.linkSpeed());
            log("Mbps \n");
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            log("ETH Disconnected \n");
            break;
        case SYSTEM_EVENT_ETH_STOP:
            log("ETH Stopped \n");
            break;
        default:
            break;
    }
}


bool MQTTGateway::create_JSON_document(const uint8_t node_ID, const RF24Radio::DecodedMessage& message, StaticJsonDocument<128>& JSON_doc) {
    JSON_doc["node_ID"] = node_ID;

    // Self watering plants, solar/battery driven
    if (node_ID <= 5) {
        JSON_doc["state"] = 0;
        JSON_doc["data_0"] = message.data_0;   
        JSON_doc["data_1"] = Hardware::calculate_node_battery_charge(node_ID, message.data_1);
        JSON_doc["data_2"] = 0; 

    // Self watering plants, USB driven
    } else if (node_ID <= 10) {
        JSON_doc["state"] = 0;
        JSON_doc["data_0"] = message.data_0;   
        JSON_doc["data_1"] = 0;
        JSON_doc["data_2"] = 0; 

    // Remaining nodes
    } else switch (node_ID) {

        // Mocca master controller
        case 11:
            JSON_doc["state"] = message.state;
            JSON_doc["data_0"] = message.data_0;   
            JSON_doc["data_1"] = 0;
            JSON_doc["data_2"] = 0; 
            break;

        // Debug
        case 255:
            JSON_doc["state"] = message.state;
            JSON_doc["data_0"] = message.data_0;   
            JSON_doc["data_1"] = message.data_1;
            JSON_doc["data_2"] = message.data_2; 
            break;

        default:
            return false;
    }

    return true;
}
