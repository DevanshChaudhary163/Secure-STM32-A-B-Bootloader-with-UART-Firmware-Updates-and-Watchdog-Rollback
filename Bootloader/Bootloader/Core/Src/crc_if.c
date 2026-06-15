#include "crc_if.h"

#define CRC32_POLY 0xEDB88320U

uint32_t CRC32_Calculate_Buffer(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFU;

    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= data[i];

        for (uint32_t bit = 0; bit < 8; bit++)
        {
            if (crc & 1U)
            {
                crc = (crc >> 1) ^ CRC32_POLY;
            }
            else
            {
                crc = crc >> 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;
}

uint32_t CRC32_Calculate_Flash(uint32_t flash_address, uint32_t length)
{
    const uint8_t *flash_data = (const uint8_t *)flash_address;

    return CRC32_Calculate_Buffer(flash_data, length);
}
