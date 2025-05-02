#pragma once
#ifdef USE_STM32

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace stm32_can {
const char *const TAG = "stm32_can";

class STM32Can : public canbus::Canbus {
 public:
  STM32Can() {}
  // virtual ~STM32Can() {

  // }

  void setup();
  // void loop() override;
 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

 private:
#if defined(FDCAN1)
  FDCAN_HandleTypeDef hcan;
#else
  CAN_HandleTypeDef hcan;
#endif
};
}  // namespace stm32_can
}  // namespace esphome
#endif
