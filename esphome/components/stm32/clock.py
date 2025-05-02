from textwrap import dedent

from esphome import codegen as cg, config_validation as cv
from esphome.const import CONF_BOARD

from .const import CONF_BOARD_FAMILY, CONF_CLOCK


def optional_dict(schema, default_value=None):
    return lambda value: schema(value or default_value or {})


HSI_STATE = cv.one_of(
    "RCC_HSI_OFF",
    "RCC_HSI_ON",
)
HSI_CALIBRATION_VALUE = cv.int_range(0, 31)
HSI_DIV = cv.one_of(*(f"RCC_HSI_DIV{1 << i}" for i in range(8)))

HSE_STATE = cv.one_of(
    "RCC_HSE_OFF",
    "RCC_HSE_ON",
    "RCC_HSE_BYPASS",
)

LSI_STATE = cv.one_of(
    "RCC_LSI_OFF",
    "RCC_LSI_ON",
)

LSE_STATE = cv.one_of(
    "RCC_LSE_OFF",
    "RCC_LSE_ON",
    "RCC_LSE_BYPASS",
)


MSI_STATE = cv.one_of(
    "RCC_MSI_OFF",
    "RCC_MSI_ON",
)
MSI_CALIBRATION_VALUE = cv.int_range(0x00, 0xFF)
MSI_CLOCK_RANGE = cv.one_of(*(f"RCC_MSIRANGE_{i}" for i in range(12)))
HSI48_STATE = cv.one_of("RCC_HSI48_OFF", "RCC_HSI48_ON")

PLLP = cv.one_of(*(f"RCC_PLLP_DIV{k}" for k in range(2, 32)))
PLLQ = cv.one_of("RCC_PLLQ_DIV2", "RCC_PLLQ_DIV4", "RCC_PLLQ_DIV6", "RCC_PLLQ_DIV8")
PLLR = cv.one_of("RCC_PLLR_DIV2", "RCC_PLLR_DIV4", "RCC_PLLR_DIV6", "RCC_PLLR_DIV8")
PLL = cv.Schema(
    {
        cv.Required("state"): cv.one_of("RCC_PLL_NONE", "RCC_PLL_OFF", "RCC_PLL_ON"),
        cv.Required("source"): cv.one_of(
            "RCC_PLLSOURCE_NONE",
            "RCC_PLLSOURCE_MSI",
            "RCC_PLLSOURCE_HSI",
            "RCC_PLLSOURCE_HSE",
        ),
        cv.Required("pllm"): cv.int_,
        cv.Required("plln"): cv.int_,
        cv.Required("pllp"): PLLP,
        cv.Required("pllq"): PLLQ,
        cv.Required("pllr"): PLLR,
    }
)

OSCILLATOR_HSI = cv.Schema(
    {
        cv.Optional("state", default="RCC_HSI_ON"): HSI_STATE,
        cv.Optional("calibration_value", default="RCC_HSICALIBRATION_DEFAULT"): cv.Any(
            HSI_CALIBRATION_VALUE, cv.one_of("RCC_HSICALIBRATION_DEFAULT")
        ),
    }
)

OSCILLATOR_HSI_G0 = cv.Schema(
    {
        cv.Optional("state", default="RCC_HSI_ON"): HSI_STATE,
        cv.Optional("calibration_value", default="RCC_HSICALIBRATION_DEFAULT"): cv.Any(
            HSI_CALIBRATION_VALUE, cv.one_of("RCC_HSICALIBRATION_DEFAULT")
        ),
        cv.Optional("div", default="RCC_HSI_DIV1"): HSI_DIV,
    }
)

OSCILLATOR_HSE = cv.Schema(
    {
        cv.Optional("state", default="RCC_HSE_ON"): HSE_STATE,
    }
)

OSCILLATOR_LSI = cv.Schema(
    {
        cv.Optional("state", default="RCC_LSI_ON"): LSI_STATE,
    }
)

OSCILLATOR_LSE = cv.Schema(
    {
        cv.Optional("state", default="RCC_LSE_ON"): LSE_STATE,
    }
)

OSCILLATOR_MSI = cv.Schema(
    {
        cv.Optional("state", default="RCC_MSI_ON"): MSI_STATE,
        cv.Optional(
            "calibration_value", default="RCC_MSICALIBRATION_DEFAULT"
        ): MSI_CALIBRATION_VALUE,
        cv.Required("clock_range"): MSI_CLOCK_RANGE,
    }
)

OSCILLATOR_HSI48 = cv.Schema(
    {
        cv.Optional("state", default="RCC_HSI48_ON"): HSI48_STATE,
    }
)

SYSCLKSOURCE = cv.one_of(
    "RCC_SYSCLKSOURCE_MSI",
    "RCC_SYSCLKSOURCE_HSI",
    "RCC_SYSCLKSOURCE_HSE",
    "RCC_SYSCLKSOURCE_PLLCLK",
)
AHB_CLK_DIVIDER = cv.one_of(
    "RCC_SYSCLK_DIV1",
    "RCC_SYSCLK_DIV2",
    "RCC_SYSCLK_DIV4",
    "RCC_SYSCLK_DIV8",
    "RCC_SYSCLK_DIV16",
    "RCC_SYSCLK_DIV32",
    "RCC_SYSCLK_DIV64",
    "RCC_SYSCLK_DIV128",
    "RCC_SYSCLK_DIV256",
    "RCC_SYSCLK_DIV512",
)
RCC_HCLK_DIVIDER = cv.one_of(*(f"RCC_HCLK_DIV{1 << i}" for i in range(5)))
FLASH_LATENCY = cv.one_of(*(f"FLASH_LATENCY_{i}" for i in range(16)))

L4_CLOCK_CONFIG = cv.Schema(
    {
        cv.Required("control_voltage_scaling"): cv.one_of(
            "PWR_REGULATOR_VOLTAGE_SCALE1_BOOST",
            "PWR_REGULATOR_VOLTAGE_SCALE1",
            "PWR_REGULATOR_VOLTAGE_SCALE2",
        ),
        # cv.Optional("foo"): cv.Schema({
        #     cv.Optional("bar", default=True): cv.boolean
        # }),
        cv.Required("oscillator"): cv.Schema(
            {
                cv.Optional("hsi"): optional_dict(OSCILLATOR_HSI),
                cv.Optional("hse"): optional_dict(OSCILLATOR_HSE),
                cv.Optional("lsi"): optional_dict(OSCILLATOR_LSI),
                cv.Optional("lse"): optional_dict(OSCILLATOR_LSE),
                cv.Optional("msi"): optional_dict(OSCILLATOR_MSI),
                cv.Optional("hsi48"): optional_dict(OSCILLATOR_HSI48),
                cv.Required("pll"): PLL,
            }
        ),
        cv.Required("clock"): cv.Schema(
            {
                cv.Required("flash_latency"): FLASH_LATENCY,
                cv.Optional("sysclk", default=True): cv.boolean,
                cv.Optional("hclk", default=True): cv.boolean,
                cv.Optional("pclk1", default=True): cv.boolean,
                cv.Optional("pclk2", default=True): cv.boolean,
                cv.Required("sys_clk_source"): SYSCLKSOURCE,
                cv.Optional(
                    "ahb_clk_divider", default="RCC_SYSCLK_DIV1"
                ): AHB_CLK_DIVIDER,
                cv.Optional(
                    "apb1_clk_divider", default="RCC_HCLK_DIV1"
                ): RCC_HCLK_DIVIDER,
                cv.Optional(
                    "apb2_clk_divider", default="RCC_HCLK_DIV1"
                ): RCC_HCLK_DIVIDER,
            }
        ),
    }
)

G0_CLOCK_CONFIG = cv.Schema(
    {
        cv.Required("control_voltage_scaling"): cv.one_of(
            "PWR_REGULATOR_VOLTAGE_SCALE1_BOOST",
            "PWR_REGULATOR_VOLTAGE_SCALE1",
            "PWR_REGULATOR_VOLTAGE_SCALE2",
        ),
        # cv.Optional("foo"): cv.Schema({
        #     cv.Optional("bar", default=True): cv.boolean
        # }),
        cv.Required("oscillator"): cv.Schema(
            {
                cv.Optional("hsi"): optional_dict(OSCILLATOR_HSI_G0),
                cv.Optional("hse"): optional_dict(OSCILLATOR_HSE),
                cv.Optional("lsi"): optional_dict(OSCILLATOR_LSI),
                cv.Optional("lse"): optional_dict(OSCILLATOR_LSE),
                cv.Optional("msi"): optional_dict(OSCILLATOR_MSI),
                cv.Optional("hsi48"): optional_dict(OSCILLATOR_HSI48),
                cv.Required("pll"): PLL,
            }
        ),
        cv.Required("clock"): cv.Schema(
            {
                cv.Required("flash_latency"): FLASH_LATENCY,
                cv.Optional("sysclk", default=True): cv.boolean,
                cv.Optional("hclk", default=True): cv.boolean,
                cv.Optional("pclk1", default=True): cv.boolean,
                cv.Required("sys_clk_source"): SYSCLKSOURCE,
                cv.Optional(
                    "ahb_clk_divider", default="RCC_SYSCLK_DIV1"
                ): AHB_CLK_DIVIDER,
                cv.Optional(
                    "apb1_clk_divider", default="RCC_HCLK_DIV1"
                ): RCC_HCLK_DIVIDER,
            }
        ),
    }
)

FAMILY_CLOCK_CONFIGS = {
    "L4": L4_CLOCK_CONFIG,
    "G4": L4_CLOCK_CONFIG,
    "G0": G0_CLOCK_CONFIG,
}

CLOCK_DEFAULTS = {
    "L4": {
        "control_voltage_scaling": "PWR_REGULATOR_VOLTAGE_SCALE1",
        "oscillator": {
            "hsi": {},
            "pll": {
                "state": "RCC_PLL_ON",
                "source": "RCC_PLLSOURCE_HSI",
                "pllm": 1,
                "plln": 10,
                "pllp": "RCC_PLLP_DIV7",
                "pllq": "RCC_PLLQ_DIV2",
                "pllr": "RCC_PLLR_DIV2",
            },
        },
        "clock": {
            "sys_clk_source": "RCC_SYSCLKSOURCE_PLLCLK",
            "flash_latency": "FLASH_LATENCY_4",
        },
    },
    "G0": {
        "control_voltage_scaling": "PWR_REGULATOR_VOLTAGE_SCALE1",
        "oscillator": {
            "hsi": {},
            "lse": {},
            "pll": {
                "state": "RCC_PLL_ON",
                "source": "RCC_PLLSOURCE_HSI",
                "pllm": 1,
                "plln": 8,
                "pllp": "RCC_PLLP_DIV2",
                "pllq": "RCC_PLLQ_DIV2",
                "pllr": "RCC_PLLR_DIV2",
            },
        },
        "clock": {
            "sys_clk_source": "RCC_SYSCLKSOURCE_PLLCLK",
            "flash_latency": "FLASH_LATENCY_2",
        },
    },
    "G4": {
        "control_voltage_scaling": "PWR_REGULATOR_VOLTAGE_SCALE1_BOOST",
        "oscillator": {
            "hsi": {},
            "pll": {
                "state": "RCC_PLL_ON",
                "source": "RCC_PLLSOURCE_HSI",
                "pllm": 4,
                "plln": 85,
                "pllp": "RCC_PLLP_DIV2",
                "pllq": "RCC_PLLQ_DIV2",
                "pllr": "RCC_PLLR_DIV2",
            },
        },
        "clock": {
            "sys_clk_source": "RCC_SYSCLKSOURCE_PLLCLK",
            "flash_latency": "FLASH_LATENCY_4",
        },
    },
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
                RCC_OscInitStruct.PLL.PLLM = {pll["pllm"]};
                RCC_OscInitStruct.PLL.PLLN = {pll["plln"]};
                RCC_OscInitStruct.PLL.PLLP = {pll["pllp"]};
                RCC_OscInitStruct.PLL.PLLQ = {pll["pllq"]};
                RCC_OscInitStruct.PLL.PLLR = {pll["pllr"]};
            """)
        )
    )
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
    "L4": _generate_clock_config,
    "G4": _generate_clock_config,
    "G0": _generate_clock_config,
}


def board_clock_config(value):
    if CONF_CLOCK not in value:
        return value
    board = value[CONF_BOARD]
    board_family = value[CONF_BOARD_FAMILY]
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
    board_family = config[CONF_BOARD_FAMILY]
    config_generator = CONFIG_GENERATORS[board_family]
    config_generator(config)
