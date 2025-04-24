#ifdef USE_STM32
#include "logger.h"
#include "esphome/core/log.h"

extern void uart_write_str(const char *str);

namespace esphome {
namespace logger {

static const char *const TAG = "logger";

void Logger::pre_setup() { global_logger = this; }

void HOT Logger::write_msg_(const char *msg) {
  ::uart_write_str(msg);
  ::uart_write_str("\n");
}

const char *const UART_SELECTIONS[] = {"UART0", "UART1", "UART2"};

const char *Logger::get_uart_selection_() { return UART_SELECTIONS[2]; }

}  // namespace logger
}  // namespace esphome
#endif  // USE_STM32
