
#pragma once 
#include "config.h"

// Measured battery ADC reading for each battery driven node

// node_ID, volt_max, volt_min
constexpr const uint16_t RADIO_NODES_VOLT_LIMITS[][3] = {
    {1, 973, 766},
    {2, 222, 222},
    {3, 333, 333},
    {4, 444, 444},
    {255, 999, 999},
};
