#define USE_STM32
#ifdef USE_STM32

#include "gpio.h"
#include "esphome/core/log.h"

namespace esphome {
namespace stm32 {

static const char *const TAG = "stm32";

static GPIO_TypeDef *GPIO_PORTS[9] = {GPIOA, GPIOB, GPIOC,
#ifdef __HAL_RCC_GPIOD_CLK_ENABLEX
                                      GPIOD,
#else
                                      nullptr,
#endif
#ifdef __HAL_RCC_GPIOE_CLK_ENABLEX
                                      GPIOE,
#else
                                      nullptr,
#endif
#ifdef __HAL_RCC_GPIOF_CLK_ENABLEX
                                      GPIOF,
#else
                                      nullptr,
#endif
#ifdef __HAL_RCC_GPIOG_CLK_ENABLEX
                                      GPIOG,
#else
                                      nullptr,
#endif
#ifdef __HAL_RCC_GPIOH_CLK_ENABLEX
                                      GPIOH,
#else
                                      nullptr,
#endif
#ifdef __HAL_RCC_GPIOI_CLK_ENABLEX
                                      GPIOI
#else
                                      nullptr
#endif
};

static GPIO_TypeDef *pin_to_port(uint8_t pin) {
  uint8_t pin_nr = pin >> 4;
  if (pin_nr < 9)
    return GPIO_PORTS[pin_nr];
  return nullptr;
}

uint16_t pin_to_mask(uint8_t pin) { return 1 << (pin & 0xf); }

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
  // TODO
}

static uint16_t initialized_ports = 0;

void _pin_mode(uint8_t pin, gpio::Flags flags) {
  uint16_t port_index = pin >> 4;
  uint16_t port_mask = 1 << port_index;
  if (!(initialized_ports & port_mask)) {
    switch (port_index) {
      case 0:
#ifdef __HAL_RCC_GPIOA_CLK_ENABLE
        __HAL_RCC_GPIOA_CLK_ENABLE();
#endif
        break;
      case 1:
#ifdef __HAL_RCC_GPIOB_CLK_ENABLE
        __HAL_RCC_GPIOB_CLK_ENABLE();
#endif
        break;
      case 2:
#ifdef __HAL_RCC_GPIOC_CLK_ENABLE
        __HAL_RCC_GPIOC_CLK_ENABLE();
#endif
        break;
      case 3:
#ifdef __HAL_RCC_GPIOD_CLK_ENABLE
        __HAL_RCC_GPIOD_CLK_ENABLE();
#endif
        break;
      case 4:
#ifdef __HAL_RCC_GPIOE_CLK_ENABLE
        __HAL_RCC_GPIOE_CLK_ENABLE();
#endif
        break;
      case 5:
#ifdef __HAL_RCC_GPIOF_CLK_ENABLE
        __HAL_RCC_GPIOF_CLK_ENABLE();
#endif
        break;
      case 6:
#ifdef __HAL_RCC_GPIOG_CLK_ENABLE
        __HAL_RCC_GPIOG_CLK_ENABLE();
#endif
        break;
      case 7:
#ifdef __HAL_RCC_GPIOH_CLK_ENABLE
        __HAL_RCC_GPIOH_CLK_ENABLE();
#endif
        break;
      case 8:
#ifdef __HAL_RCC_GPIOI_CLK_ENABLE
        __HAL_RCC_GPIOI_CLK_ENABLE();
#endif
        break;
    }
    initialized_ports |= port_mask;
  }
  GPIO_TypeDef *port = pin_to_port(pin);
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = pin_to_mask(pin);
  if (flags & gpio::Flags::FLAG_INPUT) {
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  } else {
    GPIO_InitStruct.Mode = (flags & gpio::Flags::FLAG_OPEN_DRAIN) ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT_PP;
  }
  GPIO_InitStruct.Pull = (flags & gpio::Flags::FLAG_PULLUP)
                             ? GPIO_PULLUP
                             : ((flags & gpio::Flags::FLAG_PULLDOWN) ? GPIO_PULLDOWN : GPIO_NOPULL);
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;

  if (port) {
    HAL_GPIO_Init(port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(port, pin_to_mask(pin), GPIO_PIN_RESET);
  }
}

bool _digital_read(uint8_t pin) {
  GPIO_TypeDef *port = pin_to_port(pin);
  return HAL_GPIO_ReadPin(port, pin_to_mask(pin));
}

void _digital_write(uint8_t pin, bool value) {
  GPIO_TypeDef *port = pin_to_port(pin);
  if (port) {
    HAL_GPIO_WritePin(port, pin_to_mask(pin), value ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
}
void STM32GPIOPin::pin_mode(gpio::Flags flags) { _pin_mode(pin_, flags); }

void STM32GPIOPin::digital_write(bool value) { _digital_write(pin_, value); }

std::string STM32GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", pin_);
  return buffer;
}

bool STM32GPIOPin::digital_read() { return _digital_read(pin_); }

void STM32GPIOPin::detach_interrupt() const {
  // TODO
}

}  // namespace stm32

using namespace stm32;

void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  auto pin_ = ((struct ISRPinArg *) arg_)->pin;
  _pin_mode(pin_, flags);
}

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto pin_ = ((struct ISRPinArg *) arg_)->pin;
  return _digital_read(pin_);
}

void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto pin_ = ((struct ISRPinArg *) arg_)->pin;
  _digital_write(pin_, value);
}

void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  // TODO
}

}  // namespace esphome

#endif  // USE_STM32