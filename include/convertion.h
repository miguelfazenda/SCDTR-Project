#ifndef CONVERTION_H
#define CONVERTION_H

#include <Arduino.h>

union Convertion
{
    uint16_t value16b;
    uint32_t value32b;
    uint8_t valueBytes[4];
    float valueFloat;
};

#endif