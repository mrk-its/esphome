#pragma once

#ifdef USE_STM32

#include "esphome/core/hal.h"
#include "core.h"

namespace esphome {
namespace stm32 {

uint16_t pin_to_mask(uint8_t pin);

class STM32GPIOPin : public InternalGPIOPin {
 public:
  STM32GPIOPin() : af_{nullopt} {}
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_af(uint8_t af) { this->af_ = af; }
  optional<uint8_t> get_af() { return this->af_; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }
  void set_port(GPIO_TypeDef *port) { this->port_ = port; }

  void setup() override { this->pin_mode(this->flags_); }
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;
  void detach_interrupt() const override;
  ISRInternalGPIOPin to_isr() const override;
  uint8_t get_pin() const override { return this->pin_; }
  gpio::Flags get_flags() const override { return this->flags_; }
  bool is_inverted() const override { return this->inverted_; }

 protected:
  void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const override;

  GPIO_TypeDef *port_;
  uint8_t pin_;
  optional<uint8_t> af_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace stm32
}  // namespace esphome

#endif  // USE_STM32
