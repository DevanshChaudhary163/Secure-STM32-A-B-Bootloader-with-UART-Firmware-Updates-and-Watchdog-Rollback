#include "metadata_if.h"
#include <string.h>

static HAL_StatusTypeDef Metadata_Erase_Sector(void)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error = 0;

    HAL_FLASH_Unlock();

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = METADATA_SECTOR;
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    status = HAL_FLASHEx_Erase(&erase_init, &sector_error);

    HAL_FLASH_Lock();

    return status;
}

uint8_t Metadata_IsValid(const boot_metadata_t *metadata)
{
    if (metadata->magic != METADATA_MAGIC)
    {
        return 0;
    }

    if (metadata->version != METADATA_VERSION)
    {
        return 0;
    }

    if (metadata->active_slot != BOOT_SLOT_A &&
        metadata->active_slot != BOOT_SLOT_B)
    {
        return 0;
    }

    return 1;
}

void Metadata_GetDefault(boot_metadata_t *metadata)
{
    memset(metadata, 0, sizeof(boot_metadata_t));

    metadata->magic = METADATA_MAGIC;
    metadata->version = METADATA_VERSION;

    metadata->active_slot = BOOT_SLOT_A;

    metadata->slot_a_state = SLOT_STATE_VALID;
    metadata->slot_b_state = SLOT_STATE_EMPTY;

    metadata->slot_a_size = 0;
    metadata->slot_b_size = 0;

    metadata->slot_a_crc = 0;
    metadata->slot_b_crc = 0;

    metadata->boot_attempts = 0;
}

void Metadata_Read(boot_metadata_t *metadata)
{
    memcpy(metadata, (void *)METADATA_ADDRESS, sizeof(boot_metadata_t));
}

HAL_StatusTypeDef Metadata_Write(const boot_metadata_t *metadata)
{
    HAL_StatusTypeDef status;
    uint32_t address = METADATA_ADDRESS;
    const uint32_t *data_words = (const uint32_t *)metadata;
    uint32_t word_count = sizeof(boot_metadata_t) / 4U;

    status = Metadata_Erase_Sector();

    if (status != HAL_OK)
    {
        return status;
    }

    HAL_FLASH_Unlock();

    for (uint32_t i = 0; i < word_count; i++)
    {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   address,
                                   data_words[i]);

        if (status != HAL_OK)
        {
            break;
        }

        address += 4U;
    }

    HAL_FLASH_Lock();

    return status;
}

HAL_StatusTypeDef Metadata_InitDefault(void)
{
    boot_metadata_t metadata;

    Metadata_GetDefault(&metadata);

    return Metadata_Write(&metadata);
}

HAL_StatusTypeDef Metadata_MarkSlotAPending(uint32_t slot_a_size)
{
    boot_metadata_t metadata;

    Metadata_Read(&metadata);

    if (!Metadata_IsValid(&metadata))
    {
        Metadata_GetDefault(&metadata);
    }

    metadata.active_slot = BOOT_SLOT_A;

    metadata.slot_a_state = SLOT_STATE_PENDING_TEST;
    metadata.slot_b_state = SLOT_STATE_VALID;

    metadata.slot_a_size = slot_a_size;
    metadata.boot_attempts = 0;

    return Metadata_Write(&metadata);
}

HAL_StatusTypeDef Metadata_MarkSlotBPending(uint32_t slot_b_size)
{
    boot_metadata_t metadata;

    Metadata_Read(&metadata);

    if (!Metadata_IsValid(&metadata))
    {
        Metadata_GetDefault(&metadata);
    }

    metadata.active_slot = BOOT_SLOT_B;

    metadata.slot_a_state = SLOT_STATE_VALID;
    metadata.slot_b_state = SLOT_STATE_PENDING_TEST;

    metadata.slot_b_size = slot_b_size;
    metadata.boot_attempts = 0;

    return Metadata_Write(&metadata);
}

HAL_StatusTypeDef Metadata_MarkSlotAValid(void)
{
    boot_metadata_t metadata;

    Metadata_Read(&metadata);

    if (!Metadata_IsValid(&metadata))
    {
        Metadata_GetDefault(&metadata);
    }

    metadata.active_slot = BOOT_SLOT_A;
    metadata.slot_a_state = SLOT_STATE_VALID;
    metadata.boot_attempts = 0;

    return Metadata_Write(&metadata);
}

HAL_StatusTypeDef Metadata_MarkSlotBValid(void)
{
    boot_metadata_t metadata;

    Metadata_Read(&metadata);

    if (!Metadata_IsValid(&metadata))
    {
        Metadata_GetDefault(&metadata);
    }

    metadata.active_slot = BOOT_SLOT_B;
    metadata.slot_b_state = SLOT_STATE_VALID;
    metadata.boot_attempts = 0;

    return Metadata_Write(&metadata);
}

HAL_StatusTypeDef Metadata_MarkSlotAActive(void)
{
    boot_metadata_t metadata;

    Metadata_Read(&metadata);

    if (!Metadata_IsValid(&metadata))
    {
        Metadata_GetDefault(&metadata);
    }

    metadata.active_slot = BOOT_SLOT_A;
    metadata.slot_a_state = SLOT_STATE_VALID;
    metadata.boot_attempts = 0;

    return Metadata_Write(&metadata);
}

HAL_StatusTypeDef Metadata_SetBootAttempts(uint32_t attempts)
{
    boot_metadata_t metadata;

    Metadata_Read(&metadata);

    if (!Metadata_IsValid(&metadata))
    {
        Metadata_GetDefault(&metadata);
    }

    metadata.boot_attempts = attempts;

    return Metadata_Write(&metadata);
}

HAL_StatusTypeDef Metadata_RollbackFromPending(void)
{
    boot_metadata_t metadata;

    Metadata_Read(&metadata);

    if (!Metadata_IsValid(&metadata))
    {
        Metadata_GetDefault(&metadata);
        return Metadata_Write(&metadata);
    }

    /*
     * Case 1:
     * Slot A was pending and failed.
     * Roll back to Slot B if Slot B is valid.
     */
    if (metadata.active_slot == BOOT_SLOT_A &&
        metadata.slot_a_state == SLOT_STATE_PENDING_TEST &&
        metadata.slot_b_state == SLOT_STATE_VALID)
    {
        metadata.slot_a_state = SLOT_STATE_BAD;
        metadata.active_slot = BOOT_SLOT_B;
        metadata.boot_attempts = 0;

        return Metadata_Write(&metadata);
    }

    /*
     * Case 2:
     * Slot B was pending and failed.
     * Roll back to Slot A if Slot A is valid.
     */
    if (metadata.active_slot == BOOT_SLOT_B &&
        metadata.slot_b_state == SLOT_STATE_PENDING_TEST &&
        metadata.slot_a_state == SLOT_STATE_VALID)
    {
        metadata.slot_b_state = SLOT_STATE_BAD;
        metadata.active_slot = BOOT_SLOT_A;
        metadata.boot_attempts = 0;

        return Metadata_Write(&metadata);
    }

    return HAL_OK;
}

const char *Metadata_SlotName(uint32_t slot)
{
    if (slot == BOOT_SLOT_A)
    {
        return "SLOT_A";
    }
    else if (slot == BOOT_SLOT_B)
    {
        return "SLOT_B";
    }
    else
    {
        return "UNKNOWN";
    }
}

const char *Metadata_StateName(uint32_t state)
{
    if (state == SLOT_STATE_EMPTY)
    {
        return "EMPTY";
    }
    else if (state == SLOT_STATE_VALID)
    {
        return "VALID";
    }
    else if (state == SLOT_STATE_PENDING_TEST)
    {
        return "PENDING_TEST";
    }
    else if (state == SLOT_STATE_BAD)
    {
        return "BAD";
    }
    else
    {
        return "UNKNOWN";
    }
}
