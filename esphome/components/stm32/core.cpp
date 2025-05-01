#ifdef USE_STM32
#include "core.h"
#include "esphome/core/log.h"

const char *TAG = "main";

// extern IWDG_HandleTypeDef hiwdg;

extern void setup();
extern void loop();

void init_uart();
void uart_write_str(const char *str);

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
  ESP_LOGI(TAG, "--- Oscillators ---");
  ESP_LOGI(TAG, "Oscilator Type: %s", oscillatorTypeStr);
  ESP_LOGI(TAG, "HSI State: %s, Calibration Value: %lu", (oscinitstruct.HSIState == RCC_HSI_ON) ? "ON" : "OFF",
           oscinitstruct.HSICalibrationValue);
  ESP_LOGI(TAG, "HSE State: %s",
           (oscinitstruct.HSEState == RCC_HSE_ON)       ? "ON"
           : (oscinitstruct.HSEState == RCC_HSE_BYPASS) ? "BYPASS"
                                                        : "OFF");
  ESP_LOGI(TAG, "LSE State: %s",
           (oscinitstruct.LSEState == RCC_LSE_ON)       ? "ON"
           : (oscinitstruct.LSEState == RCC_LSE_BYPASS) ? "BYPASS"
                                                        : "OFF");
  ESP_LOGI(TAG, "LSI State: %s", (oscinitstruct.LSIState == RCC_LSI_ON) ? "ON" : "OFF");
  ESP_LOGI(TAG, "PLL Source: %s", pllSourceStr);
  if (oscinitstruct.PLL.PLLState == RCC_PLL_ON) {
    ESP_LOGI(TAG, "--- PLL Configuration ---");
    ESP_LOGI(TAG, "PLL Multiplier (N): %lu", oscinitstruct.PLL.PLLN);
    ESP_LOGI(TAG, "PLL Divider (M): %lu", oscinitstruct.PLL.PLLM);
    ESP_LOGI(TAG, "PLL P Divider (P): %lu", oscinitstruct.PLL.PLLP);
    ESP_LOGI(TAG, "PLL Q Divider (Q): %lu", oscinitstruct.PLL.PLLQ);
    ESP_LOGI(TAG, "PLL R Divider (R): %lu", oscinitstruct.PLL.PLLR);
  }

  uint32_t ahb_div = get_ahb_prescaler(clkinitstruct.AHBCLKDivider);
  uint32_t apb1_div = get_apb_prescaler(clkinitstruct.APB1CLKDivider);
  uint32_t apb2_div = get_apb_prescaler(clkinitstruct.APB2CLKDivider);

  ESP_LOGI(TAG, "AHB Prescaler: %lu", ahb_div);
  ESP_LOGI(TAG, "APB1 Prescaler: %lu", apb1_div);
  ESP_LOGI(TAG, "APB2 Prescaler: %lu", apb2_div);
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
    Error_Handler();
  }
}

int main() {
  HAL_Init();
  // SystemClock_Config();
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

  log_clock_config();

  ESP_LOGI(TAG, "Active flash bank: %d", ::esphome::stm32::get_active_flash_bank());

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

void uart_write_str(const char *str) { HAL_UART_Transmit(&UartHandle, (uint8_t *) str, strlen(str), 1000); }

#endif  // USE_STM32