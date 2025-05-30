from esphome import config_validation as cv

from ..utils import HALEnum, auto, optional_dict
from .common import (
    AHB_CLK_DIVIDER,
    FLASH_LATENCY,
    OSCILLATOR_HSE,
    OSCILLATOR_HSI,
    OSCILLATOR_HSI48,
    OSCILLATOR_LSE,
    OSCILLATOR_LSI,
    OSCILLATOR_MSI,
    PLL_SOURCE,
    PLL_STATE,
    RCC_HCLK_DIVIDER,
)
from .l4 import SYSCLKSOURCE  # noqa: F401


class PWR_REGULATOR_VOLTAGE_SCALE(HALEnum):
    PWR_REGULATOR_VOLTAGE_SCALE1 = auto()
    PWR_REGULATOR_VOLTAGE_SCALE2 = auto()
    PWR_REGULATOR_VOLTAGE_SCALE3 = auto()
    PWR_REGULATOR_VOLTAGE_SCALE4 = auto()


class PLLR(HALEnum):
    RCC_PLLR_DIV1 = auto()
    RCC_PLLR_DIV2 = auto()
    RCC_PLLR_DIV4 = auto()
    RCC_PLLR_DIV6 = auto()
    RCC_PLLR_DIV8 = auto()
    RCC_PLLR_DIV10 = auto()
    RCC_PLLR_DIV12 = auto()
    RCC_PLLR_DIV14 = auto()
    RCC_PLLR_DIV16 = auto()


class PLLRGE(HALEnum):
    RCC_PLLVCIRANGE_0 = auto()
    RCC_PLLVCIRANGE_1 = auto()

    DEFAULT = RCC_PLLVCIRANGE_1


class PLLMBOOST(HALEnum):
    RCC_PLLMBOOST_DIV1 = auto()
    RCC_PLLMBOOST_DIV2 = auto()
    RCC_PLLMBOOST_DIV4 = auto()
    RCC_PLLMBOOST_DIV6 = auto()
    RCC_PLLMBOOST_DIV8 = auto()
    RCC_PLLMBOOST_DIV10 = auto()
    RCC_PLLMBOOST_DIV12 = auto()
    RCC_PLLMBOOST_DIV14 = auto()
    RCC_PLLMBOOST_DIV16 = auto()

    DEFAULT = RCC_PLLMBOOST_DIV1


PLL = cv.Schema(
    {
        cv.Required("state"): PLL_STATE.cv_enum(),
        cv.Required("source"): PLL_SOURCE.cv_enum(),
        cv.Required("pllm"): cv.int_range(1, 16),
        cv.Required("plln"): cv.int_range(4, 512),
        cv.Required("pllp"): cv.int_range(2, 2),
        cv.Required("pllq"): cv.int_range(2, 2),
        cv.Required("pllr"): cv.int_range(1, 128),
        cv.Required("pllrge"): PLLRGE.cv_enum(),
        cv.Required("pllmboost"): PLLMBOOST.cv_enum(),
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
                cv.Optional("hsi48"): optional_dict(OSCILLATOR_HSI48),
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
                cv.Optional("pclk3", default=True): cv.boolean,
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
    "U5": {
        "control_voltage_scaling": PWR_REGULATOR_VOLTAGE_SCALE.PWR_REGULATOR_VOLTAGE_SCALE1.value,
        "oscillator": {
            "hse": {},
            "pll": {
                "state": PLL_STATE.RCC_PLL_ON.value,
                "source": PLL_SOURCE.RCC_PLLSOURCE_HSE.value,
                "pllm": 1,
                "plln": 10,
                "pllp": 2,
                "pllq": 2,
                "pllr": 1,
                "pllrge": PLLRGE.DEFAULT.value,
                "pllmboost": PLLMBOOST.DEFAULT.value,
            },
        },
        "clock": {
            "sys_clk_source": SYSCLKSOURCE.RCC_SYSCLKSOURCE_PLLCLK.value,
            "flash_latency": FLASH_LATENCY.FLASH_LATENCY_4.value,
        },
    }
}
