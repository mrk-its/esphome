import esphome.codegen as cg

KEY_BOARD = "board"
KEY_STM32 = "stm32"
KEY_UART_INSTANCES = "uart_instances"

CONF_CLOCK = "clock"
CONF_BOARD_SERIES = "board_series"
CONF_BOARD_FREQ = "board_freq"
CONF_AF = "af"

stm32_ns = cg.esphome_ns.namespace("stm32")
