"""STM32-family zephyr_to_code hook.

No pinctrl-specific code yet -- STM32's I2C/UART/SPI pins are a small,
per-signal set of pre-named alternate-function phandles Zephyr already
declares (not free-mux like esp32/nordic/silabs), and remapping them isn't
implemented yet. See platform_zephyr_todo memory for the open item."""

import logging
from pathlib import Path
import shutil
import subprocess

import yaml

from esphome.core import CORE, EsphomeError
from esphome.types import ConfigType

from ..const import KEY_RUNNER, SEGGER_RTT_UART
from . import mcuboot_or_none

_LOGGER = logging.getLogger(__name__)

# Software UART drivers (devicetree compatible strings) that work on this family,
# on top of its hardware UARTs.
UART_DRIVERS: frozenset[str] = frozenset({SEGGER_RTT_UART})

# Vendor HAL west projects this family builds against on mainline Zephyr; the other
# vendor HALs are not fetched.
WEST_HALS: frozenset[str] = frozenset({"hal_stm32"})


def to_code(config: ConfigType) -> None:
    """REQUIRES_FULL_LIBCPP selects GLIBCXX_LIBCPP; without it Zephyr defaults
    to MINIMAL_LIBCPP, which has no STL and breaks ESPHome's C++ headers."""
    import esphome.codegen as cg  # noqa: PLC0415
    from esphome.components.stm32.const import (  # noqa: PLC0415
        VARIANT_STM32C5,
        VARIANT_STM32F1,
        VARIANT_STM32F4,
    )
    from esphome.const import CONF_FRAMEWORK, CONF_LOG_LEVEL  # noqa: PLC0415

    from .. import zephyr_add_prj_conf, zephyr_variant  # noqa: PLC0415 -- avoids circular import at module load

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    # STM32F1 is Cortex-M3, which has no FPU.
    if zephyr_variant() != VARIANT_STM32F1:
        zephyr_add_prj_conf("FPU", True)
    # random_bytes() uses sys_rand_get(), which requires the entropy subsystem.
    # STM32F4 is a whole chip family, not a single SoC -- RNG presence varies per
    # member (F401/F411 have none, F405/F410/F412 and larger do), so stm32f4.py
    # resolves this itself from the board's own DTS instead of a blanket default.
    # STM32F1 has no true RNG on any family member (dts/arm/st/f1 has no rng@ node
    # at all), so stm32f1.py always uses TEST_RANDOM_GENERATOR unconditionally.
    # STM32C5's SoC dtsi has no RNG node either; stm32c5.py looks it up per board.
    if zephyr_variant() not in (
        VARIANT_STM32C5,
        VARIANT_STM32F1,
        VARIANT_STM32F4,
    ):
        zephyr_add_prj_conf("ENTROPY_GENERATOR", True)
    # ARM Cortex-M's ARCH_HAS_STACKWALK only defaults on when EXTRA_EXCEPTION_INFO
    # is also set (arch/arm/core/Kconfig selects the dependency it needs).
    log_level = config[CONF_FRAMEWORK][CONF_LOG_LEVEL]
    if log_level != "NONE":
        zephyr_add_prj_conf("EXTRA_EXCEPTION_INFO", True)
        zephyr_add_prj_conf("EXCEPTION_STACK_TRACE", True)
    # Consumed by C++ code shared across every stm32-family variant (core.cpp, etc.).
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_STM32")


def bootloader(advanced: ConfigType) -> str:
    """Return the bootloader set_core_data() gets, rebuilt from the cached advanced: block."""
    return mcuboot_or_none(advanced)


# Flashing through an STM32 DfuSe bootloader with Zephyr's dfu-util runner. Until
# zephyrproject-rtos/zephyr#126198, that runner starts the first image of a multi-image
# build (MCUboot) before the others are written, so MCUboot builds need a Zephyr with it.

_DFU_UTIL_RUNNER = "dfu-util"

_DFU_BOARD_CMAKE = """\
# Added by ESPHome: the STM32 ROM DFU bootloader, which the board's own files leave out.
board_runner_args(dfu-util "--pid=0483:df11" "--alt=0" "--dfuse")
include(${ZEPHYR_BASE}/boards/common/dfu-util.board.cmake)
"""


def _needs_dfu_runner() -> bool:
    """Whether the board is flashed over DFU but its board files declare no dfu-util runner."""
    from esphome.components.stm32.const import VARIANT_STM32C5  # noqa: PLC0415

    from .. import zephyr_variant  # noqa: PLC0415

    if zephyr_variant() == VARIANT_STM32C5:
        from . import stm32c5  # noqa: PLC0415

        return stm32c5.flashes_over_rom_dfu()
    return False


def write_dfu_board_root() -> tuple[Path | None, bool]:
    """Write a board root whose board extension adds the dfu-util runner, for every
    image of the build. Returns the root (None when the board needs none) and whether
    its files changed, which needs a fresh CMake configure."""
    from esphome.helpers import rmtree, write_file_if_changed  # noqa: PLC0415

    from .. import zephyr_rtos_data  # noqa: PLC0415
    from ..board_revision import parse_board_string  # noqa: PLC0415
    from ..const import KEY_BOARD  # noqa: PLC0415

    root = CORE.relative_build_path("zephyr/dfu_board_root")
    if not _needs_dfu_runner():
        if root.is_dir():
            rmtree(root)
            return None, True
        return None, False
    name = parse_board_string(zephyr_rtos_data()[KEY_BOARD]).name
    board_dir = root / "boards" / "esphome" / f"{name}_dfu"
    changed = write_file_if_changed(
        board_dir / "board.yml", f"board:\n  extend: {name}\n"
    )
    changed |= write_file_if_changed(board_dir / "board.cmake", _DFU_BOARD_CMAKE)
    return root, changed


def _runners_yaml(build_dir: Path) -> dict:
    from ..build_zephyr import _find_runners_yaml  # noqa: PLC0415

    try:
        with _find_runners_yaml(build_dir).open(encoding="utf-8") as f:
            return yaml.safe_load(f) or {}
    except (OSError, yaml.YAMLError):
        return {}


def _flashes_with_dfu_util() -> bool:
    """Whether the build flashes with dfu-util: the advanced: runner:, else the board's
    default, or a board flashed over DFU anyway."""
    from .. import zephyr_rtos_data  # noqa: PLC0415

    if _needs_dfu_runner():
        return True
    runner = zephyr_rtos_data().get(KEY_RUNNER)
    if runner is None:
        runner = _runners_yaml(CORE.relative_build_path(".west_build")).get(
            "flash-runner"
        )
    return runner == _DFU_UTIL_RUNNER


def uses_dfu(host: str) -> bool:
    """`--device DFU`, or a serial port when the build flashes with dfu-util."""
    from esphome.upload_targets import PortType, get_port_type  # noqa: PLC0415

    port_type = get_port_type(host)
    if port_type == PortType.DFU:
        return True
    return port_type == PortType.SERIAL and _flashes_with_dfu_util()


# The STM32 ROM bootloader, unless the board's dfu-util runner arguments name another
_ROM_DFU_USB_ID = "0483:df11"
_DFU_UDEV_HINT = (
    "Add a udev rule that gives your user access, for example: "
    'SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="df11", '
    'GROUP="plugdev", MODE="0660"'
)


def _dfu_usb_id() -> str:
    args = (
        _runners_yaml(CORE.relative_build_path(".west_build")).get("args") or {}
    ).get(_DFU_UTIL_RUNNER) or []
    for index, arg in enumerate(args):
        if arg.startswith("--pid="):
            return arg.split("=", 1)[1]
        if arg == "--pid" and index + 1 < len(args):
            return args[index + 1]
    return _ROM_DFU_USB_ID


def upload_options() -> list[tuple[str, str]]:
    """The device chooser's DFU entry, when the build flashes with dfu-util and its
    device is attached."""
    if not _flashes_with_dfu_util() or (dfu_util := shutil.which("dfu-util")) is None:
        return []
    usb_id = _dfu_usb_id()
    try:
        result = subprocess.run(
            [dfu_util, "-l", "-d", usb_id],
            capture_output=True,
            text=True,
            check=False,
            timeout=10,
        )
    except (OSError, subprocess.TimeoutExpired):
        return []
    output = result.stdout + result.stderr
    if f"Found DFU: [{usb_id}]" in output:
        return [(f"DFU bootloader ({usb_id}, via dfu-util)", "DFU")]
    if f"Cannot open DFU device {usb_id}" in output:
        _LOGGER.warning(
            "A DFU device (%s) was found but cannot be opened. %s",
            usb_id,
            _DFU_UDEV_HINT,
        )
    return []


def check_dfu_util_runner(zephyr_base: Path, build_dir: Path) -> None:
    """Refuse an MCUboot build when Zephyr's dfu-util runner would start MCUboot before
    the application is written: it needs --no-reset support and the STM32 policy that
    resets only after the last image."""
    if not (build_dir / "mcuboot").is_dir():
        return
    runner = zephyr_base / "scripts" / "west_commands" / "runners" / "dfu.py"
    soc_yml = zephyr_base / "soc" / "st" / "stm32" / "soc.yml"
    try:
        fixed = "reset=True" in runner.read_text(
            encoding="utf-8"
        ) and _DFU_UTIL_RUNNER in soc_yml.read_text(encoding="utf-8")
    except OSError:
        fixed = False
    if not fixed:
        raise EsphomeError(
            "This Zephyr's dfu-util runner cannot flash an MCUboot build: it starts "
            "MCUboot before the application is written "
            "(https://github.com/zephyrproject-rtos/zephyr/pull/126198). Use a Zephyr "
            "with that change in 'framework: source:', or flash with another runner."
        )
