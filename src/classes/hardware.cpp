
#include "classes/hardware.h"
#include "rf433_radio_codes.h"
#include "radio_nodes_voltage_limits.h"


void Hardware::init() {
    delay(1000); // Allow things to settle down

    #if SERIAL_ENABLED
        Serial.begin(SERIAL_BAUD_RATE);
        while (!Serial && millis() < 5*1000) { }
        delay(50);
        log("\n\n"); 
    #endif

    // Physical I/O 
    pinMode(RF433_TX_PIN, OUTPUT);
    pinMode(RF433_RX_PIN, INPUT_PULLDOWN);
    pinMode(ERROR_LED, OUTPUT);
    digitalWrite(ERROR_LED, LOW);
    digitalWrite(RF433_TX_PIN, LOW);

    // Watchdog
    #if WATCHDOG_ENABLE
        esp_task_wdt_init(WATCHDOG_TIMEOUT, true);
        esp_task_wdt_add(NULL);
    #endif   
}


void Hardware::print_info() {
    log("\n------------------------ Info ------------------------\n");
    log("Watchdog:\t   %s\n", WATCHDOG_ENABLE ? "true" : "false");
    log("Debug time:\t   %s\n", DEBUG_TIME_ENABLED ? "true" : "false");
    log("Debug time tests:   %s\n", DEBUG_TIME_TESTS ? "true" : "false");
    log("Router IP:\t   %s\n", ROUTER_IP_TEST);
    log("\n");
}


void Hardware::error() {
    log("\n----------------------- Error! -----------------------\n");
    digitalWrite(ERROR_LED, HIGH);

    // Do nothing for 2 min then reboot, ah yes good error handling!
    do_nothing_loop(2*60*1000);
    ESP.restart();
}


void Hardware::reboot_once_a_week() {
    bool monday = (weekday() == 2);
    if (!monday) { 
        return; 
    } 

    // Reboot at 05:00 AM
    uint16_t current_minute = (hour() * 60) + minute(); 
    if (current_minute >= 300 && current_minute <= 301) {
        do_nothing_loop(2*60*1000); // Delay for 2 minutes to prevent immediate boot looping
        ESP.restart();
    } 
}


void Hardware::do_nothing_loop(const uint32_t duration_ms) {
    log("%s()\n", __func__);
    uint32_t end_time = millis() + duration_ms;

    while (millis() < end_time) {
        WDT_FEED();
        delay(1000);
    }
}


uint16_t Hardware::calculate_node_battery_charge(uint8_t node_ID, uint16_t node_voltage) {
    uint16_t volt_max = 0;
    uint16_t volt_min = 0;
    
    // Measured battery voltage (ADC reading, 0-1023) for each battery driven node
    if (!get_node_voltage_limits(node_ID, volt_max, volt_min)) {
        return 0;
    }

    // Calculate remaining battery charge 
    uint16_t voltage = node_voltage;
    uint16_t volt_85 = volt_max * 0.95;
    uint16_t volt_10 = volt_max * 0.8;
    uint16_t bat_percent;

    // Li-ion batteries do not discharge linearly!
    // https://www.icode.com/how-to-measure-and-display-battery-level-on-micro-controller-arduino-projects/

    // 100%
    if (voltage >= volt_max) bat_percent = 100;
    // 85-100% 
    else if (voltage >= volt_85)    
        bat_percent = map(voltage, volt_85, volt_max, 85, 100);
    // 10-85% 
    else if (voltage >= volt_10)    
        bat_percent = map(voltage, volt_10, volt_85, 10, 85);
    // 0-10% 
    else if (voltage >= volt_min)    
        bat_percent = map(voltage, volt_min, volt_10, 0, 10);
    // 0%
    else bat_percent = 0;

    return bat_percent;
}


bool Hardware::get_node_voltage_limits(const uint8_t node_ID, uint16_t& volt_max, uint16_t& volt_min) {
    for (const auto &value : RADIO_NODES_VOLT_LIMITS) {
        if (value[0] == node_ID) {
            volt_max = value[1];
            volt_min = value[2];
            return true;
        }       
    }

    log("\n\n\n\n\n----- %s() -----\nNode doesn't exist! Add Node here? \n", __func__); 
    return false;
}


void Hardware::send_RF433_code(const uint8_t code) {
    // Loop through the "remote code" array 
    for (size_t i = 0; i < RF433_TRANSMISSION_COUNT; i++) {
        for (size_t j = 0; j < 131; j += 2) {      
            uint16_t high_pulse = RF433_REMOTE_CODES[code][j];
            uint16_t low_pulse = RF433_REMOTE_CODES[code][j + 1];

            // High pulse
            digitalWrite(RF433_TX_PIN, HIGH);
            delayMicroseconds(high_pulse);

            // Low pulse
            digitalWrite(RF433_TX_PIN, LOW);
            delayMicroseconds(low_pulse);
        }
        // Low pulse between messages
        delayMicroseconds(9300); 
    }
}


bool Hardware::get_RF433_code(uint8_t& code, const String& topic) {
    uint16_t length = topic.length();

    // Verify "/to/rf433/<num>"
    String num = "";
    if (length >= 11 && length <= 12) { 
        num = topic.substring(10, length);
    } else {
        log("Bad topic length: expected /to/rf433/<num>, got '%s'\n", topic.c_str());
        return false;
    }

    // Digits?
    uint8_t digits = length - 10;
    for (size_t i = 0; i < digits; ++i) {
        if (!isdigit(num[i])) {
            log("Bad topic: expected digits, got '%s'\n", topic.c_str());
            return false;
        }
    }

    // Valid remote code?
    code = num.toInt();
    constexpr uint8_t num_of_codes = sizeof(RF433_REMOTE_CODES) / sizeof(RF433_REMOTE_CODES[0]);
    if (code >= num_of_codes) {
        log("Invalid code: expected 0-%i, got '%i'\n", (num_of_codes - 1), code);
        return false;
    }

    return true;
}


void Hardware::benchmark(const char* name) {
    static bool begin_timer = true;
    static uint32_t start_time = 0;

    // Start timer
	if (begin_timer) {
        begin_timer = false;
		start_time = micros();
        return;
	}

    // Stop timer + output
    uint32_t end_time = micros() - start_time;
    String num_str = String(end_time);

    // Name (if any)
    if (name != nullptr) { 
        log("%s: ", name); 
    }
    
    // Microseconds
    if (num_str.length() < 4) {
        log("%iμs \n", end_time);

    // Milliseconds
    } else if (num_str.length() < 6) {
        float end_ms = (float)end_time / 1000;
        log("%.3fms \n", end_ms);
    } else {
        log("%ims \n", end_time / 1000);
    }

    // Reset
    begin_timer = true;
    start_time = 0;	
}
