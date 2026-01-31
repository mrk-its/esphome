import argparse
import logging
import os
import tempfile
import sys

from pathlib import Path
from typing import Iterable
from platformio.proc import exec_command

from esphome.const import KEY_CORE
from esphome.core import CORE
import esphome.config_validation as cv


_LOGGER = logging.getLogger(__name__)


class BaseDeviceTreeParser:
    def __init__(self, zephyr_path: Path, gcc_path: Path, extra_board_roots: Iterable[Path] = ()):
        self.zephyr_path = zephyr_path
        self.gcc_path = gcc_path
        self.extra_board_roots = extra_board_roots
        self._install_python_deps()

    def _install_python_deps(self):
        try:
            __import__('devicetree')
        except ImportError:
            result = exec_command([sys.executable, "-m", "pip", "install", "-e", self.zephyr_path / "scripts" / "dts" / "python-devicetree"])
            assert not result['returncode'], result

        path = self.zephyr_path / "scripts"
        if path not in sys.path:
            sys.path.insert(0, str(path))

        # silent too verbose log messages from pykwalify.core
        logging.getLogger("pykwalify.core").setLevel(logging.WARN)

    def _process_dts(self, dts_path):
        INCLUDE_DIRS = ["include", "dts", "dts/arm", "dts/common", "_pio/modules/hal/stm32/dts"]
        result = exec_command([
            self.gcc_path,
            "-E", "-nostdinc", "-undef", "-x", "assembler-with-cpp", "-D__DTS__", "-P",
            *(f"-I{self.zephyr_path / dir}" for dir in INCLUDE_DIRS if (self.zephyr_path / dir).is_dir()),
            dts_path,
        ])
        if result['returncode']:
            raise ValueError(f"non-zero return code: {result['returncode']}")

        _, tmp_dts = tempfile.mkstemp(".dts")
        with open(tmp_dts, 'w') as f:
            f.write(result['out'])

        from devicetree.dtlib import DT
        class CustomDT(DT):
            def _remove_unreferenced(self):
                pass

        dt = CustomDT(tmp_dts, include_path=[self.zephyr_path / dir for dir in INCLUDE_DIRS if (self.zephyr_path / dir).is_dir()])
        os.remove(tmp_dts)
        return dt

    def get_zephyr_board(self, board_name):
        import list_boards
        list_args = argparse.Namespace(
            arch_roots=[],
            soc_roots=[self.zephyr_path],
            board_roots=[self.zephyr_path, *self.extra_board_roots],
            board=board_name,
            board_dir=[],
        )
        return list_boards.find_v2_boards(list_args).get(board_name)

    def list_boards(self):
        import list_boards as lb
        list_args = argparse.Namespace(
            arch_roots=[],
            soc_roots=[self.zephyr_path],
            board_roots=[self.zephyr_path],
            board=None,
            board_dir=[],
        )
        return lb.find_v2_boards(list_args).values()

    def get_board_dt(self, board_name):
        board = self.get_zephyr_board(board_name)
        assert board, f"board {board_name} not found"
        dts_path = board.dir / f"{board.name}.dts"
        if not dts_path.is_file():
            raise ValueError("can't find dts file")
        dt = self._process_dts(dts_path)
        import sys
        if 'config' in sys.argv:
            _LOGGER.info("%s", dt)
        return dt


def is_compatible(node, values_set):
    if not node:
        return False
    compatible = node.props.get('compatible')
    if not compatible:
        return False
    return not values_set.isdisjoint(compatible.to_strings())


def validate_zephyr_device_name(device_name, bindings, pref_label_predicate=None):
    dt = CORE.data[KEY_CORE]['devicetree']
    if device_name:
        node = dt.label2node.get(device_name)
        if not is_compatible(node, bindings):
            raise cv.Invalid(f"Invalid device name {device_name}")
    else:
        device_names = sorted([
            label
            for node in dt.node_iter()
            if is_compatible(node, bindings)
            for label in node.labels
        ], key=(lambda label: (pref_label_predicate and not pref_label_predicate(label), label)))

        if not device_names:
            raise cv.Invalid("devicetree: cannot find i2c device")
        device_name = device_names[0]
    return device_name
