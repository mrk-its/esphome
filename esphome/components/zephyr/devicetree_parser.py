import argparse
import logging
import tempfile
import sys
from pathlib import Path
from platformio.proc import exec_command


class BaseDeviceTreeParser:
    def __init__(self, zephyr_path: Path, gcc_path: Path):
        self.zephyr_path = zephyr_path
        self.gcc_path = gcc_path
        self._install_python_deps()

    def _install_python_deps(self):
        try:
            __import__('devicetree')
        except ImportError:
            result = exec_command([sys.executable, "-m", "pip", "install", self.zephyr_path / "scripts" / "dts" / "python-devicetree"])
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
        return DT(tmp_dts)

    def get_zephyr_board(self, board_name):
        import list_boards
        list_args = argparse.Namespace(
            arch_roots=[],
            soc_roots=[self.zephyr_path],
            board_roots=[self.zephyr_path],
            board=board_name,
            board_dir=[],
        )
        return list_boards.find_v2_boards(list_args).get(board_name)


    def get_board_dt(self, board_name):
        board = self.get_zephyr_board(board_name)
        print("board:", board)
        dts_path = board.dir / f"{board.name}.dts"
        if not dts_path.is_file():
            raise ValueError("can't find dts file")
        return self._process_dts(dts_path)

