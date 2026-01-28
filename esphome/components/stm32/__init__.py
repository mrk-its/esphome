import logging

import esphome.codegen as cg
from esphome.components.zephyr import (
    copy_files as zephyr_copy_files,
    zephyr_add_prj_conf,
    zephyr_set_core_data,
    zephyr_to_code,
    devicetree,
)
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_ZEPHYR, KEY_DEVICETREE
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

from pathlib import Path
from platformio.platform.factory import PlatformFactory

# force import gpio to register pin schema
from .gpio import stm32_pin_to_code  # noqa

CODEOWNERS = ["@mrk-its"]
AUTO_LOAD = ["zephyr"]
IS_TARGET_PLATFORM = True

ZEPHYR_PACKAGE = "platformio/framework-zephyr@^3.40201.0"
TOOLCHAIN_PACKAGE = "platformio/toolchain-gccarmnoneeabi@1.120301.0"

_LOGGER = logging.getLogger(__name__)


class STM32DeviceTreeParser(devicetree.BaseDeviceTreeParser):
    def __init__(self, platform, zephyr_pkg, toolchain_pkg):
        platform = PlatformFactory.new(platform, autoinstall=True)
        platform._custom_packages = [zephyr_pkg, toolchain_pkg]
        platform.install_required_packages()

        zephyr_path = Path(platform.get_package("framework-zephyr").path)
        toolchain_path = Path(platform.get_package("toolchain-gccarmnoneeabi").path)
        gcc_path = toolchain_path / "bin" / "arm-none-eabi-gcc"

        super().__init__(zephyr_path, gcc_path)


def _parse_devicetree(config: ConfigType) -> ConfigType:
    if KEY_DEVICETREE not in CORE.data[KEY_CORE]:
        parser = STM32DeviceTreeParser(config[CONF_PLATFORM], ZEPHYR_PACKAGE, TOOLCHAIN_PACKAGE)
        dt = parser.get_board_dt(config[CONF_BOARD])
        CORE.data[KEY_CORE][KEY_DEVICETREE] = dt
    return config


def set_core_data(config: ConfigType) -> ConfigType:
    zephyr_set_core_data(config)
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "stm32"
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version(4, 2, 1)

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
    _parse_devicetree,
    set_core_data,
)


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config: ConfigType) -> None:
    """Convert the configuration to code."""

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    zephyr_add_prj_conf("NEWLIB_LIBC_NANO", True)

    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_platformio_option("monitor_speed", "115200")
    cg.add_platformio_option("upload_protocol", "stlink")
    cg.add_build_flag("-DUSE_STM32")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM52")

    cg.add_define(ThreadModel.SINGLE)
    cg.add_platformio_option(CONF_FRAMEWORK, CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK])
    cg.add_platformio_option("platform", config[CONF_PLATFORM])

    cg.add_platformio_option("platform_packages", [ZEPHYR_PACKAGE, TOOLCHAIN_PACKAGE])

    zephyr_to_code(config)


def copy_files() -> None:
    """Copy files to the build directory."""
    zephyr_copy_files(overlay_filename = CORE.data["zephyr"]['board'] + ".overlay")


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
