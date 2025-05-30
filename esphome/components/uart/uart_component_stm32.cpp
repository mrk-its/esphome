#ifdef USE_STM32

#include "uart_component_stm32.h"
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/stm32/gpio.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {
namespace uart {
static const char *const TAG = "uart.stm32";

void STM32UARTComponent::setup() {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if (tx_pin_ != nullptr) {
    tx_pin_->setup();
    auto pin = tx_pin_->get_pin();
    auto port = esphome::stm32::pin_to_port(pin);
    GPIO_InitStruct.Pin = esphome::stm32::pin_to_mask(pin);
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    auto af = ((stm32::STM32GPIOPin *) tx_pin_)->get_af();
    if (af) {
      GPIO_InitStruct.Alternate = *af;
    }
    HAL_GPIO_Init(port, &GPIO_InitStruct);
  }
  if (rx_pin_ != nullptr) {
    rx_pin_->setup();
    auto pin = rx_pin_->get_pin();
    auto port = esphome::stm32::pin_to_port(pin);
    GPIO_InitStruct.Pin = esphome::stm32::pin_to_mask(pin);
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    auto af = ((stm32::STM32GPIOPin *) rx_pin_)->get_af();
    if (af) {
      GPIO_InitStruct.Alternate = *af;
    }
    HAL_GPIO_Init(port, &GPIO_InitStruct);
  }
  if (clock_initializer_) {
    clock_initializer_();
  }

  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_SYSCLK;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

  uart_handle_.Init.BaudRate = baud_rate_;
  uart_handle_.Init.WordLength = UART_WORDLENGTH_8B;
  uart_handle_.Init.StopBits = UART_STOPBITS_1;
  uart_handle_.Init.Parity = UART_PARITY_NONE;
  uart_handle_.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  uart_handle_.Init.Mode = UART_MODE_TX_RX;
  uart_handle_.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&uart_handle_) != HAL_OK) {
    Error_Handler();
  }
}

void STM32UARTComponent::dump_config() {
  // ESP_LOGCONFIG(TAG, "UART Bus %u:", this->uart_num_);
  LOG_PIN("  TX Pin: ", tx_pin_);
  LOG_PIN("  RX Pin: ", rx_pin_);
  ESP_LOGCONFIG(TAG, "  Baud Rate: %" PRIu32 " baud", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Data Bits: %u", this->data_bits_);
  ESP_LOGCONFIG(TAG, "  Parity: %s", LOG_STR_ARG(parity_to_str(this->parity_)));
  ESP_LOGCONFIG(TAG, "  Stop bits: %u", this->stop_bits_);
  this->check_logger_conflict();
}

void STM32UARTComponent::write_array(const uint8_t *data, size_t len) {
  HAL_UART_Transmit(&uart_handle_, data, len, 1000);
}

bool STM32UARTComponent::peek_byte(uint8_t *data) { return false; }

bool STM32UARTComponent::read_array(uint8_t *data, size_t len) { return false; }

int STM32UARTComponent::available() { return 0; }

void STM32UARTComponent::flush() {}

void STM32UARTComponent::check_logger_conflict() {}

}  // namespace uart
}  // namespace esphome

#endif  // USE_STM32