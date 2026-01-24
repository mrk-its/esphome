import argparse
import tempfile
import sys
from pathlib import Path
from platformio.platform.factory import PlatformFactory
from platformio.proc import exec_command


TOOLCHAIN_PKG = "toolchain-gccarmnoneeabi"

class DeviceTreeParser:
    def __init__(self, platform, zephyr_pkg, toolchain_pkg = TOOLCHAIN_PKG):
        self.platform = PlatformFactory.new(platform)
        self.platform._custom_packages = [toolchain_pkg, zephyr_pkg]
        self.platform.install_required_packages()
        self._install_devicetree()

    _zephyr_path = None
    _toolchain_path = None

    @property
    def zephyr_path(self):
        if not self._zephyr_path:
            self._zephyr_path = Path(self.platform.get_package("framework-zephyr").path)
            if (self._zephyr_path / "zephyr").is_dir():
                self._zephyr_path /=  "zephyr"
        return self._zephyr_path

    @property
    def toolchain_path(self):
        if not self._toolchain_path:
            self._toolchain_path = Path(self.platform.get_package("toolchain-gccarmnoneeabi").path)
        return self._toolchain_path

    def _install_devicetree(self):
        try:
            __import__('devicetree')
        except ImportError:
            result = exec_command([sys.executable, "-m", "pip", "install", self.zephyr_path / "scripts" / "dts" / "python-devicetree"])
            assert not result['returncode'], result

        import logging
        logger = logging.getLogger("pykwalify.core")
        logger.setLevel(logging.WARN)



    def _process_dts(self, dts_path):
        INCLUDE_DIRS = ["include", "dts", "dts/arm", "dts/common", "_pio/modules/hal/stm32/dts"]
        result = exec_command([
            self.toolchain_path / "bin" / "arm-none-eabi-gcc",
            "-E", "-nostdinc", "-undef", "-x", "assembler-with-cpp", "-D__DTS__", "-P",
            *(f"-I{self.zephyr_path / dir}" for dir in INCLUDE_DIRS if (self.zephyr_path / dir).is_dir()),
            dts_path,
        ])
        if result['returncode']:
            print(result)
            raise ValueError(f"non-zero return code: {result['returncode']}")
        _, tmp_dts = tempfile.mkstemp(".dts")
        with open(tmp_dts, 'w') as f:
            f.write(result['out'])

        from devicetree.dtlib import DT
        return DT(tmp_dts)

    def get_zephyr_board(self, board_name):
        path = self.zephyr_path / "scripts"
        if path not in sys.path:
            sys.path.insert(0, str(path))

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
        dts_path = board.dir / f"{board.name}.dts"
        if not dts_path.is_file():
            raise ValueError("can't find dts file")
        return self._process_dts(dts_path)

