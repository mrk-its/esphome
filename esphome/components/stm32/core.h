#pragma once
#ifdef USE_STM32

#include "stm32_hal.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

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

namespace stm32 {
void hal_init();
void log_clock_config();
uint8_t get_active_flash_bank();
void swap_flash_banks();

}  // namespace stm32

};  // namespace esphome

extern void setup();
extern void loop();

extern "C" {
void SysTick_Handler(void);
void Error_Handler(void);
}

#endif
