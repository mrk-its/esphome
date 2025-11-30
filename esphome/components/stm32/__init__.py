import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_PLATFORM,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_STM32,
    ThreadModel,
)
from esphome.core import CORE, coroutine_with_priority

from .boards import detect_board_details
from .clock import board_clock_config, generate_clock_config
from .const import (
    CONF_BOARD_FREQ,
    CONF_BOARD_SERIES,
    CONF_CLOCK,
    KEY_BOARD,
    KEY_DMA_CHANNELS,
    KEY_GPIO_CLOCK_ENABLED,
    KEY_STM32,
    KEY_UART_INSTANCES,
)
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
    CORE.data[KEY_STM32][CONF_BOARD_SERIES] = config[CONF_BOARD_SERIES]

    # TODO
    CORE.data[KEY_STM32][KEY_UART_INSTANCES] = [
        "USART1",
        "USART2",
        "USART3",
        "UART4",
        "UART5",
        "LPUART1",
    ]
    if config[CONF_BOARD_SERIES] in ("F1",):
        CORE.data[KEY_STM32][KEY_DMA_CHANNELS] = ["DMA1_Channel1"]
    else:
        CORE.data[KEY_STM32][KEY_DMA_CHANNELS] = ["GPDMA1_Channel1", "GPDMA1_Channel0"]

    CORE.data[KEY_STM32][KEY_GPIO_CLOCK_ENABLED] = set()
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.All(
                cv.string_strict,
            ),
            cv.Optional(CONF_PLATFORM, default="ststm32"): cv.string_strict,
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
    cg.add_platformio_option(
        "platform_packages", "toolchain-gccarmnoneeabi @ ~1.120301.0"
    )
    cg.set_cpp_standard("gnu++20")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32")
    cg.add_define(ThreadModel.SINGLE)

    cg.add_platformio_option("framework", "stm32cube")
    cg.add_platformio_option("platform", config[CONF_PLATFORM])
    cg.add_platformio_option("monitor_speed", "115200")
    cg.add_platformio_option("upload_protocol", "stlink")

    if CONF_CLOCK not in config:
        _LOGGER.warning("no clock configuraton, add 'config:' to generate default one")
    generate_clock_config(config)
