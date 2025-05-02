#ifdef USE_STM32
#include "core.h"
#include "esphome/core/log.h"

const char *TAG = "main";

// extern IWDG_HandleTypeDef hiwdg;

extern void setup();
extern void loop();

namespace esphome {
namespace stm32 {

// Helper functions to get prescaler values
uint32_t get_ahb_prescaler(uint32_t divider) {
  switch (divider) {
    case RCC_SYSCLK_DIV1:
      return 1;
    case RCC_SYSCLK_DIV2:
      return 2;
    case RCC_SYSCLK_DIV4:
      return 4;
    case RCC_SYSCLK_DIV8:
      return 8;
    case RCC_SYSCLK_DIV16:
      return 16;
    case RCC_SYSCLK_DIV64:
      return 64;
    case RCC_SYSCLK_DIV128:
      return 128;
    case RCC_SYSCLK_DIV256:
      return 256;
    case RCC_SYSCLK_DIV512:
      return 512;
    default:
      return 1;
  }
}

uint32_t get_apb_prescaler(uint32_t divider) {
  switch (divider) {
    case RCC_HCLK_DIV1:
      return 1;
    case RCC_HCLK_DIV2:
      return 2;
    case RCC_HCLK_DIV4:
      return 4;
    case RCC_HCLK_DIV8:
      return 8;
    case RCC_HCLK_DIV16:
      return 16;
    default:
      return 1;
  }
}

void log_clock_config() {
  RCC_ClkInitTypeDef clkinitstruct = {0};
  RCC_OscInitTypeDef oscinitstruct = {0};

  uint32_t uwPLLSource = 0;
  uint32_t sysClockFreq = HAL_RCC_GetSysClockFreq();
  uint32_t hclkFreq = HAL_RCC_GetHCLKFreq();
  uint32_t pclk1Freq = HAL_RCC_GetPCLK1Freq();
  uint32_t pclk2Freq = HAL_RCC_GetPCLK2Freq();

  HAL_RCC_GetClockConfig(&clkinitstruct, &uwPLLSource);
  HAL_RCC_GetOscConfig(&oscinitstruct);

#if defined(RCC_PLLSOURCE_NONE)
  const char *pllSourceStr;
  switch (uwPLLSource) {
    case RCC_PLLSOURCE_NONE:
      pllSourceStr = "None";
      break;
    case RCC_PLLSOURCE_HSI:
      pllSourceStr = "HSI";
      break;
    case RCC_PLLSOURCE_HSE:
      pllSourceStr = "HSE";
      break;
    default:
      pllSourceStr = "Unknown";
      break;
  }
#endif

  const char *oscillatorTypeStr;
  switch (oscinitstruct.OscillatorType) {
    case RCC_OSCILLATORTYPE_HSE:
      oscillatorTypeStr = "HSE";
      break;
    case RCC_OSCILLATORTYPE_HSI:
      oscillatorTypeStr = "HSI";
      break;
    case RCC_OSCILLATORTYPE_LSE:
      oscillatorTypeStr = "LSE";
      break;
    case RCC_OSCILLATORTYPE_LSI:
      oscillatorTypeStr = "LSI";
      break;
    default:
      oscillatorTypeStr = "NONE";
  }

  ESP_LOGI(TAG, "--- Clock Configuration ---");
  ESP_LOGI(TAG, "System Clock Frequency (SYSCLK): %lu Hz (max: %lu Hz)", sysClockFreq, F_CPU);
  ESP_LOGI(TAG, "HCLK Frequency (AHB Bus): %lu Hz", hclkFreq);
  ESP_LOGI(TAG, "PCLK1 Frequency (APB1 Bus): %lu Hz", pclk1Freq);
  ESP_LOGI(TAG, "PCLK2 Frequency (APB2 Bus): %lu Hz", pclk2Freq);
  ESP_LOGD(TAG, "--- Oscillators ---");
  ESP_LOGD(TAG, "Oscilator Type: %s", oscillatorTypeStr);
  ESP_LOGD(TAG, "HSI State: %s, Calibration Value: %lu", (oscinitstruct.HSIState == RCC_HSI_ON) ? "ON" : "OFF",
           oscinitstruct.HSICalibrationValue);
  ESP_LOGD(TAG, "HSE State: %s",
           (oscinitstruct.HSEState == RCC_HSE_ON)       ? "ON"
           : (oscinitstruct.HSEState == RCC_HSE_BYPASS) ? "BYPASS"
                                                        : "OFF");
  ESP_LOGD(TAG, "LSE State: %s",
           (oscinitstruct.LSEState == RCC_LSE_ON)       ? "ON"
           : (oscinitstruct.LSEState == RCC_LSE_BYPASS) ? "BYPASS"
                                                        : "OFF");
  ESP_LOGD(TAG, "LSI State: %s", (oscinitstruct.LSIState == RCC_LSI_ON) ? "ON" : "OFF");
#if defined(RCC_PLLSOURCE_NONE)
  ESP_LOGD(TAG, "PLL Source: %s", pllSourceStr);
  if (oscinitstruct.PLL.PLLState == RCC_PLL_ON) {
    ESP_LOGD(TAG, "--- PLL Configuration ---");
    ESP_LOGD(TAG, "PLL Multiplier (N): %lu", oscinitstruct.PLL.PLLN);
    ESP_LOGD(TAG, "PLL Divider (M): %lu", oscinitstruct.PLL.PLLM);
    ESP_LOGD(TAG, "PLL P Divider (P): %lu", oscinitstruct.PLL.PLLP);
    ESP_LOGD(TAG, "PLL Q Divider (Q): %lu", oscinitstruct.PLL.PLLQ);
    ESP_LOGD(TAG, "PLL R Divider (R): %lu", oscinitstruct.PLL.PLLR);
  }
#endif

  uint32_t ahb_div = get_ahb_prescaler(clkinitstruct.AHBCLKDivider);
  uint32_t apb1_div = get_apb_prescaler(clkinitstruct.APB1CLKDivider);
  uint32_t apb2_div = get_apb_prescaler(clkinitstruct.APB2CLKDivider);

  ESP_LOGD(TAG, "AHB Prescaler: %lu", ahb_div);
  ESP_LOGD(TAG, "APB1 Prescaler: %lu", apb1_div);
  ESP_LOGD(TAG, "APB2 Prescaler: %lu", apb2_div);
}
// #else
// void log_clock_config() {}
// #endif

}  // namespace stm32
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

  ::esphome::stm32::log_clock_config();
#if defined(FLASH_BANK_2)
  ESP_LOGI(TAG, "Active flash bank: %d", ::esphome::stm32::get_active_flash_bank());
#endif
  while (1) {
    loop();
  }
}

#endif  // USE_STM32