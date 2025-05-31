#ifdef USE_STM32
#include "logger.h"
#include "esphome/core/log.h"

extern void uart_write_str(const char *str);

namespace esphome {
namespace logger {

static const char *const TAG = "logger";

void Logger::pre_setup() {
  global_logger = this;
  hw_uart_ = nullptr;
}

void Logger::set_uart_parent(esphome::uart::UARTComponent *uart) { hw_uart_ = uart; }

void HOT Logger::write_msg_(const char *msg) {
  if (hw_uart_) {
    hw_uart_->write_array((uint8_t *) msg, strlen(msg));
    hw_uart_->write_array((uint8_t *) "\n", 1);
  }
}

}  // namespace logger
}  // namespace esphome
#endif  // USE_STM32
