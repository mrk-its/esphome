#ifdef USE_STM32
#include "core.h"

// extern IWDG_HandleTypeDef hiwdg;

extern void setup();
extern void loop();

void init_uart();
void uart_write_str(const char *str);

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
  init_uart();

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

  while (1) {
    loop();
  }
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

void uart_write_str(const char *str) {
  HAL_UART_Transmit(&UartHandle, (uint8_t *) str, strlen(str), 1000);
}

namespace esphome {

uint32_t random_uint32() { return 42; }

// --- Core Timing Implementation ---

/**
 * @brief Returns the number of milliseconds since the ESPHome application started.
 * Relies on the HAL SysTick timer configured by HAL_Init().
 */
uint32_t IRAM_ATTR HOT millis() { return HAL_GetTick(); }

/**
 * @brief Delays execution for a specified number of milliseconds.
 * Uses the blocking HAL_Delay function. Relies on SysTick interrupts.
 * @param ms Number of milliseconds to delay.
 */
void IRAM_ATTR HOT delay(uint32_t ms) {
  // Note: HAL_Delay uses HAL_GetTick which relies on SysTick interrupts.
  // Avoid calling this from ISRs or critical sections where SysTick might be disabled.
  HAL_Delay(ms);
}

uint32_t IRAM_ATTR HOT micros() {
#if (__CORTEX_M >= 0x03)
  // Ensure DWT cycle counter is enabled (call enable_dwt_cycle_counter() during init)
  uint32_t hclk_freq = HAL_RCC_GetHCLKFreq();
  if (hclk_freq == 0)
    return 0;  // Clock not configured?
  // Calculate microseconds from CPU cycles. Use 64-bit intermediate for accuracy.
  return (uint32_t) (((uint64_t) DWT->CYCCNT * 1000000ULL) / hclk_freq);
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

/**
 * @brief Delays execution for a specified number of microseconds.
 * Uses the DWT cycle counter and busy-waiting on Cortex-M3/M4/M7.
 * Uses a less accurate busy-wait loop for Cortex-M0/M0+.
 * @param us Number of microseconds to delay.
 */
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) {
  if (us == 0)
    return;
#if (__CORTEX_M >= 0x03)
  // Ensure DWT cycle counter is enabled
  uint32_t hclk_freq = HAL_RCC_GetHCLKFreq();
  if (hclk_freq == 0)
    return;  // Clock not configured?

  uint32_t start_cycle = DWT->CYCCNT;
  // Calculate target cycles for the delay. Use 64-bit intermediate.
  uint64_t target_cycles_64 = (uint64_t) us * hclk_freq / 1000000ULL;
  // Prevent excessive delays or overflow issues with the 32-bit counter delta
  uint32_t delay_cycles = (target_cycles_64 > 0xFFFFFFFFULL) ? 0xFFFFFFFF : (uint32_t) target_cycles_64;
  // Minimum delay of a few cycles to account for overhead
  if (delay_cycles < 20)
    delay_cycles = 20;

  uint32_t target_cycle = start_cycle + delay_cycles;
  // Busy wait loop - handles counter wrap-around correctly
  while (((int32_t) (target_cycle - DWT->CYCCNT)) > 0) {
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
    __asm volatile("nop");  // Use assembly NOP
  }
#endif
}
// --- Architecture Control Implementation ---

/**
 * @brief Allows background tasks or an RTOS scheduler to run.
 * In a bare-metal HAL scenario without specific background polling needed
 * by ESPHome components, this can often be empty.
 */
void IRAM_ATTR HOT yield() {
  // If using FreeRTOS: taskYIELD();
  // If specific polling needed (e.g., external network stack): poll_network_stack();
  // Otherwise, likely empty for basic HAL.
}

/**
 * @brief Resets the microcontroller.
 */
void arch_restart() {
  HAL_NVIC_SystemReset();
  // This function should not return. Loop forever if it does.
  while (1) {
  }
}

/**
 * @brief Performs early architecture-specific initialization.
 * Called by ESPHome before component setup.
 */
void arch_init() {}

/**
 * @brief Resets (feeds) the hardware watchdog timer.
 * Assumes IWDG is configured and the handle `hiwdg` is accessible.
 */
void IRAM_ATTR HOT arch_feed_wdt() {
#ifdef HAL_IWDG_MODULE_ENABLED_
  // Requires the watchdog to be initialized (either by CubeMX or manually in arch_init/main)
  // and the handle `hiwdg` to be globally accessible (use 'extern' declaration above).
  HAL_IWDG_Refresh(&hiwdg);
#endif
}

// --- Memory and CPU Info Implementation ---

/**
 * @brief Reads a byte from program memory (Flash).
 * On Cortex-M, Flash is usually memory-mapped, so direct dereference works.
 * @param addr Pointer to the byte in program memory.
 * @return The byte read from the address.
 */
uint8_t progmem_read_byte(const uint8_t *addr) {
  // Add checks for valid Flash address range if necessary
  return *addr;
}

/**
 * @brief Gets the raw CPU cycle count since reset or counter enable.
 * Requires DWT cycle counter (Cortex-M3+).
 */
uint32_t IRAM_ATTR HOT arch_get_cpu_cycle_count() {
#if (__CORTEX_M >= 0x03)
  return DWT->CYCCNT;
#else
  return 0;  // Not available or easily accessible on M0/M0+ without custom timer
#endif
}

/**
 * @brief Gets the configured CPU core clock frequency (HCLK) in Hz.
 * Relies on HAL/RCC being initialized correctly.
 */
uint32_t arch_get_cpu_freq_hz() { return HAL_RCC_GetHCLKFreq(); }

}  // namespace esphome

extern "C" {
/**
 * @brief This function handles System tick timer.
 */
void SysTick_Handler(void) { HAL_IncTick(); }

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
}

#endif  // USE_STM32
