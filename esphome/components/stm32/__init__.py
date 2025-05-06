import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_STM32,
)
from esphome.core import CORE, coroutine_with_priority

from .boards import detect_board_details
from .clock import board_clock_config, generate_clock_config
from .const import CONF_BOARD_FREQ, CONF_BOARD_SERIES, CONF_CLOCK, KEY_BOARD, KEY_STM32
from .gpio import stm32_pin_to_code  # noqa
from .utils import optional_dict

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@mrk-its"]
AUTO_LOAD = []
IS_TARGET_PLATFORM = True


def set_core_data(config):
    CORE.data[KEY_STM32] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_STM32
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "stm32cube"
    CORE.data[KEY_STM32][KEY_BOARD] = config[CONF_BOARD]
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.All(
                cv.string_strict,
            ),
            cv.Optional(CONF_BOARD_SERIES): cv.string_strict,
            cv.Optional(CONF_BOARD_FREQ): cv.string_strict,
            cv.Optional(CONF_CLOCK): optional_dict(dict),
        }
    ),
    detect_board_details,
    board_clock_config,
    set_core_data,
)


@coroutine_with_priority(1000)
async def to_code(config):
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_STM32")
    cg.add_build_flag("-D" + config[CONF_BOARD_SERIES])
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32")

    cg.add_platformio_option("framework", "stm32cube")
    cg.add_platformio_option("platform", "ststm32")
    cg.add_platformio_option("monitor_speed", "115200")
    cg.add_platformio_option("upload_protocol", "stlink")

    if CONF_CLOCK not in config:
        _LOGGER.warning("no clock configuraton, add 'config:' to generate default one")
    generate_clock_config(config)
