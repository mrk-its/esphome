from __future__ import annotations

from esphome import platformio_api
import esphome.codegen as cg
from esphome.components.zephyr import (
    copy_files as zephyr_copy_files,
    zephyr_add_prj_conf,
    zephyr_set_core_data,
    zephyr_to_code,
)
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_PLATFORM,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    ThreadModel,
)
from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.helpers import read_file, write_file_if_changed
from esphome.types import ConfigType

from .const import PLATFORM_STM32

# force import gpio to register pin schema
from .gpio import stm32_pin_to_code  # noqa

CODEOWNERS = ["@mrk-its"]
AUTO_LOAD = ["zephyr"]
IS_TARGET_PLATFORM = True


def set_core_data(config: ConfigType) -> ConfigType:
    zephyr_set_core_data(config)
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_STM32
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version(4, 2, 1)

    return config


def write_file_if_doesnt_exist(path, contents):
    try:
        read_file(path)
    except EsphomeError:
        write_file_if_changed(path, contents)


def validate_board(config):
    write_file_if_changed(CORE.relative_build_path("tmp/zephyr/prj.conf"), "")
    write_file_if_changed(CORE.relative_build_path("tmp/main.cpp"), "")
    write_file_if_changed(
        CORE.relative_build_path("tmp/cxx_flags.py"),
        """\
# Auto-generated ESPHome script for C++ specific compiler flags
Import("env")

# Add C++ specific flags
env.Append(CXXFLAGS=["-Wno-volatile"])
""",
    )
    write_file_if_changed(
        CORE.relative_build_path("tmp/pre_build.py"),
        """\
Import("env")

board_config = env.BoardConfig()
board_config.update("frameworks", ["arduino", "zephyr"])
""",
    )
    write_file_if_changed(
        CORE.relative_build_path("tmp/platformio.ini"),
        """\
[common]
lib_deps =
build_flags =
upload_flags =

[platformio]
description = ESPHome 2026.1.0-dev
[env:led-blink-u5-zephyr]
board = genericSTM32U535CE
board_frameworks =
    stm32cube
boards_dir = /home/mrk/repos/stm32/.esphome/build/led-blink-u5-zephyr/boards
build_flags =
    -DUSER_VECT_TAB_ADDRESS -Wl,-u_printf_float -Os
    -DESPHOME_LOG_LEVEL=ESPHOME_LOG_LEVEL_DEBUG
    -DUSE_STM32
    -DUSE_ZEPHYR
    -Wno-sign-compare
    -Wno-unused-but-set-variable
    -Wno-unused-variable
    -fno-exceptions
    -std=gnu++20
build_unflags =
    -std=gnu++11
    -std=gnu++14
    -std=gnu++17
    -std=gnu++23
    -std=gnu++2a
    -std=gnu++2b
    -std=gnu++2c
extra_scripts =
    pre:pre_build.py
    pre:cxx_flags.py
framework = zephyr
lib_deps =
    ${common.lib_deps}
monitor_speed = 115200
platform = https://github.com/mrk-its/platform-ststm32.git
platform_packages =
    platformio/framework-zephyr@^3.40201.0
upload_protocol = stlink
    """,
    )
    platformio_api.run_platformio_cli(
        "run", "-d", str(CORE.relative_build_path("tmp")), "-t", "envdump"
    )
    # raise cv.Invalid("invalid board")
    return config


stm32_ns = cg.esphome_ns.namespace("stm32")


CONFIG_SCHEMA = cv.All(
    set_core_data,
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.string_strict,
            cv.Optional(KEY_BOOTLOADER): cv.string_strict,
            cv.Optional(CONF_PLATFORM, default="ststm32"): cv.string_strict,
        }
    ),
    # validate_board,
)


def _final_validate(config):
    pass


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config: ConfigType) -> None:
    """Convert the configuration to code."""

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)

    zephyr_add_prj_conf("CONFIG_SERIAL", True)
    zephyr_add_prj_conf("UART_CONSOLE", True)
    zephyr_add_prj_conf("CONSOLE", True)

    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_platformio_option("monitor_speed", "115200")
    cg.add_platformio_option("upload_protocol", "stlink")
    cg.add_build_flag("-DUSE_STM32")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM52")
    # nRF52 processors are single-core
    cg.add_define(ThreadModel.SINGLE)
    cg.add_platformio_option(CONF_FRAMEWORK, CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK])
    cg.add_platformio_option("platform", config[CONF_PLATFORM])

    cg.add_platformio_option(
        "platform_packages",
        ["platformio/framework-zephyr@^3.40201.0"],
    )

    zephyr_to_code(config)


def copy_files() -> None:
    """Copy files to the build directory."""
    zephyr_copy_files()


def _upload_using_platformio(
    config: ConfigType, port: str, upload_args: list[str]
) -> int | str:
    from esphome import platformio_api

    if port is not None:
        upload_args += ["--upload-port", port]
    return platformio_api.run_platformio_cli_run(config, CORE.verbose, *upload_args)


def upload_program(config: ConfigType, args, host: str) -> bool:
    _upload_using_platformio(config, host, ["-t", "upload"])
    return True
