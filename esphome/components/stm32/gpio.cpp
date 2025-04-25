#define USE_STM32
#ifdef USE_STM32

#include "gpio.h"
#include "esphome/core/log.h"

namespace esphome {
namespace stm32 {

static const char *const TAG = "stm32";

static GPIO_TypeDef *GPIO_PORTS[8] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH};

static GPIO_TypeDef *pin_to_port(uint8_t pin) {
  uint8_t pin_nr = pin >> 4;
  if (pin_nr < 4)
    return GPIO_PORTS[pin_nr];
  return nullptr;
}

uint16_t pin_to_mask(uint8_t pin) { return 1 << (pin & 0xf); }

// static int IRAM_ATTR flags_to_mode(gpio::Flags flags, uint8_t pin) {
//   if (flags == gpio::FLAG_INPUT) {  // NOLINT(bugprone-branch-clone)
//     return INPUT;
//   } else if (flags == gpio::FLAG_OUTPUT) {
//     return OUTPUT;
//   } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
//     return INPUT_PULLUP;
//   } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN)) {
//     return INPUT_PULLDOWN;
//     // } else if (flags == (gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) {
//     //   return OpenDrain;
//   } else {
//     return 0;
//   }
// }

struct ISRPinArg {
  uint8_t pin;
  bool inverted;
};

ISRInternalGPIOPin STM32GPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = pin_;
  arg->inverted = inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void STM32GPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  // PinStatus arduino_mode = LOW;
  // switch (type) {
  //   case gpio::INTERRUPT_RISING_EDGE:
  //     arduino_mode = inverted_ ? FALLING : RISING;
  //     break;
  //   case gpio::INTERRUPT_FALLING_EDGE:
  //     arduino_mode = inverted_ ? RISING : FALLING;
  //     break;
  //   case gpio::INTERRUPT_ANY_EDGE:
  //     arduino_mode = CHANGE;
  //     break;
  //   case gpio::INTERRUPT_LOW_LEVEL:
  //     arduino_mode = inverted_ ? HIGH : LOW;
  //     break;
  //   case gpio::INTERRUPT_HIGH_LEVEL:
  //     arduino_mode = inverted_ ? LOW : HIGH;
  //     break;
  // }

  // attachInterrupt(pin_, func, arduino_mode, arg);
}
void STM32GPIOPin::pin_mode(gpio::Flags flags) {
  // pinMode(pin_, flags_to_mode(flags, pin_));  // NOLINT
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = pin_to_mask(pin_);
  if (flags & gpio::Flags::FLAG_INPUT) {
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  } else {
    GPIO_InitStruct.Mode = (flags & gpio::Flags::FLAG_OPEN_DRAIN) ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT_PP;
  }
  GPIO_InitStruct.Pull = (flags & gpio::Flags::FLAG_PULLUP)
                             ? GPIO_PULLUP
                             : ((flags & gpio::Flags::FLAG_PULLDOWN) ? GPIO_PULLDOWN : GPIO_NOPULL);
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;

  // GPIO_TypeDef *port = pin_to_port(pin_);
  GPIO_TypeDef *port = GPIOA;
  if (port) {
    HAL_GPIO_Init(port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(port, pin_to_mask(pin_), GPIO_PIN_RESET);
  }
}

std::string STM32GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", pin_);
  return buffer;
}

bool STM32GPIOPin::digital_read() {
  // return bool(digitalRead(pin_)) != inverted_;  // NOLINT
  return false;
}

void STM32GPIOPin::digital_write(bool value) {
  // GPIO_TypeDef *port = pin_to_port(pin_);
  GPIO_TypeDef *port = GPIOA;

  if (port) {
    HAL_GPIO_WritePin(port, pin_to_mask(pin_), value ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
}

void STM32GPIOPin::detach_interrupt() const {
  // detachInterrupt(pin_);
}

}  // namespace stm32

using namespace stm32;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  // auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  // return bool(digitalRead(arg->pin)) != arg->inverted;  // NOLINT
  return false;
}
void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  // auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  // digitalWrite(arg->pin, value != arg->inverted ? 1 : 0);  // NOLINT
}
void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  // TODO: implement
  // auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  // GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1UL << arg->pin);
}

void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  // auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  // pinMode(arg->pin, flags_to_mode(flags, arg->pin));  // NOLINT
}

}  // namespace esphome

#endif  // USE_STM32