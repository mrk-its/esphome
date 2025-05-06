from esphome import config_validation as cv

from ..utils import HALEnum, auto, optional_dict
from .common import (
    AHB_CLK_DIVIDER,
    FLASH_LATENCY,
    HSI_CALIBRATION_VALUE,
    HSI_STATE,
    OSCILLATOR_HSE,
    OSCILLATOR_HSI48,
    OSCILLATOR_LSE,
    OSCILLATOR_LSI,
    OSCILLATOR_MSI,
    PLL,
    PLL_SOURCE,
    PLL_STATE,
    PLLP,
    PLLQ,
    PLLR,
    PWR_REGULATOR_VOLTAGE_SCALE,
    RCC_HCLK_DIVIDER,
)


class HSI_DIV(HALEnum):
    RCC_HSI_DIV1 = auto()
    RCC_HSI_DIV2 = auto()
    RCC_HSI_DIV4 = auto()
    RCC_HSI_DIV8 = auto()
    RCC_HSI_DIV16 = auto()
    RCC_HSI_DIV32 = auto()
    RCC_HSI_DIV64 = auto()
    RCC_HSI_DIV128 = auto()
    DEFAULT = RCC_HSI_DIV1


class SYSCLKSOURCE(HALEnum):
    RCC_SYSCLKSOURCE_HSI = auto()
    RCC_SYSCLKSOURCE_HSE = auto()
    RCC_SYSCLKSOURCE_PLLCLK = auto()


OSCILLATOR_HSI = cv.Schema(
    {
        cv.Optional("state", default=HSI_STATE.DEFAULT.value): HSI_STATE.cv_enum(),
        cv.Optional("calibration_value", default="RCC_HSICALIBRATION_DEFAULT"): cv.Any(
            HSI_CALIBRATION_VALUE, cv.one_of("RCC_HSICALIBRATION_DEFAULT")
        ),
        cv.Optional("div", default=HSI_DIV.DEFAULT.value): HSI_DIV.cv_enum(),
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
                cv.Required("sys_clk_source"): SYSCLKSOURCE.cv_enum(),
                cv.Optional(
                    "ahb_clk_divider",
                    default=AHB_CLK_DIVIDER.DEFAULT.value,
                ): AHB_CLK_DIVIDER.cv_enum(),
                cv.Optional(
                    "apb1_clk_divider",
                    default=RCC_HCLK_DIVIDER.DEFAULT.value,
                ): RCC_HCLK_DIVIDER.cv_enum(),
            }
        ),
    }
)

CLOCK_DEFAULTS = {
    "G0": {
        "control_voltage_scaling": PWR_REGULATOR_VOLTAGE_SCALE.PWR_REGULATOR_VOLTAGE_SCALE1.value,
        "oscillator": {
            "hsi": {},
            "lse": {},
            "pll": {
                "state": PLL_STATE.RCC_PLL_ON.value,
                "source": PLL_SOURCE.RCC_PLLSOURCE_HSI.value,
                "pllm": 1,
                "plln": 8,
                "pllp": PLLP.RCC_PLLP_DIV2.value,
                "pllq": PLLQ.RCC_PLLQ_DIV2.value,
                "pllr": PLLR.RCC_PLLR_DIV2.value,
            },
        },
        "clock": {
            "sys_clk_source": SYSCLKSOURCE.RCC_SYSCLKSOURCE_PLLCLK.value,
            "flash_latency": FLASH_LATENCY.FLASH_LATENCY_2.value,
        },
    }
}
