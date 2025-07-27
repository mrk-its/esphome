#ifdef USE_STM32

#include "gpio.h"
#include "esphome/core/log.h"

namespace esphome {
namespace stm32 {

static const char *const TAG = "stm32";

uint16_t pin_to_mask(uint8_t pin) { return 1 << (pin & 0xf); }

struct ISRPinArg {
  GPIO_TypeDef *port;
  uint8_t pin;
  bool inverted;
  optional<uint8_t> af;
};

ISRInternalGPIOPin STM32GPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = this->pin_;
  arg->inverted = this->inverted_;
  arg->af = this->af_;
  return ISRInternalGPIOPin((void *) arg);
}

void STM32GPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  // TODO
}

void _pin_mode(GPIO_TypeDef *port, uint8_t pin, gpio::Flags flags, optional<uint8_t> af) {
  uint16_t port_index = pin >> 4;
  uint16_t port_mask = 1 << port_index;
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = pin_to_mask(pin);
  if (flags & gpio::Flags::FLAG_INPUT) {
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  } else {
    if (!af) {
      GPIO_InitStruct.Mode = flags & gpio::Flags::FLAG_OPEN_DRAIN ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT_PP;
    } else {
      GPIO_InitStruct.Mode = flags & gpio::Flags::FLAG_OPEN_DRAIN ? GPIO_MODE_AF_OD : GPIO_MODE_AF_PP;
    }
  }
  GPIO_InitStruct.Pull = (flags & gpio::Flags::FLAG_PULLUP)
                             ? GPIO_PULLUP
                             : ((flags & gpio::Flags::FLAG_PULLDOWN) ? GPIO_PULLDOWN : GPIO_NOPULL);
// TODO
#ifndef F1
  if (af) {
    GPIO_InitStruct.Alternate = *af;
  }
#endif

#ifdef GPIO_SPEED_HIGH
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
#endif

  HAL_GPIO_Init(port, &GPIO_InitStruct);
}

bool _digital_read(GPIO_TypeDef *port, uint8_t pin) { return HAL_GPIO_ReadPin(port, pin_to_mask(pin)); }

void _digital_write(GPIO_TypeDef *port, uint8_t pin, bool value) {
  HAL_GPIO_WritePin(port, pin_to_mask(pin), value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
void STM32GPIOPin::pin_mode(gpio::Flags flags) { _pin_mode(this->port_, this->pin_, flags, this->af_); }

void STM32GPIOPin::digital_write(bool value) { _digital_write(this->port_, this->pin_, value != this->inverted_); }

std::string STM32GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "P%c%u", 'A' + (pin_ >> 4), pin_ & 0xf);
  return buffer;
}

bool STM32GPIOPin::digital_read() { return _digital_read(this->port_, pin_) != inverted_; }

void STM32GPIOPin::detach_interrupt() const {
  // TODO
}

}  // namespace stm32

using namespace stm32;

void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  auto ptr = ((struct ISRPinArg *) arg_);
  _pin_mode(ptr->port, ptr->pin, flags, ptr->af);
}

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto ptr = ((struct ISRPinArg *) arg_);
  return bool(ptr->inverted) != _digital_read(ptr->port, ptr->pin);
}

void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto ptr = ((struct ISRPinArg *) arg_);
  _digital_write(ptr->port, ptr->pin, value != bool(ptr->inverted));
}

void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  // TODO
}

}  // namespace esphome

#endif  // USE_STM32
