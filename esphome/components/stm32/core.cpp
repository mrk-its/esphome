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
#elif H7
#include "stm32f7xx_hal.h"
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

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

void setup();
void loop();

int main() {
   HAL_Init();
    __HAL_RCC_GPIOA_CLK_ENABLE();
  setup();
  while(1) {
    loop();
  }
}

namespace esphome {

void IRAM_ATTR HOT yield() { }
uint32_t IRAM_ATTR HOT millis() { return 0;}
void IRAM_ATTR HOT delay(uint32_t ms) { }
uint32_t IRAM_ATTR HOT micros() { return 0;}
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) {  }
void arch_restart() {while(1); }
void arch_init() {}
void IRAM_ATTR HOT arch_feed_wdt() { }

uint8_t progmem_read_byte(const uint8_t *addr) {
  return 0;
}
uint32_t IRAM_ATTR HOT arch_get_cpu_cycle_count() { return 0; }
uint32_t arch_get_cpu_freq_hz() { return 0; }

}


#endif  // USE_STM32
