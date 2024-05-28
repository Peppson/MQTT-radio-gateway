
#pragma once
#include "config.h"
#include "classes/RF24_radio.h"


class MQTTGateway : public MQTTClient {
public:
    MQTTGateway() : debug_time_enabled(DEBUG_TIME_ENABLED), prev_sync_time(0) {};

    bool debug_time_enabled;
    uint32_t prev_sync_time;

    bool init(RF24Radio* radio);
    bool connect_to_MQTT_broker();
    void route_incoming_MQTT(const String& topic, const String& payload);
    void radio_data_to_DB(RF24Radio::DecodedMessage& message);
    bool get_node_ID(const String& topic, uint16_t& node_id);
    void get_time_from_broker(const bool await_response = false);
    void connect_or_reboot();

private:
    WiFiClient _ETH_client;

    static bool _is_time_set;
    static RF24Radio* _radio_ptr;
    static constexpr const char* _subscribed_MQTT_topics[] = {
        "/from/current_time", 
        "/to/rf24/#", 
        "/to/rf433/#",
        "/to/master",
    };

    void MQTT_to_radio_nodes(const String& topic, const String& payload); 
    void MQTT_to_master(const String& payload);
    void MQTT_to_RF433_radio(const String& topic);
    void set_current_time(const String& payload);
    bool test_ETH_connection(const char * host, const uint16_t router_port);
    static void ETH_event(WiFiEvent_t event);
    static inline bool create_JSON_document(const uint8_t node_ID, 
                                            const RF24Radio::DecodedMessage& message, 
                                            StaticJsonDocument<128>& JSON_doc
                                            );
};
