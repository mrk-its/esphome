"""STM32-family zephyr_to_code hook.

No pinctrl-specific code yet -- STM32's I2C/UART/SPI pins are a small,
per-signal set of pre-named alternate-function phandles Zephyr already
declares (not free-mux like esp32/nordic/silabs), and remapping them isn't
implemented yet. See platform_zephyr_todo memory for the open item."""

from pathlib import Path

import yaml

from esphome.core import CORE, EsphomeError
from esphome.types import ConfigType

from ..const import KEY_RUNNER, SEGGER_RTT_UART
from . import mcuboot_or_none

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


def uses_dfu(host: str) -> bool:
    """`--device DFU`, or a serial port when the build flashes with dfu-util: the
    advanced: runner:, else the board's default, or a board flashed over DFU anyway."""
    from esphome.upload_targets import PortType, get_port_type  # noqa: PLC0415

    from .. import zephyr_rtos_data  # noqa: PLC0415

    if host == "DFU":
        return True
    if get_port_type(host) != PortType.SERIAL:
        return False
    if _needs_dfu_runner():
        return True
    runner = zephyr_rtos_data().get(KEY_RUNNER)
    if runner is None:
        runner = _runners_yaml(CORE.relative_build_path(".west_build")).get(
            "flash-runner"
        )
    return runner == _DFU_UTIL_RUNNER


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
