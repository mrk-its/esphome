import logging
from os import makedirs, write
from pathlib import Path

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
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType
from esphome.helpers import write_file, write_file_if_changed

# force import gpio to register pin schema
from .gpio import stm32_pin_to_code  # noqa

CODEOWNERS = ["@mrk-its"]
AUTO_LOAD = ["zephyr"]
IS_TARGET_PLATFORM = True

_LOGGER = logging.getLogger(__name__)


ZEPHYR_PACKAGE = "platformio/framework-zephyr@^3.40201.0"

PLATFORMIO_INI_TPL = """
; Auto generated code by esphome

[common]
lib_deps =
build_flags =
upload_flags =

; ========== AUTO GENERATED CODE BEGIN ===========

[platformio]
description =
[env:{name}]
board = {board_name}
board_frameworks =
    stm32cube
; boards_dir =
build_flags =
build_unflags =
extra_scripts =
    pre:pre_build.py
framework = zephyr
lib_deps =
    ${{common.lib_deps}}
;monitor_speed = 115200
platform = {platform}
platform_packages =
    {zephyr_package}
;upload_protocol = stlink
; =========== AUTO GENERATED CODE END ============
"""

PRE_BUILD_TPL = """
Import("env")
board_config = env.BoardConfig()
board_config.update("frameworks", ["zephyr"])
platform = env.PioPlatform()
FRAMEWORK_DIR = platform.get_package_dir("framework-zephyr")
print(FRAMEWORK_DIR)
env.Execute(f"$PYTHONEXE -m pip install {FRAMEWORK_DIR}/scripts/dts/python-devicetree")
"""

CMAKELISTS_TPL = """
cmake_minimum_required(VERSION 3.13.1)
include($ENV{{ZEPHYR_BASE}}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project({name})

FILE(GLOB app_sources ../src/*.c*)
target_sources(app PRIVATE ${{app_sources}})
# foo
"""

def write_file_if_not_exists(path: Path, contents: str):
    if not path.exists():
        print(f"writing file {path}")
        write_file_if_changed(path, contents)


def set_core_data(config: ConfigType) -> ConfigType:
    zephyr_set_core_data(config)
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "stm32"
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version(4, 2, 1)

    dst_dir = CORE.relative_build_path("")
    zephyr_dts_path = dst_dir / ".pioenvs" / f"{CORE.name}" / "zephyr" / "zephyr.dts"
    if not zephyr_dts_path.exists():
        write_file_if_not_exists(dst_dir / "src" / "foo.c", "")
        write_file_if_not_exists(dst_dir / "zephyr" / "prj.conf", "")
        write_file_if_not_exists(dst_dir / "platformio.ini", PLATFORMIO_INI_TPL.format(
            zephyr_package=ZEPHYR_PACKAGE,
            platform=config[CONF_PLATFORM],
            board_name=config[CONF_BOARD],
            name=CORE.name,

        ))
        write_file_if_not_exists(dst_dir / "pre_build.py", PRE_BUILD_TPL)
        write_file_if_not_exists(dst_dir / "zephyr/CMakeLists.txt", CMAKELISTS_TPL.format(
            name=CORE.name,
        ))
        if not CORE.data[KEY_CORE].get("pre_build"):
            from esphome import platformio_api
            platformio_api.run_platformio_cli_run(config, CORE.verbose, *["-t", "envdump"])
            print("HERE (set_core_data)", CORE.data, config)
            CORE.data[KEY_CORE]["pre_build"] = True
    if zephyr_dts_path.exists():
        from devicetree import dtlib;
        print(list(dtlib.DT(zephyr_dts_path).node_iter()))
    return config


stm32_ns = cg.esphome_ns.namespace("stm32")


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.string_strict,
            cv.Optional(KEY_BOOTLOADER, default=""): cv.string_strict,
            cv.Optional(CONF_PLATFORM, default="ststm32"): cv.string_strict,
        }
    ),
    set_core_data,
)


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config: ConfigType) -> None:
    """Convert the configuration to code."""

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    zephyr_add_prj_conf("NEWLIB_LIBC_NANO", True)

    zephyr_add_prj_conf("FLASH", True)
    zephyr_add_prj_conf("SOC_FLASH_STM32", True)
    zephyr_add_prj_conf("FLASH_STM32_OPTION_BYTES", True)
    zephyr_add_prj_conf("FLASH_EX_OP_ENABLED", True)

    zephyr_add_prj_conf("SHELL", False)
    zephyr_add_prj_conf("FLASH_SHELL", True)
    zephyr_add_prj_conf("GPIO_SHELL", True)
    zephyr_add_prj_conf("SERIAL", True)
    zephyr_add_prj_conf("SHELL_BACKEND_SERIAL", True)

    zephyr_add_prj_conf("LOG", False)
    zephyr_add_prj_conf("LOG_MODE_DEFERRED", True)
    zephyr_add_prj_conf("LOG_BUFFER_SIZE", 4096)
    zephyr_add_prj_conf("CAN_LOG_LEVEL_DBG", True)
    zephyr_add_prj_conf("LOG_DEFAULT_LEVEL", 1)

    zephyr_add_prj_conf("REBOOT", True)
    # zephyr_add_prj_conf("USE_STM32_HAL_HASH", True)
    # zephyr_add_prj_conf("USE_STM32_HAL_HASH_EX", True)

    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_platformio_option("monitor_speed", "115200")
    cg.add_platformio_option("upload_protocol", "stlink")
    cg.add_build_flag("-DUSE_STM32")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM52")

    cg.add_define(ThreadModel.SINGLE)
    cg.add_platformio_option(CONF_FRAMEWORK, CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK])
    cg.add_platformio_option("platform", config[CONF_PLATFORM])

    cg.add_platformio_option(
        "platform_packages",
        [ZEPHYR_PACKAGE],
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
