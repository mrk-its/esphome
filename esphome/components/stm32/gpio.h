#pragma once

#ifdef USE_STM32

#include "esphome/core/hal.h"
#include "core.h"

namespace esphome {
namespace stm32 {

uint16_t pin_to_mask(uint8_t pin);
GPIO_TypeDef *pin_to_port(uint8_t pin);

class STM32GPIOPin : public InternalGPIOPin {
 public:
  STM32GPIOPin() : af_{nullopt} {}
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_af(uint8_t af) { af_ = af; }
  optional<uint8_t> get_af() { return af_; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

  void setup() override { pin_mode(flags_); }
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;
  void detach_interrupt() const override;
  ISRInternalGPIOPin to_isr() const override;
  uint8_t get_pin() const override { return pin_; }
  gpio::Flags get_flags() const override { return flags_; }
  bool is_inverted() const override { return inverted_; }

 protected:
  void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const override;

  uint8_t pin_;
  optional<uint8_t> af_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace stm32
}  // namespace esphome

#endif  // USE_STM32
