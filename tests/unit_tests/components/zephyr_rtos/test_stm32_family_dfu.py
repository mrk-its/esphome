"""Tests for flashing STM32 boards over DFU with west's dfu-util runner."""

from __future__ import annotations

from pathlib import Path
import subprocess
from types import SimpleNamespace
from unittest.mock import patch

import pytest
import yaml

from esphome.components.stm32.const import VARIANT_STM32C5, VARIANT_STM32F4
from esphome.components.zephyr_rtos import upload_program
from esphome.components.zephyr_rtos.const import BOOTLOADER_MCUBOOT, KEY_ZEPHYR_RTOS
from esphome.components.zephyr_rtos.variants import stm32_family
from esphome.const import (
    CONF_FRAMEWORK,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_PLATFORM,
    PLATFORM_STM32,
)
from esphome.core import CORE, EsphomeError
from esphome.upload_targets import PortType
from tests.unit_tests.components.zephyr_rtos.zephyr_state import empty_zephyr_data

PORT = "/dev/ttyACM0"
BLACKPILL = "blackpill_f411ce"
XIAO = "xiao_stm32c5"


def _state(
    tmp_path: Path,
    variant: str = VARIANT_STM32F4,
    board: str = BLACKPILL,
    runner: str | None = None,
    bootloader: str = "",
) -> None:
    CORE.data[KEY_ZEPHYR_RTOS] = empty_zephyr_data(variant=variant, board=board)
    CORE.data[KEY_ZEPHYR_RTOS]["runner"] = runner
    CORE.data[KEY_ZEPHYR_RTOS]["bootloader"] = bootloader
    CORE.data[KEY_CORE] = {
        KEY_FRAMEWORK_VERSION: "4.5.0",
        KEY_TARGET_PLATFORM: PLATFORM_STM32,
    }
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path


def _write_runners_yaml(tmp_path: Path, flash_runner: str) -> None:
    """runners.yaml of a build without sysbuild domains."""
    path = tmp_path / ".west_build" / "zephyr" / "runners.yaml"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(yaml.safe_dump({"flash-runner": flash_runner}), encoding="utf-8")


@pytest.mark.parametrize(
    ("host", "flash_runner", "runner", "expected"),
    [
        ("DFU", "stm32cubeprogrammer", None, True),
        (PORT, "dfu-util", None, True),  # the board's default, as on the Black Pills
        (PORT, "stm32cubeprogrammer", "dfu-util", True),
        (PORT, "dfu-util", "openocd", False),  # advanced: runner: wins
        (PORT, "stm32cubeprogrammer", None, False),
        ("192.168.1.5", "dfu-util", None, False),
    ],
)
def test_uses_dfu(
    tmp_path: Path, host: str, flash_runner: str, runner: str | None, expected: bool
) -> None:
    _state(tmp_path, runner=runner)
    _write_runners_yaml(tmp_path, flash_runner)
    assert stm32_family.uses_dfu(host) is expected


@pytest.mark.parametrize(
    ("bootloader", "runner", "expected"),
    [
        (BOOTLOADER_MCUBOOT, None, True),
        (BOOTLOADER_MCUBOOT, "dfu-util", True),
        (BOOTLOADER_MCUBOOT, "stm32cubeprogrammer", False),
        ("", None, False),  # TinyUF2 takes the serial port
    ],
)
def test_uses_dfu_on_the_xiao(
    tmp_path: Path, bootloader: str, runner: str | None, expected: bool
) -> None:
    """The XIAO's board files declare no dfu-util runner, so its default never says so."""
    _state(tmp_path, VARIANT_STM32C5, XIAO, runner, bootloader)
    _write_runners_yaml(tmp_path, "stm32cubeprogrammer")
    assert stm32_family.uses_dfu(PORT) is expected


def test_write_dfu_board_root_extends_the_xiao(tmp_path: Path) -> None:
    _state(
        tmp_path, VARIANT_STM32C5, f"{XIAO}/stm32c5a3xx", bootloader=BOOTLOADER_MCUBOOT
    )
    root, changed = stm32_family.write_dfu_board_root()
    assert changed is True
    board_dir = root / "boards" / "esphome" / "xiao_stm32c5_dfu"
    assert yaml.safe_load((board_dir / "board.yml").read_text()) == {
        "board": {"extend": XIAO}
    }
    board_cmake = (board_dir / "board.cmake").read_text()
    assert 'board_runner_args(dfu-util "--pid=0483:df11" "--alt=0" "--dfuse")' in (
        board_cmake
    )
    assert "boards/common/dfu-util.board.cmake" in board_cmake
    # Unchanged files keep the CMake configure
    assert stm32_family.write_dfu_board_root() == (root, False)


def test_write_dfu_board_root_removes_it_when_not_needed(tmp_path: Path) -> None:
    _state(tmp_path, VARIANT_STM32C5, XIAO, bootloader=BOOTLOADER_MCUBOOT)
    root, _ = stm32_family.write_dfu_board_root()
    CORE.data[KEY_ZEPHYR_RTOS]["runner"] = "stm32cubeprogrammer"
    assert stm32_family.write_dfu_board_root() == (None, True)
    assert not root.exists()
    assert stm32_family.write_dfu_board_root() == (None, False)


def _zephyr_base(tmp_path: Path, runner_caps: str, soc_runners: str) -> Path:
    base = tmp_path / "zephyr"
    runner = base / "scripts" / "west_commands" / "runners" / "dfu.py"
    runner.parent.mkdir(parents=True)
    runner.write_text(f"return RunnerCaps(commands={{'flash'}}{runner_caps})\n")
    soc = base / "soc" / "st" / "stm32" / "soc.yml"
    soc.parent.mkdir(parents=True)
    soc.write_text(f"family: []\n{soc_runners}")
    return base


FIXED_SOC = "runners:\n  run_once:\n    '--reset':\n    - runners:\n      - dfu-util\n"


@pytest.mark.parametrize(
    ("runner_caps", "soc_runners", "mcuboot", "fails"),
    [
        (", reset=True", FIXED_SOC, True, False),
        ("", "", True, True),  # stock Zephyr 4.5
        (", reset=True", "", True, True),
        ("", "", False, False),  # a single image is written with 'leave' last anyway
    ],
)
def test_check_dfu_util_runner(
    tmp_path: Path, runner_caps: str, soc_runners: str, mcuboot: bool, fails: bool
) -> None:
    base = _zephyr_base(tmp_path, runner_caps, soc_runners)
    build_dir = tmp_path / ".west_build"
    (build_dir / ("mcuboot" if mcuboot else "zephyr")).mkdir(parents=True)
    if fails:
        with pytest.raises(EsphomeError, match="126198"):
            stm32_family.check_dfu_util_runner(base, build_dir)
    else:
        stm32_family.check_dfu_util_runner(base, build_dir)


@pytest.mark.parametrize("board", [BLACKPILL, XIAO])
def test_upload_program_flashes_with_dfu_util(tmp_path: Path, board: str) -> None:
    if board == XIAO:
        _state(tmp_path, VARIANT_STM32C5, XIAO, bootloader=BOOTLOADER_MCUBOOT)
    else:
        _state(tmp_path)
    _write_runners_yaml(tmp_path, "dfu-util" if board == BLACKPILL else "openocd")
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.components.zephyr_rtos.build_zephyr.check_bootloader_built"),
        patch(
            "esphome.components.zephyr_rtos._upload_west_flash", return_value=True
        ) as mock_flash,
    ):
        assert upload_program({PLATFORM_STM32: {CONF_FRAMEWORK: {}}}, object(), PORT)
    mock_flash.assert_called_once_with(PORT, runner="dfu-util")


def _write_dfu_args(tmp_path: Path, args: list[str]) -> None:
    path = tmp_path / ".west_build" / "zephyr" / "runners.yaml"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        yaml.safe_dump({"flash-runner": "dfu-util", "args": {"dfu-util": args}}),
        encoding="utf-8",
    )


def _dfu_list(output: str) -> SimpleNamespace:
    return SimpleNamespace(returncode=0, stdout=output, stderr="")


@pytest.mark.parametrize(
    ("args", "output", "expected"),
    [
        (
            ["--pid=0483:df11", "--alt=0", "--dfuse"],
            'Found DFU: [0483:df11] ver=0300, devnum=33, cfg=1, intf=0, alt=0, name="@Internal Flash"',
            [("DFU bootloader (0483:df11, via dfu-util)", "DFU")],
        ),
        (
            ["--pid", "1209:db42", "--alt", "0"],
            "Found DFU: [1209:db42] ver=0100, devnum=4, cfg=1, intf=0, alt=0",
            [("DFU bootloader (1209:db42, via dfu-util)", "DFU")],
        ),
        (["--pid=0483:df11", "--alt=0", "--dfuse"], "", []),  # not in DFU mode
    ],
)
def test_upload_options_lists_an_attached_dfu_device(
    tmp_path: Path, args: list[str], output: str, expected: list
) -> None:
    _state(tmp_path)
    _write_dfu_args(tmp_path, args)
    with (
        patch.object(stm32_family.shutil, "which", return_value="dfu-util"),
        patch.object(
            stm32_family.subprocess, "run", return_value=_dfu_list(output)
        ) as mock_run,
    ):
        assert stm32_family.upload_options() == expected
    usb_id = expected[0][0].split("(")[1].split(",")[0] if expected else "0483:df11"
    assert mock_run.call_args.args[0] == ["dfu-util", "-l", "-d", usb_id]


def test_upload_options_warns_when_the_device_cannot_be_opened(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _state(tmp_path)
    _write_dfu_args(tmp_path, ["--pid=0483:df11", "--alt=0", "--dfuse"])
    output = "dfu-util: Cannot open DFU device 0483:df11 found on devnum 33 (LIBUSB_ERROR_ACCESS)"
    with (
        patch.object(stm32_family.shutil, "which", return_value="dfu-util"),
        patch.object(stm32_family.subprocess, "run", return_value=_dfu_list(output)),
    ):
        assert stm32_family.upload_options() == []
    assert "udev rule" in caplog.text


def test_upload_options_needs_dfu_util_and_a_dfu_build(tmp_path: Path) -> None:
    _state(tmp_path)
    _write_runners_yaml(tmp_path, "stm32cubeprogrammer")
    with patch.object(stm32_family.subprocess, "run") as mock_run:
        assert stm32_family.upload_options() == []  # not flashed with dfu-util
        _write_runners_yaml(tmp_path, "dfu-util")
        with patch.object(stm32_family.shutil, "which", return_value=None):
            assert stm32_family.upload_options() == []  # dfu-util not installed
    mock_run.assert_not_called()


def test_upload_options_survives_a_hanging_dfu_util(tmp_path: Path) -> None:
    _state(tmp_path)
    _write_runners_yaml(tmp_path, "dfu-util")
    with (
        patch.object(stm32_family.shutil, "which", return_value="dfu-util"),
        patch.object(
            stm32_family.subprocess,
            "run",
            side_effect=subprocess.TimeoutExpired(["dfu-util"], 10),
        ),
    ):
        assert stm32_family.upload_options() == []
