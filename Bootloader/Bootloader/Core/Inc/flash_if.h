#ifndef FLASH_IF_H
#define FLASH_IF_H

#include "main.h"
#include <stdint.h>

#define FLASH_TEST_ADDRESS   0x08060000U
#define FLASH_TEST_SECTOR    FLASH_SECTOR_7
#define FLASH_TEST_VALUE     0x12345678U
#define SLOT_A_ADDRESS       0x08020000U
#define SLOT_A_SECTOR        FLASH_SECTOR_5
#define SLOT_A_SIZE          (128U * 1024U)
#define SLOT_B_ADDRESS       0x08040000U
#define SLOT_B_SECTOR        FLASH_SECTOR_6
#define SLOT_B_SIZE          (128U * 1024U)

HAL_StatusTypeDef Flash_Erase_Test_Sector(void);
HAL_StatusTypeDef Flash_Erase_Slot_A(void);
HAL_StatusTypeDef Flash_Erase_Slot_B(void);
HAL_StatusTypeDef Flash_Write_Word(uint32_t address, uint32_t data);
HAL_StatusTypeDef Flash_Write_Buffer(uint32_t address, uint8_t *data, uint32_t length);
uint32_t Flash_Read_Word(uint32_t address);
uint8_t Flash_Test(void);

#endif
