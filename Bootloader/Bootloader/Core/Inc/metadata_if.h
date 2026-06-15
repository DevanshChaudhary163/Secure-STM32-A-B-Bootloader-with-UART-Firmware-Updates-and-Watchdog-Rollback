#ifndef METADATA_IF_H
#define METADATA_IF_H

#include "main.h"
#include <stdint.h>

#define METADATA_ADDRESS   0x08010000U
#define METADATA_SECTOR    FLASH_SECTOR_4

#define METADATA_MAGIC     0xB00710ADU
#define METADATA_VERSION   1U

#define BOOT_SLOT_A        1U
#define BOOT_SLOT_B        2U

#define SLOT_STATE_EMPTY         0U
#define SLOT_STATE_VALID         1U
#define SLOT_STATE_PENDING_TEST  2U
#define SLOT_STATE_BAD           3U

typedef struct
{
    uint32_t magic;
    uint32_t version;

    uint32_t active_slot;

    uint32_t slot_a_state;
    uint32_t slot_b_state;

    uint32_t slot_a_size;
    uint32_t slot_b_size;

    uint32_t slot_a_crc;
    uint32_t slot_b_crc;

    uint32_t boot_attempts;

    uint32_t reserved[5];

} boot_metadata_t;

uint8_t Metadata_IsValid(const boot_metadata_t *metadata);
void Metadata_GetDefault(boot_metadata_t *metadata);
void Metadata_Read(boot_metadata_t *metadata);
HAL_StatusTypeDef Metadata_Write(const boot_metadata_t *metadata);
HAL_StatusTypeDef Metadata_MarkSlotAPending(uint32_t slot_a_size);
HAL_StatusTypeDef Metadata_MarkSlotAValid(void);
HAL_StatusTypeDef Metadata_InitDefault(void);
HAL_StatusTypeDef Metadata_MarkSlotBPending(uint32_t slot_b_size);
HAL_StatusTypeDef Metadata_MarkSlotAActive(void);
HAL_StatusTypeDef Metadata_MarkSlotBValid(void);
HAL_StatusTypeDef Metadata_SetBootAttempts(uint32_t attempts);
HAL_StatusTypeDef Metadata_RollbackFromPending(void);

const char *Metadata_SlotName(uint32_t slot);
const char *Metadata_StateName(uint32_t state);

#endif
