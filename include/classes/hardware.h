
#pragma once
#include "config.h"


class Hardware {
public:
    Hardware() = default;

    static void init();
    static void print_info();
    static void error();
    static void reboot_once_a_week();
    static void do_nothing_loop(const uint32_t duration_ms);
    static uint16_t calculate_node_battery_charge(uint8_t node_id, uint16_t node_voltage);
    static bool get_node_voltage_limits(uint8_t node_ID, uint16_t& volt_max, uint16_t& volt_min);
    static void send_RF433_code(const uint8_t code);
    static bool get_RF433_code(uint8_t& code, const String& topic);
    static void benchmark(const char* name = nullptr); // Timer function. Call before and after measured code
}; 
