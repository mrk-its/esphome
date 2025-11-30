from esphome import config_validation as cv

from ..utils import HALEnum, auto, optional_dict
from .common import (
    AHB_CLK_DIVIDER,
    FLASH_LATENCY,
    OSCILLATOR_HSE,
    OSCILLATOR_HSI,
    OSCILLATOR_LSE,
    OSCILLATOR_LSI,
    OSCILLATOR_MSI,
    PLL_SOURCE,
    PLL_STATE,
    PWR_REGULATOR_VOLTAGE_SCALE,
    RCC_HCLK_DIVIDER,
)


class PLLP(HALEnum):
    RCC_PLLP_DIV2 = auto()
    RCC_PLLP_DIV4 = auto()
    RCC_PLLP_DIV6 = auto()
    RCC_PLLP_DIV8 = auto()


class SYSCLKSOURCE(HALEnum):
    RCC_SYSCLKSOURCE_MSI = auto()
    RCC_SYSCLKSOURCE_HSI = auto()
    RCC_SYSCLKSOURCE_HSE = auto()
    RCC_SYSCLKSOURCE_PLLCLK = auto()


PLL = cv.Schema(
    {
        cv.Required("state"): PLL_STATE.cv_enum(),
        cv.Required("source"): PLL_SOURCE.cv_enum(),
        cv.Required("pllm"): cv.int_range(0, 63),
        cv.Required("plln"): cv.int_range(50, 432),  # for STM32F411xE max is 192
        cv.Required("pllp"): PLLP.cv_enum(),
        cv.Required("pllq"): cv.int_range(2, 15),
        cv.Optional("pllr"): cv.int_range(2, 7),
    }
)

CLOCK_CONFIG = cv.Schema(
    {
        cv.Required("control_voltage_scaling"): PWR_REGULATOR_VOLTAGE_SCALE.cv_enum(),
        cv.Required("oscillator"): cv.Schema(
            {
                cv.Optional("hsi"): optional_dict(OSCILLATOR_HSI),
                cv.Optional("hse"): optional_dict(OSCILLATOR_HSE),
                cv.Optional("lsi"): optional_dict(OSCILLATOR_LSI),
                cv.Optional("lse"): optional_dict(OSCILLATOR_LSE),
                cv.Optional("msi"): optional_dict(OSCILLATOR_MSI),
                cv.Required("pll"): PLL,
            }
        ),
        cv.Required("clock"): cv.Schema(
            {
                cv.Required("flash_latency"): FLASH_LATENCY.cv_enum(),
                cv.Optional("sysclk", default=True): cv.boolean,
                cv.Optional("hclk", default=True): cv.boolean,
                cv.Optional("pclk1", default=True): cv.boolean,
                cv.Optional("pclk2", default=True): cv.boolean,
                cv.Required("sys_clk_source"): SYSCLKSOURCE.cv_enum(),
                cv.Optional(
                    "ahb_clk_divider",
                    default=AHB_CLK_DIVIDER.DEFAULT.value,
                ): AHB_CLK_DIVIDER.cv_enum(),
                cv.Optional(
                    "apb1_clk_divider",
                    default=RCC_HCLK_DIVIDER.DEFAULT.value,
                ): RCC_HCLK_DIVIDER.cv_enum(),
                cv.Optional(
                    "apb2_clk_divider",
                    default=RCC_HCLK_DIVIDER.DEFAULT.value,
                ): RCC_HCLK_DIVIDER.cv_enum(),
            }
        ),
    }
)

CLOCK_DEFAULTS = {
    "F4": {
        "control_voltage_scaling": PWR_REGULATOR_VOLTAGE_SCALE.PWR_REGULATOR_VOLTAGE_SCALE1.value,
        "oscillator": {
            "hsi": {},
            "pll": {
                "state": PLL_STATE.RCC_PLL_ON.value,
                "source": PLL_SOURCE.RCC_PLLSOURCE_HSI.value,
                "pllm": 1,
                "pllp": PLLP.RCC_PLLP_DIV2.value,
                "pllq": 4,
            },
        },
        "clock": {
            "sys_clk_source": SYSCLKSOURCE.RCC_SYSCLKSOURCE_PLLCLK.value,
            "flash_latency": FLASH_LATENCY.FLASH_LATENCY_4.value,
        },
    },
    "84Mhz": {
        "oscillator": {
            "pll": {
                "plln": 84,
            },
        },
        "clock": {
            "flash_latency": FLASH_LATENCY.FLASH_LATENCY_2.value,
        },
    },
    "100Mhz": {
        "oscillator": {
            "pll": {
                "plln": 100,
            },
        },
        "clock": {
            "flash_latency": FLASH_LATENCY.FLASH_LATENCY_3.value,
        },
    },
    "168Mhz": {
        "oscillator": {
            "pll": {
                "plln": 168,
            },
        },
        "clock": {
            "flash_latency": FLASH_LATENCY.FLASH_LATENCY_5.value,
        },
    },
    "180Mhz": {
        "oscillator": {
            "pll": {
                "plln": 180,
            },
        },
        "clock": {
            "flash_latency": FLASH_LATENCY.FLASH_LATENCY_5.value,
        },
    },
}

BOARD_CLOCK_DEFAULTS = {}
