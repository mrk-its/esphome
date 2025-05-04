import json
import re

from esphome import platformio_api
import esphome.config_validation as cv
from esphome.const import CONF_BOARD

from .const import CONF_BOARD_SERIES

STM32_BASE_PINS = {
    "LED": 5,
}

STM32_BOARD_PINS = {}

BOARD_SERIES = {}

SERIES_RE = re.compile("(nucleo_|stm32)([fghl][0-9])", re.I)


def get_board_series(name):
    if match := SERIES_RE.match(name):
        _, family = match.groups()
        return family.upper()
    return BOARD_SERIES.get(name)


def platformio_get_board_details(board):
    boards = json.loads(
        platformio_api.run_platformio_cli(
            "boards", "--json-output", board, capture_stdout=True
        )
    )
    boards_data = [b for b in boards if b["id"] == board]
    if len(boards_data) != 1:
        raise cv.Invalid(f"Unknown board: '{board}'")
    return boards_data[0]


def platformio_get_board_series(board):
    board_data = platformio_get_board_details(board)
    mcu = board_data.get("mcu")
    return mcu and get_board_series(mcu)


def detect_board_series(platform):
    if platform.get(CONF_BOARD_SERIES):
        return platform
    board = platform[CONF_BOARD]
    board_series = get_board_series(board) or platformio_get_board_series(board)
    if not board_series:
        raise cv.Invalid(f"Can't detect board series for '{board}'")
    platform[CONF_BOARD_SERIES] = board_series
    return platform
