#pragma once
#ifdef USE_STM32

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/stm32/can_ring_buffer.h"

namespace esphome {
namespace stm32_can {
const char *const TAG = "stm32_can";

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);

class STM32Can : public canbus::Canbus {
  friend void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);

 public:
  STM32Can() { rx_fifo_ = new stm32::CanFrameRingBuffer(31); }

  void setup();
  // void loop() override;

  void set_tx_pin(InternalGPIOPin *tx_pin) { tx_pin_ = tx_pin; }
  void set_rx_pin(InternalGPIOPin *rx_pin) { rx_pin_ = rx_pin; }

  void set_instance(CAN_TypeDef *instance) { hcan_.Instance = instance; }

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;
  void push_can_frame(CAN_HandleTypeDef *hcan, struct canbus::CanFrame *frame);

 private:
  CAN_HandleTypeDef hcan_;
  InternalGPIOPin *tx_pin_;
  InternalGPIOPin *rx_pin_;
  stm32::CanFrameRingBuffer *rx_fifo_;
};
}  // namespace stm32_can
}  // namespace esphome
#endif
