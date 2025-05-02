import logging
import os
from string import ascii_letters, digits

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_STM32,
)
from esphome.core import CORE, EsphomeError, coroutine_with_priority
from esphome.helpers import copy_file_if_changed, mkdir_p, read_file, write_file

from .boards import detect_board_family
from .clock import board_clock_config, generate_clock_config, optional_dict
from .const import CONF_BOARD_FAMILY, CONF_CLOCK, KEY_BOARD, KEY_PIO_FILES, KEY_STM32
from .gpio import stm32_pin_to_code  # noqa

# force import gpio to register pin schema
# from .gpio import rp2040_pin_to_code  # noqa

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@mrk"]
# AUTO_LOAD = ["preferences"]
AUTO_LOAD = []
IS_TARGET_PLATFORM = True


def set_core_data(config):
    CORE.data[KEY_STM32] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_STM32
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "stm32cube"
    CORE.data[KEY_STM32][KEY_BOARD] = config[CONF_BOARD]

    CORE.data[KEY_STM32][KEY_PIO_FILES] = {}

    return config


def get_download_types(storage_json):
    return [
        {
            "title": "UF2 factory format",
            "description": "For copying to RP2040 over USB.",
            "file": "firmware.uf2",
            "download": f"{storage_json.name}.factory.uf2",
        },
        {
            "title": "OTA format",
            "description": "For OTA updating a device.",
            "file": "firmware.ota.bin",
            "download": f"{storage_json.name}.ota.bin",
        },
    ]


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.All(
                cv.string_strict,
            ),
            cv.Optional(CONF_BOARD_FAMILY): cv.string_strict,
            cv.Optional(CONF_CLOCK): optional_dict(dict),
        }
    ),
    detect_board_family,
    board_clock_config,
    set_core_data,
)


@coroutine_with_priority(1000)
async def to_code(config):
    import json

    print(json.dumps(config, indent=2))
    # cg.add(stm32_ns.setup_preferences())

    # Allow LDF to properly discover dependency including those in preprocessor
    # conditionals
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_STM32")
    cg.add_build_flag("-D" + config[CONF_BOARD_FAMILY])
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32")
    # cg.add_platformio_option("extra_scripts", ["post:post_build.py"])

    cg.add_platformio_option("framework", "stm32cube")
    cg.add_platformio_option("platform", "ststm32")
    cg.add_platformio_option("monitor_speed", "115200")
    cg.add_platformio_option("upload_protocol", "stlink")

    generate_clock_config(config)

    # print(config["clock"]["foo"])


def add_pio_file(component: str, key: str, data: str):
    try:
        cv.validate_id_name(key)
    except cv.Invalid as e:
        raise EsphomeError(
            f"[{component}] Invalid PIO key: {key}. Allowed characters: [{ascii_letters}{digits}_]\nPlease report an issue https://github.com/esphome/issues"
        ) from e
    CORE.data[KEY_STM32][KEY_PIO_FILES][key] = data


def generate_pio_files() -> bool:
    import shutil

    shutil.rmtree(CORE.relative_build_path("src/pio"), ignore_errors=True)

    includes: list[str] = []
    files = CORE.data[KEY_STM32][KEY_PIO_FILES]
    if not files:
        return False
    for key, data in files.items():
        pio_path = CORE.relative_build_path(f"src/pio/{key}.pio")
        mkdir_p(os.path.dirname(pio_path))
        write_file(pio_path, data)
        includes.append(f"pio/{key}.pio.h")

    write_file(
        CORE.relative_build_path("src/pio_includes.h"),
        "#pragma once\n" + "\n".join([f'#include "{include}"' for include in includes]),
    )

    dir = os.path.dirname(__file__)
    build_pio_file = os.path.join(dir, "build_pio.py.script")
    copy_file_if_changed(
        build_pio_file,
        CORE.relative_build_path("build_pio.py"),
    )

    return True


# Called by writer.py
def copy_files():
    # dir = os.path.dirname(__file__)
    # post_build_file = os.path.join(dir, "post_build.py.script")
    # copy_file_if_changed(
    #     post_build_file,
    #     CORE.relative_build_path("post_build.py"),
    # )
    if generate_pio_files():
        path = CORE.relative_src_path("esphome.h")
        content = read_file(path).rstrip("\n")
        write_file(path, content + '\n#include "pio_includes.h"\n')
