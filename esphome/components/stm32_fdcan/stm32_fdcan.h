#pragma once
#ifdef USE_STM32

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace stm32_fdcan {
const char *const TAG = "stm32_fdcan";

class OnInitializedTrigger : public Trigger<> {};

class STM32FDCan : public canbus::Canbus {
 public:
  STM32FDCan() : hcan{0}, tx_pin_{0}, rx_pin_{0}, on_initialized_{0} {}
  // virtual ~STM32FDCan() {

  // }

  void setup();
  // void loop() override;
  void set_tx_pin(InternalGPIOPin *tx_pin) { tx_pin_ = tx_pin; }
  void set_rx_pin(InternalGPIOPin *rx_pin) { rx_pin_ = rx_pin; }
  void loop() override;

  void add_trigger(OnInitializedTrigger *trigger) { on_initialized_ = trigger; }

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

 private:
  FDCAN_HandleTypeDef hcan;
  InternalGPIOPin *tx_pin_;
  InternalGPIOPin *rx_pin_;
  OnInitializedTrigger *on_initialized_;
};
}  // namespace stm32_fdcan
}  // namespace esphome
#endif
