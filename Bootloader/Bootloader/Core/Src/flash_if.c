#include "flash_if.h"

HAL_StatusTypeDef Flash_Erase_Test_Sector(void)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error = 0;

    HAL_FLASH_Unlock();

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = FLASH_TEST_SECTOR;
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    status = HAL_FLASHEx_Erase(&erase_init, &sector_error);

    HAL_FLASH_Lock();

    return status;
}
HAL_StatusTypeDef Flash_Erase_Slot_A(void)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error = 0;

    HAL_FLASH_Unlock();

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = SLOT_A_SECTOR;
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    status = HAL_FLASHEx_Erase(&erase_init, &sector_error);

    HAL_FLASH_Lock();

    return status;
}
HAL_StatusTypeDef Flash_Erase_Slot_B(void)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error = 0;

    HAL_FLASH_Unlock();

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = SLOT_B_SECTOR;
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    status = HAL_FLASHEx_Erase(&erase_init, &sector_error);

    HAL_FLASH_Lock();

    return status;
}
HAL_StatusTypeDef Flash_Write_Word(uint32_t address, uint32_t data)
{
    HAL_StatusTypeDef status;

    HAL_FLASH_Unlock();

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data);

    HAL_FLASH_Lock();

    return status;
}

uint32_t Flash_Read_Word(uint32_t address)
{
    return *(volatile uint32_t *)address;
}
HAL_StatusTypeDef Flash_Write_Buffer(uint32_t address, uint8_t *data, uint32_t length)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t current_address = address;
    uint32_t word = 0;

    HAL_FLASH_Unlock();

    for (uint32_t i = 0; i < length; i += 4)
    {
        word = 0xFFFFFFFFU;

        word  = data[i];

        if ((i + 1) < length)
            word |= ((uint32_t)data[i + 1] << 8);

        if ((i + 2) < length)
            word |= ((uint32_t)data[i + 2] << 16);

        if ((i + 3) < length)
            word |= ((uint32_t)data[i + 3] << 24);

        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_address, word);

        if (status != HAL_OK)
        {
            break;
        }

        current_address += 4;
    }

    HAL_FLASH_Lock();

    return status;
}

uint8_t Flash_Test(void)
{
    HAL_StatusTypeDef status;
    uint32_t read_value;

    status = Flash_Erase_Test_Sector();

    if (status != HAL_OK)
    {
        return 0;
    }

    status = Flash_Write_Word(FLASH_TEST_ADDRESS, FLASH_TEST_VALUE);

    if (status != HAL_OK)
    {
        return 0;
    }

    read_value = Flash_Read_Word(FLASH_TEST_ADDRESS);

    if (read_value == FLASH_TEST_VALUE)
    {
        return 1;
    }

    return 0;
}
