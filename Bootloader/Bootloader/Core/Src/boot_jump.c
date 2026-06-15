#include "boot_jump.h"

#define FLASH_END_ADDR   0x08080000U

#define SRAM_START_ADDR  0x20000000U
#define SRAM_END_ADDR    0x20018000U   // STM32F401RE has 96 KB SRAM

typedef void (*app_entry_t)(void);

static int valid_stack_pointer(uint32_t sp)
{
    return (sp >= SRAM_START_ADDR) && (sp <= SRAM_END_ADDR);
}

static int valid_reset_handler(uint32_t reset_handler)
{
    return (reset_handler >= SLOT_A_ADDRESS) &&
           (reset_handler < FLASH_END_ADDR);
}

void jump_to_address(uint32_t app_address)
{
    uint32_t app_stack = *(volatile uint32_t *)app_address;
    uint32_t app_reset = *(volatile uint32_t *)(app_address + 4U);

    if (!valid_stack_pointer(app_stack))
    {
        return;
    }

    if (!valid_reset_handler(app_reset))
    {
        return;
    }

    HAL_DeInit();

    __disable_irq();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (uint32_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    SCB->VTOR = app_address;

    __set_MSP(app_stack);

    __enable_irq();

    app_entry_t app_entry = (app_entry_t)app_reset;
    app_entry();

    while (1)
    {
    }
}

void jump_to_app(void)
{
    jump_to_address(SLOT_A_ADDRESS);
}
