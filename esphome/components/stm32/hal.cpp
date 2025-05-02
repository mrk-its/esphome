#ifdef USE_STM32
#include "core.h"

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
  volatile uint32_t remap = READ_BIT(SYSCFG->MEMRMP, 0x1 << 8);
  return remap == 0 ? FLASH_BANK_1 : FLASH_BANK_1;
}

void swap_flash_banks() {
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
}
static UART_HandleTypeDef UartHandle;

void init_uart() {
  GPIO_InitTypeDef GPIO_InitStruct;

  USARTx_TX_GPIO_CLK_ENABLE();
  USARTx_RX_GPIO_CLK_ENABLE();

  USARTx_CLK_ENABLE();

  GPIO_InitStruct.Pin = USARTx_TX_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
  GPIO_InitStruct.Alternate = USARTx_TX_AF;

  HAL_GPIO_Init(USARTx_TX_GPIO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = USARTx_RX_PIN;
  GPIO_InitStruct.Alternate = USARTx_RX_AF;

  HAL_GPIO_Init(USARTx_RX_GPIO_PORT, &GPIO_InitStruct);
  UartHandle.Instance = USARTx;

  UartHandle.Init.BaudRate = 115200;
  UartHandle.Init.WordLength = UART_WORDLENGTH_8B;
  UartHandle.Init.StopBits = UART_STOPBITS_1;
  UartHandle.Init.Parity = UART_PARITY_NONE;
  UartHandle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  UartHandle.Init.Mode = UART_MODE_TX_RX;
  UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&UartHandle) != HAL_OK) {
    for (;;)
      ;
  }
}

void uart_write_char(char c) { HAL_UART_Transmit(&UartHandle, (uint8_t *) (&c), 1, 1000); }

void uart_write_str(const char *str) { HAL_UART_Transmit(&UartHandle, (uint8_t *) str, strlen(str), 1000); }

}  // namespace stm32

uint32_t random_uint32() { return 42; }

uint32_t IRAM_ATTR HOT millis() { return HAL_GetTick(); }

void IRAM_ATTR HOT delay(uint32_t ms) { HAL_Delay(ms); }

uint32_t IRAM_ATTR HOT micros() {
#if (__CORTEX_M >= 0x03)
  uint32_t hclk_freq = HAL_RCC_GetHCLKFreq();
  if (hclk_freq == 0)
    return 0;  // Clock not configured?
  return (uint32_t) ((stm32::get_dwt_cycle_cnt() * 1000000ULL) / hclk_freq);

#else
  // Fallback for Cortex-M0/M0+: Use SysTick (lower resolution)
  // Get current SysTick counter value and snapshot of millis()
  uint32_t ms = HAL_GetTick();
  uint32_t tick_val = SysTick->VAL;
  uint32_t tick_load = SysTick->LOAD;
  uint32_t ms_check = HAL_GetTick();
  // Check if SysTick rolled over while reading values
  // Or if the COUNTFLAG is set (indicating a rollover occurred since last check)
  if (ms != ms_check || (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)) {
    ms = HAL_GetTick();       // Read updated millis() again
    tick_val = SysTick->VAL;  // Re-read VAL after potential rollover
  }

  // Calculate microseconds: ms part + fraction of the current ms based on SysTick counter
  // SysTick counts down from LOAD to 0. Fraction is (LOAD - VAL) / (LOAD + 1)
  return (ms * 1000) + ((tick_load - tick_val) * 1000) / (tick_load + 1);
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
  // Fallback for Cortex-M0/M0+: Calibrated busy-wait loop (less accurate)
  uint32_t hclk_mhz = HAL_RCC_GetHCLKFreq() / 1000000;
  // Rough calibration: Number of loop iterations per microsecond.
  // This needs careful tuning for your specific M0/M0+ chip and optimization level!
  // Example factor (adjust!): Divide by 4 seems common, but testing is needed.
  uint32_t loop_iterations = us * (hclk_mhz / 4);
  while (loop_iterations--) {
    __NOP();
  }
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

void IRAM_ATTR HOT arch_feed_wdt() {
#ifdef HAL_IWDG_MODULE_ENABLED_
  // Requires the watchdog to be initialized (either by CubeMX or manually in arch_init/main)
  // and the handle `hiwdg` to be globally accessible (use 'extern' declaration above).
  HAL_IWDG_Refresh(&hiwdg);
#endif
}

uint8_t progmem_read_byte(const uint8_t *addr) {
  // Add checks for valid Flash address range if necessary
  return *addr;
}

uint32_t IRAM_ATTR HOT arch_get_cpu_cycle_count() {
#if (__CORTEX_M >= 0x03)
  return DWT->CYCCNT;
#else
  return 0;  // Not available or easily accessible on M0/M0+ without custom timer
#endif
}

uint32_t arch_get_cpu_freq_hz() { return HAL_RCC_GetHCLKFreq(); }

}  // namespace esphome

#endif  // USE_STM32