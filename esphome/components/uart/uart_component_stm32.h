#pragma once

#ifdef USE_STM32

#include "esphome/core/component.h"
#include "uart_component.h"
#include "esphome/components/stm32/core.h"

namespace esphome {
namespace uart {

class STM32UARTComponent : public UARTComponent, public Component {
 public:
  STM32UARTComponent() : uart_handle_{0} {}
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS + 501.f; }

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  int available() override;
  void flush() override;

  /**
   * Load the UART with the current settings.
   * @param dump_config (Optional, default `true`): True for displaying new settings or
   * false to change it quitely
   *
   * Example:
   * ```cpp
   * id(uart1).load_settings();
   * ```
   *
   * This will load the current UART interface with the latest settings (baud_rate, parity, etc).
   */
  // void load_settings(bool dump_config) override;
  // void load_settings() override { this->load_settings(true); }

  void set_instance(USART_TypeDef *instance) { uart_handle_.Instance = instance; }
  void set_clock_initializer(void (*clock_initializer)(void)) { clock_initializer_ = clock_initializer; }
  void set_name(const char *name) { name_ = name; }
  const char *get_name() { return name_.c_str(); }

 protected:
  UART_HandleTypeDef uart_handle_;
  std::string name_;
  void (*clock_initializer_)(void);
  void check_logger_conflict() override;

  bool has_peek_{false};
  uint8_t peek_byte_;
};

}  // namespace uart
}  // namespace esphome

#endif  // USE_STM32
