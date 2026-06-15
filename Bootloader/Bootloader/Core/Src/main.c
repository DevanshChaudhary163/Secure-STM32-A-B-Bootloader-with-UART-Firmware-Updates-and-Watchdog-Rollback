/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - A/B bootloader with CRC update check
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>
#include "boot_jump.h"
#include "flash_if.h"
#include "metadata_if.h"
#include "crc_if.h"

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN 0 */

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

static uint8_t is_user_button_pressed(void)
{
    return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET;
}

static uint32_t read_u32_le(uint8_t *bytes)
{
    return  ((uint32_t)bytes[0]) |
            ((uint32_t)bytes[1] << 8) |
            ((uint32_t)bytes[2] << 16) |
            ((uint32_t)bytes[3] << 24);
}

static uint8_t is_valid_stack_pointer(uint32_t sp)
{
    return (sp >= 0x20000000U && sp <= 0x20018000U);
}

static uint8_t is_valid_reset_vector_for_slot(uint32_t reset_vector,
                                              uint32_t slot_address,
                                              uint32_t slot_size)
{
    return (reset_vector >= slot_address &&
            reset_vector < (slot_address + slot_size));
}

static void print_metadata(void)
{
    boot_metadata_t metadata;

    Metadata_Read(&metadata);

    if (!Metadata_IsValid(&metadata))
    {
        printf("Metadata: INVALID / EMPTY\r\n");
        return;
    }

    printf("Metadata: VALID\r\n");
    printf("Active slot: %s\r\n", Metadata_SlotName(metadata.active_slot));
    printf("Slot A state: %s\r\n", Metadata_StateName(metadata.slot_a_state));
    printf("Slot B state: %s\r\n", Metadata_StateName(metadata.slot_b_state));
    printf("Slot A size: %lu bytes\r\n", metadata.slot_a_size);
    printf("Slot B size: %lu bytes\r\n", metadata.slot_b_size);
    printf("Boot attempts: %lu\r\n", metadata.boot_attempts);
}

static void handle_update_command(uint8_t fixed_slot_mode, uint32_t fixed_slot)
{
    boot_metadata_t metadata;

    uint8_t size_bytes[4];
    uint8_t crc_bytes[4];

    uint32_t file_size = 0;
    uint32_t expected_crc = 0;
    uint32_t actual_crc = 0;

    uint32_t received = 0;
    uint8_t buffer[128];

    uint32_t target_slot = 0;
    uint32_t target_address = 0;
    uint32_t target_size = 0;

    uint32_t first_word = 0;
    uint32_t reset_vector = 0;

    uint8_t failed = 0;

    Metadata_Read(&metadata);

    if (!Metadata_IsValid(&metadata))
    {
        printf("Metadata invalid. Initializing default metadata...\r\n");

        if (Metadata_InitDefault() != HAL_OK)
        {
            printf("Metadata init failed\r\n");
            failed = 1;
        }

        Metadata_Read(&metadata);
    }

    if (!failed)
    {
        printf("Current active slot: %s\r\n", Metadata_SlotName(metadata.active_slot));

        if (fixed_slot_mode)
        {
            target_slot = fixed_slot;
        }
        else
        {
            if (metadata.active_slot == BOOT_SLOT_A)
            {
                target_slot = BOOT_SLOT_B;
            }
            else if (metadata.active_slot == BOOT_SLOT_B)
            {
                target_slot = BOOT_SLOT_A;
            }
            else
            {
                printf("Invalid active slot in metadata\r\n");
                failed = 1;
            }
        }
    }

    if (!failed)
    {
        if (target_slot == BOOT_SLOT_A)
        {
            target_address = SLOT_A_ADDRESS;
            target_size = SLOT_A_SIZE;
            printf("Target update slot: SLOT_A\r\n");
        }
        else if (target_slot == BOOT_SLOT_B)
        {
            target_address = SLOT_B_ADDRESS;
            target_size = SLOT_B_SIZE;
            printf("Target update slot: SLOT_B\r\n");
        }
        else
        {
            printf("Invalid target slot\r\n");
            failed = 1;
        }
    }

    if (!failed)
    {
        printf("Send file size as 4 bytes...\r\n");

        if (HAL_UART_Receive(&huart2, size_bytes, 4, HAL_MAX_DELAY) != HAL_OK)
        {
            printf("Failed to receive file size\r\n");
            failed = 1;
        }
        else
        {
            file_size = read_u32_le(size_bytes);

            printf("Received file size: %lu bytes\r\n", file_size);

            if (file_size == 0 || file_size > target_size)
            {
                printf("Invalid file size\r\n");
                failed = 1;
            }
        }
    }

    if (!failed)
    {
        printf("Send expected CRC32 as 4 bytes...\r\n");

        if (HAL_UART_Receive(&huart2, crc_bytes, 4, HAL_MAX_DELAY) != HAL_OK)
        {
            printf("Failed to receive CRC32\r\n");
            failed = 1;
        }
        else
        {
            expected_crc = read_u32_le(crc_bytes);
            printf("Expected CRC32: 0x%08lX\r\n", expected_crc);
        }
    }

    if (!failed)
    {
        if (target_slot == BOOT_SLOT_A)
        {
            printf("Erasing Slot A...\r\n");

            if (Flash_Erase_Slot_A() != HAL_OK)
            {
                printf("Slot A erase failed\r\n");
                failed = 1;
            }
            else
            {
                printf("Slot A erase OK\r\n");
            }
        }
        else if (target_slot == BOOT_SLOT_B)
        {
            printf("Erasing Slot B...\r\n");

            if (Flash_Erase_Slot_B() != HAL_OK)
            {
                printf("Slot B erase failed\r\n");
                failed = 1;
            }
            else
            {
                printf("Slot B erase OK\r\n");
            }
        }
    }

    if (!failed)
    {
        printf("Receiving and writing firmware...\r\n");
    }

    while (!failed && received < file_size)
    {
        uint32_t remaining = file_size - received;
        uint32_t chunk_size = remaining;

        if (chunk_size > sizeof(buffer))
        {
            chunk_size = sizeof(buffer);
        }

        if (HAL_UART_Receive(&huart2, buffer, chunk_size, HAL_MAX_DELAY) != HAL_OK)
        {
            printf("UART receive failed\r\n");
            failed = 1;
            break;
        }

        if (Flash_Write_Buffer(target_address + received, buffer, chunk_size) != HAL_OK)
        {
            printf("Flash write failed at 0x%08lX\r\n", target_address + received);
            failed = 1;
            break;
        }

        received += chunk_size;

        if ((received % 1024U) == 0 || received == file_size)
        {
            printf("Written %lu / %lu bytes\r\n", received, file_size);
        }
    }

    if (!failed && received == file_size)
    {
        printf("Firmware write complete\r\n");

        first_word = *(volatile uint32_t *)target_address;
        reset_vector = *(volatile uint32_t *)(target_address + 4U);

        printf("Target slot: %s\r\n", Metadata_SlotName(target_slot));
        printf("Target address: 0x%08lX\r\n", target_address);
        printf("First word:   0x%08lX\r\n", first_word);
        printf("Reset vector: 0x%08lX\r\n", reset_vector);

        if (!is_valid_stack_pointer(first_word))
        {
            printf("Invalid stack pointer. Update rejected.\r\n");
            failed = 1;
        }

        if (!failed && !is_valid_reset_vector_for_slot(reset_vector, target_address, target_size))
        {
            printf("Invalid reset vector for target slot. Update rejected.\r\n");
            failed = 1;
        }
    }

    if (!failed && received == file_size)
    {
        printf("Calculating CRC from flash...\r\n");

        actual_crc = CRC32_Calculate_Flash(target_address, file_size);

        printf("Actual CRC32:   0x%08lX\r\n", actual_crc);
        printf("Expected CRC32: 0x%08lX\r\n", expected_crc);

        if (actual_crc != expected_crc)
        {
            printf("CRC MISMATCH\r\n");
            printf("Update rejected. Metadata not changed.\r\n");
            failed = 1;
        }
        else
        {
            printf("CRC OK\r\n");
        }
    }

    if (!failed && received == file_size)
    {
        if (target_slot == BOOT_SLOT_A)
        {
            printf("Updating metadata: Slot A = PENDING_TEST, active_slot = A\r\n");

            if (Metadata_MarkSlotAPending(file_size) == HAL_OK)
            {
                printf("Metadata update OK\r\n");
                print_metadata();
                printf("UPDATE PASSED\r\n");
            }
            else
            {
                printf("Metadata update FAILED\r\n");
                printf("UPDATE FAILED\r\n");
            }
        }
        else if (target_slot == BOOT_SLOT_B)
        {
            printf("Updating metadata: Slot B = PENDING_TEST, active_slot = B\r\n");

            if (Metadata_MarkSlotBPending(file_size) == HAL_OK)
            {
                printf("Metadata update OK\r\n");
                print_metadata();
                printf("UPDATE PASSED\r\n");
            }
            else
            {
                printf("Metadata update FAILED\r\n");
                printf("UPDATE FAILED\r\n");
            }
        }
    }
    else
    {
        printf("UPDATE FAILED\r\n");
    }
}

static void bootloader_uart_command_loop(void)
{
    uint8_t rx;
    char cmd[32];
    uint32_t index = 0;

    printf("BOOTLOADER MODE ACTIVE\r\n");
    printf("Commands: ping, info, metainfo, metainit, flashtest, recvtest, writetest, recvfile, update, updateb, jump\r\n");
    printf("> ");

    while (1)
    {
        if (HAL_UART_Receive(&huart2, &rx, 1, HAL_MAX_DELAY) == HAL_OK)
        {
            HAL_UART_Transmit(&huart2, &rx, 1, HAL_MAX_DELAY);

            if (rx == '\r' || rx == '\n')
            {
                cmd[index] = '\0';
                printf("\r\n");

                if (strcmp(cmd, "ping") == 0)
                {
                    printf("pong\r\n");
                }
                else if (strcmp(cmd, "info") == 0)
                {
                    printf("Bootloader: STM32F401RE custom bootloader\r\n");
                    printf("Bootloader address: 0x08000000\r\n");
                    printf("Bootloader size: 64 KB\r\n");
                    printf("Metadata address: 0x08010000\r\n");
                    printf("App Slot A address: 0x08020000\r\n");
                    printf("App Slot A size: 128 KB\r\n");
                    printf("App Slot B address: 0x08040000\r\n");
                    printf("App Slot B size: 128 KB\r\n");
                    printf("CRC check: ENABLED\r\n");
                }
                else if (strcmp(cmd, "metainfo") == 0)
                {
                    print_metadata();
                }
                else if (strcmp(cmd, "metainit") == 0)
                {
                    printf("Initializing default metadata...\r\n");

                    if (Metadata_InitDefault() == HAL_OK)
                    {
                        printf("Metadata init OK\r\n");
                        print_metadata();
                    }
                    else
                    {
                        printf("Metadata init FAILED\r\n");
                    }
                }
                else if (strcmp(cmd, "flashtest") == 0)
                {
                    uint32_t read_value;

                    printf("Erasing test sector...\r\n");

                    if (Flash_Erase_Test_Sector() != HAL_OK)
                    {
                        printf("Erase failed\r\n");
                    }
                    else
                    {
                        printf("Erase OK\r\n");
                        printf("Writing test word...\r\n");

                        if (Flash_Write_Word(FLASH_TEST_ADDRESS, FLASH_TEST_VALUE) != HAL_OK)
                        {
                            printf("Write failed\r\n");
                        }
                        else
                        {
                            printf("Write OK\r\n");

                            read_value = Flash_Read_Word(FLASH_TEST_ADDRESS);

                            printf("Read back: 0x%08lX\r\n", read_value);

                            if (read_value == FLASH_TEST_VALUE)
                            {
                                printf("Flash test PASSED\r\n");
                            }
                            else
                            {
                                printf("Flash test FAILED\r\n");
                            }
                        }
                    }
                }
                else if (strcmp(cmd, "recvtest") == 0)
                {
                    uint8_t buffer[16];

                    printf("Send 16 bytes now...\r\n");

                    if (HAL_UART_Receive(&huart2, buffer, 16, HAL_MAX_DELAY) == HAL_OK)
                    {
                        printf("\r\nReceived 16 bytes\r\n");
                        printf("Data:\r\n");

                        for (int i = 0; i < 16; i++)
                        {
                            printf("%02X ", buffer[i]);
                        }

                        printf("\r\nReceive test PASSED\r\n");
                    }
                    else
                    {
                        printf("\r\nReceive test FAILED\r\n");
                    }
                }
                else if (strcmp(cmd, "writetest") == 0)
                {
                    uint8_t buffer[16];

                    printf("Erasing test sector...\r\n");

                    if (Flash_Erase_Test_Sector() != HAL_OK)
                    {
                        printf("Erase failed\r\n");
                    }
                    else
                    {
                        printf("Erase OK\r\n");
                        printf("Send 16 bytes now...\r\n");

                        if (HAL_UART_Receive(&huart2, buffer, 16, HAL_MAX_DELAY) != HAL_OK)
                        {
                            printf("\r\nReceive failed\r\n");
                        }
                        else
                        {
                            printf("\r\nWriting received bytes to flash...\r\n");

                            if (Flash_Write_Buffer(FLASH_TEST_ADDRESS, buffer, 16) != HAL_OK)
                            {
                                printf("Write failed\r\n");
                            }
                            else
                            {
                                uint8_t passed = 1;

                                printf("Write OK\r\n");
                                printf("Reading back from flash:\r\n");

                                for (int i = 0; i < 16; i++)
                                {
                                    uint8_t flash_byte = *(volatile uint8_t *)(FLASH_TEST_ADDRESS + i);

                                    printf("%02X ", flash_byte);

                                    if (flash_byte != buffer[i])
                                    {
                                        passed = 0;
                                    }
                                }

                                printf("\r\n");

                                if (passed)
                                {
                                    printf("Write test PASSED\r\n");
                                }
                                else
                                {
                                    printf("Write test FAILED\r\n");
                                }
                            }
                        }
                    }
                }
                else if (strcmp(cmd, "recvfile") == 0)
                {
                    uint8_t size_bytes[4];
                    uint32_t file_size = 0;
                    uint32_t received = 0;
                    uint8_t buffer[128];

                    printf("Send file size as 4 bytes...\r\n");

                    if (HAL_UART_Receive(&huart2, size_bytes, 4, HAL_MAX_DELAY) != HAL_OK)
                    {
                        printf("Failed to receive file size\r\n");
                    }
                    else
                    {
                        file_size = read_u32_le(size_bytes);

                        printf("Received file size: %lu bytes\r\n", file_size);

                        if (file_size == 0 || file_size > (128U * 1024U))
                        {
                            printf("Invalid file size\r\n");
                        }
                        else
                        {
                            printf("Receiving file...\r\n");

                            while (received < file_size)
                            {
                                uint32_t remaining = file_size - received;
                                uint32_t chunk_size = remaining;

                                if (chunk_size > sizeof(buffer))
                                {
                                    chunk_size = sizeof(buffer);
                                }

                                if (HAL_UART_Receive(&huart2, buffer, chunk_size, HAL_MAX_DELAY) != HAL_OK)
                                {
                                    printf("Receive failed\r\n");
                                    break;
                                }

                                received += chunk_size;

                                if ((received % 1024U) == 0 || received == file_size)
                                {
                                    printf("Received %lu / %lu bytes\r\n", received, file_size);
                                }
                            }

                            if (received == file_size)
                            {
                                printf("File receive test PASSED\r\n");
                            }
                            else
                            {
                                printf("File receive test FAILED\r\n");
                            }
                        }
                    }
                }
                else if (strcmp(cmd, "update") == 0)
                {
                    handle_update_command(0, 0);
                }
                else if (strcmp(cmd, "updateb") == 0)
                {
                    /*
                     * Legacy command.
                     * Still supported, but now it also expects CRC32.
                     */
                    handle_update_command(1, BOOT_SLOT_B);
                }
                else if (strcmp(cmd, "jump") == 0)
                {
                    printf("Jumping to app...\r\n");
                    HAL_Delay(200);

                    jump_to_app();

                    printf("Jump failed\r\n");
                }
                else if (index > 0)
                {
                    printf("Unknown command: %s\r\n", cmd);
                }

                index = 0;
                memset(cmd, 0, sizeof(cmd));
                printf("> ");
            }
            else if (rx == '\b' || rx == 127)
            {
                if (index > 0)
                {
                    index--;
                    cmd[index] = '\0';
                    printf(" \b");
                }
            }
            else
            {
                if (index < sizeof(cmd) - 1)
                {
                    cmd[index++] = rx;
                }
                else
                {
                    index = 0;
                    memset(cmd, 0, sizeof(cmd));
                    printf("\r\nCommand too long\r\n> ");
                }
            }
        }
    }
}

static void boot_selected_slot(void)
{
    boot_metadata_t metadata;

    Metadata_Read(&metadata);

    if (!Metadata_IsValid(&metadata))
    {
        printf("Metadata invalid, booting Slot A by default\r\n");
        HAL_Delay(300);
        jump_to_address(SLOT_A_ADDRESS);

        printf("Slot A jump failed\r\n");
        return;
    }

    printf("Metadata valid\r\n");
    printf("Active slot: %s\r\n", Metadata_SlotName(metadata.active_slot));
    printf("Slot A state: %s\r\n", Metadata_StateName(metadata.slot_a_state));
    printf("Slot B state: %s\r\n", Metadata_StateName(metadata.slot_b_state));
    printf("Boot attempts: %lu\r\n", metadata.boot_attempts);

    if (metadata.active_slot == BOOT_SLOT_A)
    {
        if (metadata.slot_a_state == SLOT_STATE_PENDING_TEST)
        {
            if (metadata.boot_attempts == 0)
            {
                printf("Slot A is PENDING_TEST. First boot attempt.\r\n");
                printf("Setting boot_attempts = 1\r\n");

                Metadata_SetBootAttempts(1);

                HAL_Delay(300);
                jump_to_address(SLOT_A_ADDRESS);

                printf("Slot A jump failed\r\n");
            }
            else
            {
                printf("Slot A pending test failed. Rolling back...\r\n");

                if (Metadata_RollbackFromPending() == HAL_OK)
                {
                    printf("Rollback metadata update OK\r\n");
                }
                else
                {
                    printf("Rollback metadata update FAILED\r\n");
                }

                Metadata_Read(&metadata);
                print_metadata();

                if (metadata.active_slot == BOOT_SLOT_B &&
                    metadata.slot_b_state == SLOT_STATE_VALID)
                {
                    printf("Booting rollback Slot B\r\n");
                    HAL_Delay(300);
                    jump_to_address(SLOT_B_ADDRESS);

                    printf("Slot B jump failed\r\n");
                }
            }
        }
        else if (metadata.slot_a_state == SLOT_STATE_VALID)
        {
            printf("Booting valid Slot A\r\n");
            HAL_Delay(300);
            jump_to_address(SLOT_A_ADDRESS);

            printf("Slot A jump failed\r\n");
        }
        else
        {
            printf("Slot A is not bootable. Trying Slot B...\r\n");

            if (metadata.slot_b_state == SLOT_STATE_VALID)
            {
                HAL_Delay(300);
                jump_to_address(SLOT_B_ADDRESS);

                printf("Slot B jump failed\r\n");
            }
        }
    }
    else if (metadata.active_slot == BOOT_SLOT_B)
    {
        if (metadata.slot_b_state == SLOT_STATE_PENDING_TEST)
        {
            if (metadata.boot_attempts == 0)
            {
                printf("Slot B is PENDING_TEST. First boot attempt.\r\n");
                printf("Setting boot_attempts = 1\r\n");

                Metadata_SetBootAttempts(1);

                HAL_Delay(300);
                jump_to_address(SLOT_B_ADDRESS);

                printf("Slot B jump failed\r\n");
            }
            else
            {
                printf("Slot B pending test failed. Rolling back...\r\n");

                if (Metadata_RollbackFromPending() == HAL_OK)
                {
                    printf("Rollback metadata update OK\r\n");
                }
                else
                {
                    printf("Rollback metadata update FAILED\r\n");
                }

                Metadata_Read(&metadata);
                print_metadata();

                if (metadata.active_slot == BOOT_SLOT_A &&
                    metadata.slot_a_state == SLOT_STATE_VALID)
                {
                    printf("Booting rollback Slot A\r\n");
                    HAL_Delay(300);
                    jump_to_address(SLOT_A_ADDRESS);

                    printf("Slot A jump failed\r\n");
                }
            }
        }
        else if (metadata.slot_b_state == SLOT_STATE_VALID)
        {
            printf("Booting valid Slot B\r\n");
            HAL_Delay(300);
            jump_to_address(SLOT_B_ADDRESS);

            printf("Slot B jump failed\r\n");
        }
        else
        {
            printf("Slot B is not bootable. Trying Slot A...\r\n");

            if (metadata.slot_a_state == SLOT_STATE_VALID)
            {
                HAL_Delay(300);
                jump_to_address(SLOT_A_ADDRESS);

                printf("Slot A jump failed\r\n");
            }
        }
    }
    else
    {
        printf("Invalid active slot. Booting Slot A by default.\r\n");
        HAL_Delay(300);
        jump_to_address(SLOT_A_ADDRESS);

        printf("Slot A jump failed\r\n");
    }
}

/* USER CODE END 0 */

int main(void)
{
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */

  HAL_Delay(500);

  printf("\r\nBOOTLOADER START\r\n");

  if (is_user_button_pressed())
  {
      printf("User button pressed during reset\r\n");
      printf("Entering UART bootloader mode\r\n");
      bootloader_uart_command_loop();
  }
  else
  {
      printf("No button press detected\r\n");
      printf("Booting selected slot from metadata\r\n");

      HAL_Delay(500);

      boot_selected_slot();

      printf("BOOT SELECT FAILED\r\n");
  }

  /* USER CODE END 2 */

  while (1)
  {
      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
      printf("NO VALID APP FOUND\r\n");
      HAL_Delay(1000);
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();

  while (1)
  {
      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
      HAL_Delay(100);
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
