
/*
########################  MQTT-radio-gateway  ########################
#                                                                    #
#   - MQTT bridge for NRF24L01+ nodes and generic 433Mhz radios      #
#   - https://github.com/Peppson/MQTT-radio-gateway                  #
#   - Target Mcu/Board: ESP32 D1 R32                                 #
#   - Uploaded version: 1.1.1                                        #
#   - Build version: 1.1.1                                           #
#                                                                    #
######################################################################
*/

#include "config.h"
#include "classes/hardware.h"
#include "classes/RF24_radio.h"
#include "classes/MQTT_gateway.h"

Hardware hardware;
RF24Radio radio;
MQTTGateway mqtt;


void setup() {
    hardware.init();

    // Init radio and MQTT
    if (radio.init(&mqtt) && mqtt.init(&radio)) {
        mqtt.get_time_from_broker(true);
        radio.startListening();
        hardware.print_info();
    } else {
        hardware.error();
    }
}


void loop() {
    for (size_t i = 0; i < LOOP_ITERATIONS; i++) {
        WDT_FEED();

        // MQTT
        if (mqtt.loop()) {
            // Calls mqtt.route_incoming_MQTT() 
        }
        // 2.4Ghz radio
        if (radio.available()) {
            radio.route_incoming_radio(true);   
        }
    }

    // MQTT/router connected?
    if (!mqtt.connected()) {
        mqtt.connect_or_reboot();    
    } 
    // Get time
    if (millis() - mqtt.prev_sync_time > UPDATE_INTERVAL) {       
        mqtt.get_time_from_broker(); 
    }
    // Reboot each monday at 05:00 AM
    hardware.reboot_once_a_week();
}
