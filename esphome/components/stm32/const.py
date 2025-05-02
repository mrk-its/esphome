import esphome.codegen as cg

KEY_BOARD = "board"
KEY_STM32 = "stm32"
KEY_PIO_FILES = "pio_files"

CONF_BOARD_FAMILY = "board_family"
CONF_CLOCK = "clock"

stm32_ns = cg.esphome_ns.namespace("stm32")
