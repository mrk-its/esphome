"""STM32-family zephyr_to_code hook.

No pinctrl-specific code yet -- STM32's I2C/UART/SPI pins are a small,
per-signal set of pre-named alternate-function phandles Zephyr already
declares (not free-mux like esp32/nordic/silabs), and remapping them isn't
implemented yet. See platform_zephyr_todo memory for the open item."""

import argparse
from dataclasses import dataclass
import logging
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

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


# Upload through an STM32 ROM bootloader's USB DFU (DfuSe) interface. Zephyr's dfu-util
# runner writes every image with "leave", so the first one starts before the others are
# written, and it fails on dfu-util's error after "leave" (Zephyr issue #126196).

_DFU_UTIL_RUNNER = "dfu-util"
_ROM_DFU_TIMEOUT = 30.0


@dataclass(frozen=True)
class DfuTarget:
    """The DfuSe device to write: USB VID:PID and the alternate setting of its flash."""

    usb_id: str = "0483:df11"  # every STM32 ROM bootloader
    alt: str = "0"


def _runners_yaml(build_dir: Path) -> dict:
    from ..build_zephyr import _find_runners_yaml  # noqa: PLC0415

    try:
        with _find_runners_yaml(build_dir).open(encoding="utf-8") as f:
            return yaml.safe_load(f) or {}
    except (OSError, yaml.YAMLError):
        return {}


def _dfu_target(runners_yaml: dict) -> DfuTarget | None:
    """The board's DfuSe device from its dfu-util runner arguments, or the STM32 ROM
    bootloader when it has none. None for a plain DFU (not DfuSe) setup."""
    args = (runners_yaml.get("args") or {}).get(_DFU_UTIL_RUNNER)
    if args is None:
        return DfuTarget()
    parser = argparse.ArgumentParser(
        add_help=False, allow_abbrev=False, exit_on_error=False
    )
    parser.add_argument("--pid", "-i", "--dev-id", dest="usb_id")
    parser.add_argument("--alt")
    parser.add_argument("--dfuse", action="store_true")
    try:
        parsed, _ = parser.parse_known_args(args)
    except argparse.ArgumentError:
        return None
    if not parsed.dfuse:
        return None
    default = DfuTarget()
    return DfuTarget(parsed.usb_id or default.usb_id, parsed.alt or default.alt)


def uses_dfu(host: str) -> bool:
    """`--device DFU`, or a serial port when the build flashes with dfu-util (the
    advanced: runner:, else the board's default) or the variant asks for DFU."""
    from esphome.components.stm32.const import VARIANT_STM32C5  # noqa: PLC0415
    from esphome.upload_targets import PortType, get_port_type  # noqa: PLC0415

    from .. import zephyr_rtos_data, zephyr_variant  # noqa: PLC0415

    if host == "DFU":
        return True
    if get_port_type(host) != PortType.SERIAL:
        return False
    if zephyr_variant() == VARIANT_STM32C5:
        from . import stm32c5  # noqa: PLC0415

        if stm32c5.prefers_rom_dfu():
            return True
    runners_yaml = _runners_yaml(CORE.relative_build_path(".west_build"))
    runner = zephyr_rtos_data().get(KEY_RUNNER) or runners_yaml.get("flash-runner")
    return runner == _DFU_UTIL_RUNNER and _dfu_target(runners_yaml) is not None


def _hex_start(hex_file: Path) -> int:
    """Address of the first data record of an Intel HEX file."""
    base = 0
    for line in hex_file.read_text(encoding="utf-8").splitlines():
        record = bytes.fromhex(line.strip()[1:])
        offset, kind, data = int.from_bytes(record[1:3]), record[3], record[4:-1]
        if kind == 0x00:
            return base + offset
        if kind == 0x02:
            base = int.from_bytes(data) << 4
        elif kind == 0x04:
            base = int.from_bytes(data) << 16
    raise EsphomeError(f"No data in {hex_file}")


def _dfu_present(dfu_util: str, target: DfuTarget) -> bool:
    result = subprocess.run(
        [dfu_util, "-l", "-d", target.usb_id],
        capture_output=True,
        text=True,
        check=False,
    )
    return f"[{target.usb_id}]" in result.stdout


def _dfu_images(build_dir: Path) -> list[tuple[Path, int]]:
    """Each sysbuild image's binary and the address it is written to, MCUboot first."""
    app = build_dir / "zephyr" / "zephyr"
    if (mcuboot := build_dir / "mcuboot" / "zephyr").is_dir():
        stems = [mcuboot / "zephyr", app / "zephyr.signed"]
    else:
        stems = [app / "zephyr"]
    images = []
    for stem in stems:
        binary, hex_file = (
            stem.with_name(f"{stem.name}{ext}") for ext in (".bin", ".hex")
        )
        if not (binary.is_file() and hex_file.is_file()):
            raise EsphomeError(f"{binary} not found; compile the firmware first")
        # The HEX file holds the image's absolute address, the BIN file does not.
        images.append((binary, _hex_start(hex_file)))
    return images


def upload_dfu() -> bool:
    """Write every image through the DfuSe bootloader, then start the firmware."""
    from ..build_zephyr import check_bootloader_built

    if (dfu_util := shutil.which("dfu-util")) is None:
        raise EsphomeError("dfu-util not found; install it to upload over USB DFU")
    build_dir = CORE.relative_build_path(".west_build")
    check_bootloader_built(build_dir)
    images = _dfu_images(build_dir)
    target = _dfu_target(_runners_yaml(build_dir)) or DfuTarget()
    if not _dfu_present(dfu_util, target):
        _LOGGER.info(
            "Waiting for the DFU bootloader (%s): hold BOOT and press RESET",
            target.usb_id,
        )
        deadline = time.monotonic() + _ROM_DFU_TIMEOUT
        while not _dfu_present(dfu_util, target):
            if time.monotonic() > deadline:
                raise EsphomeError(
                    "The DFU bootloader did not appear. Hold BOOT, press RESET, "
                    "and run the upload again."
                )
            time.sleep(0.5)
    base_cmd = [dfu_util, "-d", target.usb_id, "-a", target.alt]
    *first, (last, last_address) = images
    for binary, address in first:
        _LOGGER.info("Writing %s at %#010x", binary.name, address)
        cmd = [*base_cmd, "-s", f"{address:#010x}", "-D", str(binary)]
        _LOGGER.debug("bootloader cmd: %s", cmd)
        if subprocess.run(cmd, check=False).returncode != 0:
            raise EsphomeError(f"dfu-util failed writing {binary}")
        _dfu_verify(base_cmd, binary, address)
    # ":leave" makes the ROM bootloader start the firmware. It leaves USB at once, so
    # dfu-util cannot read the final status and fails after a complete write.
    _LOGGER.info("Writing %s at %#010x", last.name, last_address)
    cmd = [*base_cmd, "-s", f"{last_address:#010x}:leave", "-D", str(last)]
    _LOGGER.debug("app cmd: %s", cmd)
    result = subprocess.run(cmd, check=False, capture_output=True, text=True)
    _LOGGER.debug("%s", result.stdout)
    if result.returncode != 0 and "Submitting leave request" not in result.stdout:
        _LOGGER.error("%s", result.stderr)
        raise EsphomeError(f"dfu-util failed writing {last}")
    return True


def _dfu_verify(base_cmd: list[str], binary: Path, address: int) -> None:
    """Read an image back: the ROM bootloader reports success on write-protected pages
    it leaves unchanged."""
    expected = binary.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        readback = Path(tmp) / "readback.bin"
        cmd = [*base_cmd, "-s", f"{address:#010x}:{len(expected)}", "-U", str(readback)]
        if subprocess.run(cmd, check=False, capture_output=True).returncode != 0:
            raise EsphomeError(f"dfu-util failed reading back {binary}")
        if readback.read_bytes() != expected:
            raise EsphomeError(
                f"Flash at {address:#010x} does not hold {binary.name} after writing it. "
                "The pages are likely write protected (WRP option bytes), which the ROM "
                "bootloader cannot change; clear the protection first."
            )
