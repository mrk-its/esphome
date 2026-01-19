#pragma once
#ifdef USE_ZEPHYR

#include <zephyr/drivers/can.h>
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace zephyr_can {
const char *const TAG = "zephyr_can";

class OnInitializedTrigger : public Trigger<> {};

class ZephyrCan : public canbus::Canbus {
 public:
  ZephyrCan(const device *can_dev, struct k_msgq *rx_queue) : can_dev_(can_dev), rx_queue_(rx_queue) {}

  void setup();
  void loop() override;

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

 private:
  const device *can_dev_{};
  struct k_msgq *rx_queue_{};
};
}  // namespace zephyr_can
}  // namespace esphome
#endif
