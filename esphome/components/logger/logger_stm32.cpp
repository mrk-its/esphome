#ifdef USE_STM32
#include "logger.h"
#include "esphome/core/log.h"
#include "esphome/components/stm32/core.h"

extern void uart_write_str(const char *str);

namespace esphome {
namespace logger {

static const char *const TAG = "logger";

void Logger::pre_setup() { global_logger = this; }

void HOT Logger::write_msg_(const char *msg) {
  ::esphome::stm32::uart_write_str(msg);
  ::esphome::stm32::uart_write_str("\n");
}

const char *const UART_SELECTIONS[] = {"UART2"};

const char *Logger::get_uart_selection_() { return UART_SELECTIONS[0]; }

}  // namespace logger
}  // namespace esphome
#endif  // USE_STM32
