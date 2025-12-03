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
  if (this->tx_pin_) {
    this->tx_pin_->setup();
  }
  if (this->rx_pin_) {
    this->rx_pin_->setup();
  }

  this->uart_handle_.Init.BaudRate = baud_rate_;
  switch (data_bits_) {
#ifdef UART_WORDLENGTH_7B
    case 7:
      this->uart_handle_.Init.WordLength = UART_WORDLENGTH_7B;
      break;
#endif
#ifdef UART_WORDLENGTH_9B
    case 9:
      this->uart_handle_.Init.WordLength = UART_WORDLENGTH_9B;
      break;
#endif
    default:
      this->uart_handle_.Init.WordLength = UART_WORDLENGTH_8B;
      break;
  }
  switch (stop_bits_) {
#ifdef UART_STOPBITS_0_5
    case 5:
      this->uart_handle_.Init.StopBits = UART_STOPBITS_0_5;
      break;
#endif
#ifdef UART_STOPBITS_1_5
    case 15:
      this->uart_handle_.Init.StopBits = UART_STOPBITS_1_5;
      break;
#endif
    case 2:
      this->uart_handle_.Init.StopBits = UART_STOPBITS_2;
    default:
      this->uart_handle_.Init.StopBits = UART_STOPBITS_1;
      break;
  }
  switch (parity_) {
    case UARTParityOptions::UART_CONFIG_PARITY_EVEN:
      this->uart_handle_.Init.Parity = UART_PARITY_EVEN;
      break;
    case UARTParityOptions::UART_CONFIG_PARITY_ODD:
      this->uart_handle_.Init.Parity = UART_PARITY_ODD;
      break;
    default:
      this->uart_handle_.Init.Parity = UART_PARITY_NONE;
      break;
  }
  this->uart_handle_.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  this->uart_handle_.Init.Mode = UART_MODE_TX_RX;
  this->uart_handle_.Init.OverSampling = UART_OVERSAMPLING_16;

#ifdef STM32U5

  this->uart_handle_.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  this->uart_handle_.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  this->uart_handle_.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
#endif

  if (HAL_UART_Init(&this->uart_handle_) != HAL_OK) {
    Error_Handler();
  }

#ifdef STM32U5
  if (HAL_UARTEx_SetRxFifoThreshold(&this->uart_handle_, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_EnableFifoMode(&this->uart_handle_) != HAL_OK) {
    Error_Handler();
  }
#endif

  if (!this->rx_buffer_size_) {
    // no rx_buffer_size_, skip DMA setup
    return;
  }

  this->rx_buffer_ = new uint8_t[this->rx_buffer_size_];

#ifdef STM32_UART_DMA
#if defined(STM32F1) || defined(STM32L4)
  __HAL_RCC_DMA1_CLK_ENABLE();
  this->dma_handle_.Init.Direction = DMA_PERIPH_TO_MEMORY;
  this->dma_handle_.Init.PeriphInc = DMA_PINC_DISABLE;
  this->dma_handle_.Init.MemInc = DMA_MINC_ENABLE;
  this->dma_handle_.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  this->dma_handle_.Init.MemDataAlignment = DMA_PDATAALIGN_BYTE;
  this->dma_handle_.Init.Mode = DMA_CIRCULAR;
  this->dma_handle_.Init.Priority = DMA_PRIORITY_HIGH;
  if (HAL_DMA_Init(&this->dma_handle_) != HAL_OK) {
    Error_Handler();
  }

  __HAL_LINKDMA(&this->uart_handle_, hdmarx, this->dma_handle_);

  if (HAL_UART_Receive_DMA(&this->uart_handle_, this->rx_buffer_, this->rx_buffer_size_) != HAL_OK) {
    Error_Handler();
  };
#elif defined(STM32U5)
  __HAL_RCC_GPDMA1_CLK_ENABLE();

  DMA_NodeConfTypeDef node_config;

  node_config.NodeType = DMA_GPDMA_LINEAR_NODE;
  node_config.Init.Request = this->dma_request_;
  node_config.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  node_config.Init.Direction = DMA_PERIPH_TO_MEMORY;
  node_config.Init.SrcInc = DMA_SINC_FIXED;
  node_config.Init.DestInc = DMA_DINC_INCREMENTED;
  node_config.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
  node_config.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
  node_config.Init.SrcBurstLength = 1;
  node_config.Init.DestBurstLength = 1;
  node_config.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
  node_config.Init.TransferEventMode = DMA_TCEM_REPEATED_BLOCK_TRANSFER;
  node_config.Init.Mode = DMA_NORMAL;
  node_config.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
  node_config.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
  node_config.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;

  if (HAL_DMAEx_List_BuildNode(&node_config, &this->dma_node_) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_DMAEx_List_InsertNode(&this->dma_list_, NULL, &this->dma_node_) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_DMAEx_List_SetCircularMode(&this->dma_list_) != HAL_OK) {
    Error_Handler();
  }

  this->dma_handle_.InitLinkedList.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
  this->dma_handle_.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
  this->dma_handle_.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
  this->dma_handle_.InitLinkedList.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  this->dma_handle_.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
  if (HAL_DMAEx_List_Init(&this->dma_handle_) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_DMAEx_List_LinkQ(&this->dma_handle_, &this->dma_list_) != HAL_OK) {
    Error_Handler();
  }

  __HAL_LINKDMA(&this->uart_handle_, hdmarx, this->dma_handle_);

  if (HAL_DMA_ConfigChannelAttributes(&this->dma_handle_, DMA_CHANNEL_NPRIV) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_UART_Receive_DMA(&this->uart_handle_, this->rx_buffer_, this->rx_buffer_size_) != HAL_OK) {
    Error_Handler();
  };
#else
#warning UART RX not supported yet on this stm32 family
#endif
#endif
}

void STM32UARTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Instance: %s", this->name_.c_str());
  LOG_PIN("  TX Pin: ", this->tx_pin_);
  LOG_PIN("  RX Pin: ", this->rx_pin_);
  ESP_LOGCONFIG(TAG, "  Baud Rate: %" PRIu32 " baud", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Data Bits: %u", this->data_bits_);
  ESP_LOGCONFIG(TAG, "  Parity: %s", LOG_STR_ARG(parity_to_str(this->parity_)));
  ESP_LOGCONFIG(TAG, "  Stop bits: %u", this->stop_bits_);
}

void STM32UARTComponent::write_array(const uint8_t *data, size_t len) {
  HAL_UART_Transmit(&this->uart_handle_, data, len, 1000);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_TX, data[i]);
  }
#endif
}

size_t STM32UARTComponent::get_tail_offset_() {
  if (!this->rx_buffer_size_) {
    // no rx_buffer, DMA is disabled, so always return 0
    return 0;
  }
  return this->rx_buffer_size_ - __HAL_DMA_GET_COUNTER(this->uart_handle_.hdmarx);
}

bool STM32UARTComponent::peek_byte(uint8_t *data) {
  size_t offs = this->get_tail_offset_();
  if (offs != this->prev_rx_offset_) {
    *data = this->rx_buffer_[this->prev_rx_offset_];
    return true;
  }
  return false;
}

bool STM32UARTComponent::read_array(uint8_t *data, size_t len) {
  uint8_t *dest_ptr = data;
  size_t remaining = len;

  if (!this->rx_buffer_size_) {
    // no rx_buffer, DMA is disabled - so fail
    return false;
  }

  while (remaining > 0) {
    size_t end_offset = std::min(this->prev_rx_offset_ + this->available(), this->rx_buffer_size_);
    size_t available = std::min(end_offset - this->prev_rx_offset_, remaining);
    if (available) {
      memcpy(dest_ptr, this->rx_buffer_ + this->prev_rx_offset_, available);
      dest_ptr += available;
      remaining -= available;
      this->prev_rx_offset_ = (this->prev_rx_offset_ + available) % this->rx_buffer_size_;
    } else {
      delay(1);
    }
  }
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
  }
#endif
  return true;
}

int STM32UARTComponent::available() {
  if (!this->rx_buffer_size_) {
    return 0;
  }
  size_t offs = this->get_tail_offset_();
  return (this->rx_buffer_size_ + offs - this->prev_rx_offset_) % this->rx_buffer_size_;
}

void STM32UARTComponent::flush() {
  // TODO
}

}  // namespace uart
}  // namespace esphome

#endif  // USE_STM32
