#ifndef CRC_IF_H
#define CRC_IF_H

#include <stdint.h>

uint32_t CRC32_Calculate_Buffer(const uint8_t *data, uint32_t length);
uint32_t CRC32_Calculate_Flash(uint32_t flash_address, uint32_t length);

#endif
