#pragma once

#ifdef USE_STM32

#include "esphome/core/component.h"
#include "uart_component.h"
#include "esphome/components/stm32/core.h"

namespace esphome {
namespace uart {

class STM32UARTComponent : public UARTComponent, public Component {
 public:
  STM32UARTComponent() {}
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS + 501.f; }

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  int available() override;
  void flush() override;

  void set_instance(USART_TypeDef *instance) { this->uart_handle_.Instance = instance; }
  void set_name(const char *name) { this->name_ = name; }
  const char *get_name() { return this->name_.c_str(); }

 protected:
  void check_logger_conflict() override {}
  UART_HandleTypeDef uart_handle_{0};
  std::string name_;
};

}  // namespace uart
}  // namespace esphome

#endif  // USE_STM32
