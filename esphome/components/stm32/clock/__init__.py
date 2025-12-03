from functools import reduce
import json
import logging
from textwrap import dedent

from esphome import codegen as cg, config_validation as cv
from esphome.config_helpers import merge_config
from esphome.const import CONF_BOARD

from ..const import CONF_CLOCK, CONF_FCPU, CONF_MCU_SERIES
from . import f1, f4, g0, g4, l4, u5

logger = logging.getLogger(__name__)

SYSCLKSOURCE = cv.one_of(
    "RCC_SYSCLKSOURCE_MSI",
    "RCC_SYSCLKSOURCE_HSI",
    "RCC_SYSCLKSOURCE_HSE",
    "RCC_SYSCLKSOURCE_PLLCLK",
)

SERIES_CLOCK_CONFIGS = {
    "F1": f1.CLOCK_CONFIG,
    "F4": f4.CLOCK_CONFIG,
    "L4": l4.CLOCK_CONFIG,
    "G0": g0.CLOCK_CONFIG,
    "G4": g4.CLOCK_CONFIG,
    "U5": u5.CLOCK_CONFIG,
}

CLOCK_DEFAULTS = {
    **f1.CLOCK_DEFAULTS,
    **f4.CLOCK_DEFAULTS,
    **l4.CLOCK_DEFAULTS,
    **g0.CLOCK_DEFAULTS,
    **g4.CLOCK_DEFAULTS,
    **u5.CLOCK_DEFAULTS,
}


def _generate_clock_config(config):
    cg.add(
        cg.RawStatement(
            dedent("""
                RCC_OscInitTypeDef RCC_OscInitStruct = {0};
                RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
                #ifdef __HAL_RCC_PWR_CLK_ENABLE
                __HAL_RCC_PWR_CLK_ENABLE();
                #endif
            """)
        )
    )
    if "control_voltage_scaling" in config["clock"]:
        cg.add(
            cg.RawStatement(
                dedent(f"""
                    if (HAL_PWREx_ControlVoltageScaling({config["clock"]["control_voltage_scaling"]}) != HAL_OK) {{
                        Error_Handler();
                    }};
                """)
            )
        )
    oscillator_types = []

    oscillator = config["clock"]["oscillator"]
    if hsi := oscillator.get("hsi"):
        oscillator_types.append("RCC_OSCILLATORTYPE_HSI")
        cg.add(
            cg.RawStatement(
                dedent(f"""
                    RCC_OscInitStruct.HSIState = {hsi["state"]};
                    RCC_OscInitStruct.HSICalibrationValue = {hsi["calibration_value"]};
                """)
            )
        )
        if "div" in hsi:
            cg.add(
                cg.RawStatement(
                    dedent(f"""
                        RCC_OscInitStruct.HSIDiv = {hsi["div"]};
                    """)
                )
            )

    if hse := oscillator.get("hse"):
        oscillator_types.append("RCC_OSCILLATORTYPE_HSE")
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.HSEState = {hse['state']};"))
        if "prediv" in hse:
            cg.add(
                cg.RawStatement(f"RCC_OscInitStruct.HSEPredivValue = {hse['prediv']};")
            )
    if lsi := oscillator.get("lsi"):
        oscillator_types.append("RCC_OSCILLATORTYPE_LSI")
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.LSIState = {lsi['state']};"))
    if lse := oscillator.get("lse"):
        oscillator_types.append("RCC_OSCILLATORTYPE_LSE")
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.LSEState = {lse['state']};"))
    if msi := oscillator.get("msi"):
        oscillator_types.append("RCC_OSCILLATORTYPE_MSI")
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.MSIState = {msi['state']};"))
        cg.add(
            cg.RawStatement(
                f"RCC_OscInitStruct.MSICalibrationValue = {msi['calibration_value']}"
            )
        )
        cg.add(
            cg.RawStatement(f"RCC_OscInitStruct.MSIClockRange = {msi['clock_range']};")
        )
    if hsi48 := oscillator.get("hsi48"):
        oscillator_types.append("RCC_OSCILLATORTYPE_HSI48")
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.HSI48State = {hsi48['state']};"))

    if oscillator_types:
        cg.add(
            cg.RawStatement(
                f"RCC_OscInitStruct.OscillatorType = {' | '.join(oscillator_types)};"
            )
        )

    pll = oscillator["pll"]
    cg.add(
        cg.RawStatement(
            dedent(f"""
                RCC_OscInitStruct.PLL.PLLState = {pll["state"]};
                RCC_OscInitStruct.PLL.PLLSource = {pll["source"]};
            """)
        )
    )
    for field, key in (
        ("PLLMUL", "mul"),
        ("PLLM", "pllm"),
        ("PLLN", "plln"),
        ("PLLP", "pllp"),
        ("PLLQ", "pllq"),
        ("PLLR", "pllr"),
        ("PLLRGE", "pllrge"),
        ("PLLMBOOST", "pllmboost"),
    ):
        if key not in pll:
            continue
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.PLL.{field} = {pll[key]};"))

    cg.add(
        cg.RawStatement(
            dedent("""
                if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
                    Error_Handler();
                }
            """)
        )
    )
    clock = config["clock"]["clock"]

    clock_types = [
        f"RCC_CLOCKTYPE_{clk_name.upper()}"
        for clk_name in ("sysclk", "hclk", "pclk1", "pclk2", "pclk3")
        if clk_name in clock
    ]

    if clock_types:
        cg.add(
            cg.RawStatement(
                f"""RCC_ClkInitStruct.ClockType = {" | ".join(clock_types)};"""
            )
        )

    for field, key in (
        ("SYSCLKSource", "sys_clk_source"),
        ("AHBCLKDivider", "ahb_clk_divider"),
        ("APB1CLKDivider", "apb1_clk_divider"),
        ("APB2CLKDivider", "apb2_clk_divider"),
        ("APB3CLKDivider", "apb3_clk_divider"),
    ):
        if key not in clock:
            continue
        cg.add(cg.RawStatement(f"RCC_ClkInitStruct.{field} = {clock[key]};"))

    cg.add(
        cg.RawStatement(
            dedent(f"""
                if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, {clock["flash_latency"]}) != HAL_OK) {{
                    Error_Handler();
                }}
            """)
        )
    )


CONFIG_GENERATORS = {
    "F1": _generate_clock_config,
    "F4": _generate_clock_config,
    "L4": _generate_clock_config,
    "G4": _generate_clock_config,
    "G0": _generate_clock_config,
    "U5": _generate_clock_config,
}


def board_clock_config(value):
    if CONF_CLOCK not in value:
        return value
    board = value[CONF_BOARD]
    board_series = value[CONF_MCU_SERIES]
    board_freq = value[CONF_FCPU]
    clock_config = SERIES_CLOCK_CONFIGS.get(board_series)
    if not clock_config:
        raise cv.Invalid(f"Can't find clock config for '{board_series}' board family")

    series_defaults = CLOCK_DEFAULTS.get(board_series) or {}
    freq_defaults = CLOCK_DEFAULTS.get(board_freq) or {}
    board_defaults = CLOCK_DEFAULTS.get(board) or {}
    user_defaults = value[CONF_CLOCK] or {}

    config = reduce(
        merge_config, (series_defaults, freq_defaults, board_defaults, user_defaults)
    )

    value[CONF_CLOCK] = clock_config(config)
    return value


def generate_clock_config(config):
    if CONF_CLOCK not in config:
        return
    logger.debug("clock config: %s", json.dumps(config, indent=2))
    board_family = config[CONF_MCU_SERIES]
    config_generator = CONFIG_GENERATORS[board_family]
    config_generator(config)
