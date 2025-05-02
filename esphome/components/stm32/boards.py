import re

import esphome.config_validation as cv
from esphome.const import CONF_BOARD

from .const import CONF_BOARD_FAMILY

STM32_BASE_PINS = {
    "LED": 5,
}

STM32_BOARD_PINS = {}

BOARD_FAMILIES = {
    "cloud_jam": "F4",
    "cloud_jam_l4": "L4",
}

FAMILY_RE = re.compile("(nucleo_|stm32)([fghl][0-9])")


def get_board_family(name):
    if match := FAMILY_RE.match(name):
        _, family = match.groups()
        return family.upper()
    return BOARD_FAMILIES.get(name)


def detect_board_family(platform):
    if platform.get(CONF_BOARD_FAMILY):
        return platform
    board = platform[CONF_BOARD]
    board_family = get_board_family(board)
    if not board_family:
        raise cv.Invalid(f"Can't determine board family for {board}")
    platform[CONF_BOARD_FAMILY] = board_family
    return platform
