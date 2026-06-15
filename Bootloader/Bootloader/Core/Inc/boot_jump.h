#ifndef BOOT_JUMP_H
#define BOOT_JUMP_H

#include "main.h"
#include <stdint.h>

#define SLOT_A_ADDRESS    0x08020000U
#define SLOT_B_ADDRESS    0x08040000U

void jump_to_app(void);
void jump_to_address(uint32_t app_address);

#endif
