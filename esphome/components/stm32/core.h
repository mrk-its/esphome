#pragma once

#ifdef USE_STM32

#if F0
#include "stm32f0xx_hal.h"
#elif F1
#include "stm32f1xx_hal.h"
#elif F2
#include "stm32f2xx_hal.h"
#elif F3
#include "stm32f3xx_hal.h"
#elif F4
#include "stm32f4xx_hal.h"
#elif F7
#include "stm32f7xx_hal.h"
#elif H7
#include "stm32h7xx_hal.h"
#elif G0
#include "stm32g0xx_hal.h"
#elif G4
#include "stm32g4xx_hal.h"
#elif H5
#include "stm32h5xx_hal.h"
#elif H7
#include "stm32h7xx_hal.h"
#elif L0
#include "stm32l0xx_hal.h"
#elif L1
#include "stm32l1xx_hal.h"
#elif L4
#include "stm32l4xx_hal.h"
#elif L5
#include "stm32l5xx_hal.h"
#else
#error "Unsupported STM32 Family"
#endif

#ifndef TEST_ESP_STM32
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#else
#define HOT
#define IRAM_ATTR
#endif

#define USARTx USART2
#define USARTx_CLK_ENABLE() __HAL_RCC_USART2_CLK_ENABLE()
#define USARTx_CLK_DISABLE() __HAL_RCC_USART2_CLK_DISABLE()
#define USARTx_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define USARTx_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define USARTx_RX_GPIO_CLK_DISABLE() __HAL_RCC_GPIOA_CLK_DISABLE()
#define USARTx_TX_GPIO_CLK_DISABLE() __HAL_RCC_GPIOA_CLK_DISABLE()

#define USARTx_FORCE_RESET() __HAL_RCC_USART2_FORCE_RESET()
#define USARTx_RELEASE_RESET() __HAL_RCC_USART2_RELEASE_RESET()

#define USARTx_TX_PIN GPIO_PIN_2
#define USARTx_TX_GPIO_PORT GPIOA
#define USARTx_TX_AF GPIO_AF7_USART2
#define USARTx_RX_PIN GPIO_PIN_3
#define USARTx_RX_GPIO_PORT GPIOA
#define USARTx_RX_AF GPIO_AF7_USART2

namespace esphome {
uint32_t IRAM_ATTR HOT millis();
void IRAM_ATTR HOT delay(uint32_t ms);
uint32_t IRAM_ATTR HOT micros();
void IRAM_ATTR HOT delayMicroseconds(uint32_t us);
void IRAM_ATTR HOT yield();
void arch_restart();
void arch_init();
void IRAM_ATTR HOT arch_feed_wdt();
uint8_t progmem_read_byte(const uint8_t *addr);
};  // namespace esphome

extern "C" {
void SysTick_Handler(void);
void Error_Handler(void);
}

#endif