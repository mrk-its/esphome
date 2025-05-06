from textwrap import dedent

from esphome import codegen as cg, config_validation as cv
from esphome.const import CONF_BOARD

from ..const import CONF_BOARD_SERIES, CONF_CLOCK
from . import f1, g0, g4, l4

SYSCLKSOURCE = cv.one_of(
    "RCC_SYSCLKSOURCE_MSI",
    "RCC_SYSCLKSOURCE_HSI",
    "RCC_SYSCLKSOURCE_HSE",
    "RCC_SYSCLKSOURCE_PLLCLK",
)

FAMILY_CLOCK_CONFIGS = {
    "F1": f1.CLOCK_CONFIG,
    "L4": l4.CLOCK_CONFIG,
    "G0": g0.CLOCK_CONFIG,
    "G4": g4.CLOCK_CONFIG,
}

CLOCK_DEFAULTS = {
    "F1": f1.CLOCK_DEFAULTS,
    "L4": l4.CLOCK_DEFAULTS,
    "G0": g0.CLOCK_DEFAULTS,
    "G4": g4.CLOCK_DEFAULTS,
}


def _generate_clock_config(config):
    cg.add(
        cg.RawStatement(
            dedent("""
                RCC_OscInitTypeDef RCC_OscInitStruct = {0};
                RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
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
    if "mul" in pll:
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.PLL.PLLMUL = {pll['mul']};"))
    if "pllm" in pll:
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.PLL.PLLM = {pll['pllm']};"))
    if "plln" in pll:
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.PLL.PLLN = {pll['plln']};"))
    if "pllp" in pll:
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.PLL.PLLP = {pll['pllp']};"))
    if "pllq" in pll:
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.PLL.PLLQ = {pll['pllq']};"))
    if "pllr" in pll:
        cg.add(cg.RawStatement(f"RCC_OscInitStruct.PLL.PLLR = {pll['pllr']};"))

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

    clock_types = []
    for clk_name in ("sysclk", "hclk", "pclk1", "pclk2"):
        if clk_name in clock:
            clock_types.append(f"RCC_CLOCKTYPE_{clk_name.upper()}")

    if clock_types:
        cg.add(
            cg.RawStatement(
                f"""RCC_ClkInitStruct.ClockType = {" | ".join(clock_types)};"""
            )
        )
    cg.add(
        cg.RawStatement(
            dedent(f"""
                RCC_ClkInitStruct.SYSCLKSource = {clock["sys_clk_source"]};
                RCC_ClkInitStruct.AHBCLKDivider = {clock["ahb_clk_divider"]};
                RCC_ClkInitStruct.APB1CLKDivider = {clock["apb1_clk_divider"]};
            """)
        )
    )
    if "apb2_clk_divider" in clock:
        cg.add(
            cg.RawStatement(
                dedent(f"""
                    RCC_ClkInitStruct.APB2CLKDivider = {clock["apb2_clk_divider"]};
                """)
            )
        )
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
    "L4": _generate_clock_config,
    "G4": _generate_clock_config,
    "G0": _generate_clock_config,
}


def board_clock_config(value):
    if CONF_CLOCK not in value:
        return value
    board = value[CONF_BOARD]
    board_family = value[CONF_BOARD_SERIES]
    clock_config = FAMILY_CLOCK_CONFIGS.get(board_family)
    if not clock_config:
        raise cv.Invalid(f"Can't find clock config for '{board_family}' board family")

    board_defaults = CLOCK_DEFAULTS.get(board) or CLOCK_DEFAULTS.get(board_family)
    if board_defaults is None:
        raise cv.Invalid(
            f"can't find defaults for '{board}' / '{board_family}' board family"
        )

    value[CONF_CLOCK] = clock_config(value[CONF_CLOCK] or board_defaults)
    return value


def generate_clock_config(config):
    if CONF_CLOCK not in config:
        return
    board_family = config[CONF_BOARD_SERIES]
    config_generator = CONFIG_GENERATORS[board_family]
    config_generator(config)
