
#pragma once 
#include "config.h"

// Measured battery ADC reading for each battery driven node

// node_ID, volt_max, volt_min
constexpr const uint16_t RADIO_NODES_VOLT_LIMITS[][3] = {
    {1, 899, 730},
    {2, 900, 725},
    {255, 999, 999} // Debug
};
