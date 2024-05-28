
#pragma once
#include <Arduino.h>
#include <SPI.h>  
#include <nRF24L01.h>  
#include <RF24.h>
#include <TimeLib.h>
#include <ETH.h>
#include <MQTT.h>
#include <ArduinoJson.h> 
#include <esp_task_wdt.h>
#include <stdio.h>
#include <IPAddress.h>


// Dev
#define SERIAL_ENABLED 1                            // Serial logs On/Off   
#define WATCHDOG_ENABLE 1                           // Watchdog toggle
#define DEBUG_TIME_ENABLED 0                        // Send time to nodes as "12:00", can also be changed by MQTT 
#define DEBUG_TIME_TESTS 0                          // Send a series of mock times to nodes (overwrites above)    

// NRF24L01+
#define RF24_THIS_DEV_ADDRESS 0                     // This device address (0)                                        
#define RF24_CHANNEL 120                            // Channel (120)
#define RF24_OUTPUT (RF24_PA_MAX)                   // Output level: RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX, ERROR
#define RF24_DATARATE (RF24_250KBPS)                // Datarate: RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
#define RF24_PIPE 0                                 // Listening pipe (0-5)

// Ethernet                 
#define ROUTER_PORT_TEST 80                         // Router Port. Used when testing connection
#define ROUTER_IP_TEST "192.168.1.1"                // Router IP. Used when testing connection
const IPAddress ROUTER_IP(192, 168, 1, 1);          // Router IP
const IPAddress THIS_DEV_IP(192, 168, 1, 25);       // Set static IP for this device

// Misc
#define SERIAL_BAUD_RATE 115200                     // Baud
#define WATCHDOG_TIMEOUT 60                         // Watchdog timout in seconds
#define LOOP_ITERATIONS 100*1000UL                  // In main loop
#define UPDATE_INTERVAL 14*60*60*1000UL             // How often to grab current time manually
#define RF433_TRANSMISSION_COUNT 5                  // RF433 retries when sending 

// LAN8720 pins
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN    
#define ETH_ADDR        1                      
#define ETH_POWER_PIN   2                        
#define ETH_MDC_PIN     23      
#define ETH_MDIO_PIN    18

// NRF24L01+ pins
#define SPI_1_MISO  12
#define SPI_1_MOSI  13
#define SPI_1_SCLK  14
#define SPI_1_CE    16 
#define SPI_1_CSN   5

// 433Mhz radio pins
#define RF433_TX_PIN 33 
#define RF433_RX_PIN 32

// Error led pin
#define ERROR_LED 4

// Serial 
#if SERIAL_ENABLED
    #define log(...) Serial.printf(__VA_ARGS__)
    #define STOP log("\n-----  Stop right there criminal scum!  ----- \n"); while (1) { WDT_FEED(); delay(100); }
#else 
    #define log(...)
    #define STOP while (1) { delay(100); }
#endif 

// Watchdog
#if WATCHDOG_ENABLE
    #define WDT_FEED() esp_task_wdt_reset()
#else
    #define WDT_FEED()
#endif

/*
######################  LAN8720 Ethernet module - ESP32 R1 R32 Pinout  ######################

    * GPIO02    -   NC (From NC > Osc enable pin on Lan8720 module, ETH_POWER_PIN) 
    * GPIO22    -   TX1                                      
    * GPIO19    -   TX0                                      
    * GPIO21    -   TX_EN                                    
    * GPIO26    -   RX1                                     
    * GPIO25    -   RX0                                      
    * GPIO27    -   CRS                                      
    * GPIO00    -   nINT/REFCLK     (4k7 Pullup > 3.3V)    
    * GPIO23    -   MDC             (ETH_MDC_PIN)                                    
    * GPIO18    -   MDIO            (ETH_MDIO_PIN)                                 
    * 3V3       -   VCC
    * GND       -   GND                                           
    
- ETH_CLOCK_GPIO0_IN   - default: external clock from crystal oscillator
- ETH_CLOCK_GPIO0_OUT  - 50MHz clock from internal APLL output on GPIO00 - possibly an inverter is needed for LAN8720
- ETH_CLOCK_GPIO16_OUT - 50MHz clock from internal APLL output on GPIO16 - possibly an inverter is needed for LAN8720
- ETH_CLOCK_GPIO17_OUT - 50MHz clock from internal APLL inverted output on GPIO17 - tested with LAN8720

- https://github.com/flusflas/esp32-ethernet
- https://github.com/SensorsIot/ESP32-Ethernet/blob/main/EthernetPrototype/EthernetPrototype.ino#L44
*/
