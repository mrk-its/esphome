import re

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANALOG,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.core import CORE

from . import boards
from .const import KEY_BOARD, KEY_STM32, stm32_ns

STM32GPIOPin = stm32_ns.class_("STM32GPIOPin", cg.InternalGPIOPin)

PIN_RE = re.compile("^P([A-P])(0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15)$")


def _lookup_pin(value):
    board = CORE.data[KEY_STM32][KEY_BOARD]
    board_pins = boards.STM32_BOARD_PINS.get(board, {})

    while isinstance(board_pins, str):
        board_pins = boards.STM32_BOARD_PINS[board_pins]

    if value in board_pins:
        return board_pins[value]
    if value in boards.STM32_BASE_PINS:
        return boards.STM32_BASE_PINS[value]
    raise cv.Invalid(f"Cannot resolve pin name '{value}' for board {board}.")


def _translate_pin(value):
    if isinstance(value, dict) or value is None:
        raise cv.Invalid(
            "This variable only supports pin numbers, not full pin schemas "
            "(with inverted and mode)."
        )
    if isinstance(value, int) and not isinstance(value, bool):
        return value
    if not isinstance(value, str):
        raise cv.Invalid(f"Invalid pin number: {value}")

    parsed = PIN_RE.match(value)
    if parsed:
        port_nr = ord(parsed[1]) - ord("A")
        pin = int(parsed[2])
        return port_nr * 16 + pin
    try:
        return int(value)
    except ValueError:
        pass
    if value.startswith("GPIO"):
        return cv.int_(value[len("GPIO") :].strip())
    return _lookup_pin(value)


def validate_gpio_pin(value):
    value = _translate_pin(value)
    if value < 0 or value > 63:
        raise cv.Invalid(f"STM32: Invalid pin number: {value}")
    return value


def validate_supports(value):
    board = CORE.data[KEY_STM32][KEY_BOARD]
    if board != "rpipicow" or value[CONF_NUMBER] != 32:
        return value
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_open_drain = mode[CONF_OPEN_DRAIN]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]
    if not is_output or is_input or is_open_drain or is_pullup or is_pulldown:
        raise cv.Invalid("Only output mode is supported for Pico-w LED pin")
    return value


STM32_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        STM32GPIOPin,
        validate_gpio_pin,
        modes=pins.GPIO_STANDARD_MODES + (CONF_ANALOG,),
    ),
    validate_supports,
)


@pins.PIN_SCHEMA_REGISTRY.register("stm32", STM32_PIN_SCHEMA)
async def stm32_pin_to_code(config):
    # cg.add(cg.RawExpression(f"__HAL_RCC_GPIOA_CLK_ENABLE()"))
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
