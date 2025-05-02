#ifdef USE_STM32
#include "core.h"
#include "esphome/core/log.h"

const char *TAG = "main";

// extern IWDG_HandleTypeDef hiwdg;

extern void setup();
extern void loop();

namespace esphome {
namespace stm32 {}  // namespace stm32
}  // namespace esphome

int main() {
  HAL_Init();
#if (__CORTEX_M >= 0x03)
  // Enable the DWT Cycle Counter for micros()/delayMicroseconds()
  if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // Enable TRCENA
  }
  DWT->CYCCNT = 0;  // Reset cycle counter
  if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;  // Enable cycle counter
  }
#endif

  // Good place for one-time setup required by the functions in this file.

  // Initialize Watchdog here IF NOT done in main() by CubeMX's MX_IWDG_Init()
  // Example manual IWDG init (check specific parameters for your MCU):
  // #ifdef HAL_IWDG_MODULE_ENABLED
  //   hiwdg.Instance = IWDG;
  //   hiwdg.Init.Prescaler = IWDG_PRESCALER_32; // Example
  //   hiwdg.Init.Reload = 4095; // Example ~4 seconds with LSI=32kHz
  //   if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
  //     // Error_Handler(); // Handle error
  //   }
  // #endif

  setup();
  esphome::stm32::log_clock_config();
#if defined(FLASH_BANK_2)
  ESP_LOGI(TAG, "Active flash bank: %d", ::esphome::stm32::get_active_flash_bank());
#endif
  while (1) {
    loop();
  }
}

#endif  // USE_STM32