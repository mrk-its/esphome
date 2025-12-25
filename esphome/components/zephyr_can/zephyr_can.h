#pragma once
#ifdef USE_ZEPHYR

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace zephyr_can {
const char *const TAG = "zephyr_can";

extern "C" void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);

class OnInitializedTrigger : public Trigger<> {};

class ZephyrCan : public canbus::Canbus {
 public:
  ZephyrCan(const device *can_dev) : can_dev_(can_dev) {}

  void setup();
  void loop() override;

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

 private:
  const device *can_dev_{};
};
}  // namespace zephyr_can
}  // namespace esphome
#endif
