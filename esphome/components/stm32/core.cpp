#ifdef USE_STM32
#include "core.h"
#include "esphome/core/log.h"

static const char *TAG = "main";

namespace esphome {

namespace stm32 {
#if (__CORTEX_M >= 0x03)
static uint32_t prev_dwt_cycle_cnt = 0;
static uint64_t cycle_cnt = 0;

uint64_t get_dwt_cycle_cnt() {
  uint32_t dwt_cycle_cnt = DWT->CYCCNT;
  uint32_t delta = dwt_cycle_cnt - prev_dwt_cycle_cnt;
  cycle_cnt += delta;
  prev_dwt_cycle_cnt = dwt_cycle_cnt;
  return cycle_cnt;
}
#endif

uint8_t get_active_flash_bank() {
#if defined(FLASH_BANK_2) && defined(OB_BFB2_ENABLE)
  volatile uint32_t remap = READ_BIT(SYSCFG->MEMRMP, 0x1 << 8);
  return remap == 0 ? FLASH_BANK_1 : FLASH_BANK_2;
#elif defined(FLASH_BANK_2) && defined(OB_USER_BANK_SWAP)
#error GO - TODO!
  return 0;
#elif OB_SWAP_BANK_ENABLE
  FLASH_OBProgramInitTypeDef ob_config = {0};
  HAL_FLASHEx_OBGetConfig(&ob_config);
  bool swapped = (ob_config.USERConfig & OB_SWAP_BANK_ENABLE) > 0;
  return swapped ? FLASH_BANK_2 : FLASH_BANK_1;
#endif
}

void swap_flash_banks() {
#if defined(FLASH_BANK_2) && defined(OB_BFB2_ENABLE)
  FLASH_OBProgramInitTypeDef ob_config = {0};
  HAL_FLASHEx_OBGetConfig(&ob_config);

  if (get_active_flash_bank() == FLASH_BANK_1) {
    ob_config.USERConfig |= OB_BFB2_ENABLE;
  } else {
    ob_config.USERConfig &= ~OB_BFB2_ENABLE;
  }

  ob_config.OptionType = OPTIONBYTE_USER;
  ob_config.USERType = OB_USER_BFB2;

  HAL_FLASH_Unlock();
  HAL_FLASH_OB_Unlock();
  HAL_FLASHEx_OBProgram(&ob_config);
  HAL_FLASH_OB_Launch();
  HAL_FLASH_OB_Lock();
#elif defined(U5)
  FLASH_OBProgramInitTypeDef ob_config = {0};
  HAL_FLASHEx_OBGetConfig(&ob_config);

  if (get_active_flash_bank() == FLASH_BANK_1) {
    ob_config.USERConfig |= OB_SWAP_BANK_ENABLE;
  } else {
    ob_config.USERConfig &= ~OB_SWAP_BANK_ENABLE;
  }

  ob_config.OptionType = OPTIONBYTE_USER;
  ob_config.USERType = OB_USER_SWAP_BANK;

  HAL_FLASH_Unlock();
  HAL_FLASH_OB_Unlock();
  HAL_FLASHEx_OBProgram(&ob_config);
  HAL_FLASH_OB_Launch();
  HAL_FLASH_OB_Lock();
#elif defined(FLASH_BANK_2) && defined(OB_USER_BANK_SWAP)
#error G0 - TODO!
#endif
}

void log_clock_config() {
  uint32_t sysClockFreq = HAL_RCC_GetSysClockFreq();
  uint32_t hclkFreq = HAL_RCC_GetHCLKFreq();

  ESP_LOGI(TAG, "--- Clock Configuration ---");
  ESP_LOGI(TAG, "System Clock Frequency (SYSCLK): %lu Hz (max: %lu Hz)", sysClockFreq, F_CPU);
  ESP_LOGI(TAG, "HSE Frequency: %u Hz", HSE_VALUE);
  ESP_LOGI(TAG, "HCLK Frequency (AHB Bus): %lu Hz", hclkFreq);
#ifdef HAL_RCC_GetPCLK1Freq
  uint32_t pclk1Freq = HAL_RCC_GetPCLK1Freq();
  ESP_LOGI(TAG, "PCLK1 Frequency (APB1 Bus): %lu Hz", pclk1Freq);
#endif
#ifdef HAL_RCC_GetPCLK2Freq
  uint32_t pclk2Freq = HAL_RCC_GetPCLK2Freq();
  ESP_LOGI(TAG, "PCLK2 Frequency (APB2 Bus): %lu Hz", pclk2Freq);
#endif
}

void hal_init() {
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
}

void init_uart() {}

}  // namespace stm32

uint32_t IRAM_ATTR HOT millis() { return HAL_GetTick(); }

void IRAM_ATTR HOT delay(uint32_t ms) { HAL_Delay(ms); }

uint32_t IRAM_ATTR HOT micros() {
#if (__CORTEX_M >= 0x03)
  uint32_t hclk_freq = HAL_RCC_GetHCLKFreq();
  if (hclk_freq == 0)
    return 0;  // Clock not configured?
  return (uint32_t) ((stm32::get_dwt_cycle_cnt() * 1000000ULL) / hclk_freq);
#else
  return millis() * 1000;
#endif
}

void IRAM_ATTR HOT delayMicroseconds(uint32_t us) {
  if (us == 0)
    return;
#if (__CORTEX_M >= 0x03)
  // Ensure DWT cycle counter is enabled
  uint32_t hclk_freq = HAL_RCC_GetHCLKFreq();
  if (hclk_freq == 0)
    return;  // Clock not configured?

  uint64_t target_cycle_cnt = stm32::get_dwt_cycle_cnt() + (uint64_t) us * hclk_freq / 1000000ULL;

  while (target_cycle_cnt > stm32::get_dwt_cycle_cnt()) {
    __NOP();  // Ensure loop doesn't get optimized away
  }
#else
  delay(us / 1000);
#endif
}

void IRAM_ATTR HOT yield() {}

void arch_restart() {
  HAL_NVIC_SystemReset();
  // This function should not return. Loop forever if it does.
  while (1) {
  }
}

void arch_init() {}

void IRAM_ATTR HOT arch_feed_wdt() {}

uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }

uint32_t IRAM_ATTR HOT arch_get_cpu_cycle_count() {
#if (__CORTEX_M >= 0x03)
  return DWT->CYCCNT;
#else
  return 0;  // Not available or easily accessible on M0/M0+ without custom timer
#endif
}

uint32_t arch_get_cpu_freq_hz() { return HAL_RCC_GetHCLKFreq(); }

}  // namespace esphome

int main() {
  ::esphome::stm32::hal_init();

  setup();

  esphome::stm32::log_clock_config();
#if defined(FLASH_BANK_2)
  ESP_LOGI(TAG, "Active flash bank: %d", ::esphome::stm32::get_active_flash_bank());
#endif

  while (1) {
    loop();
  }
}

extern "C" {
void SysTick_Handler(void) { HAL_IncTick(); }

void Error_Handler(void) {
  while (1) {
  }
}
}

#endif  // USE_STM32
