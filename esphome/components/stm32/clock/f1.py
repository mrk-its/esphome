from esphome import config_validation as cv

from ..utils import HALEnum, auto, optional_dict
from .common import (
    AHB_CLK_DIVIDER,
    FLASH_LATENCY,
    HSE_STATE,
    OSCILLATOR_HSI,
    OSCILLATOR_LSE,
    OSCILLATOR_LSI,
    PLL_STATE,
    RCC_HCLK_DIVIDER,
)


class SYSCLKSOURCE(HALEnum):
    RCC_SYSCLKSOURCE_HSI = auto()
    RCC_SYSCLKSOURCE_HSE = auto()
    RCC_SYSCLKSOURCE_PLLCLK = auto()


class PLL_MUL(HALEnum):
    RCC_PLL_MUL4 = auto()
    RCC_PLL_MUL5 = auto()
    RCC_PLL_MUL6 = auto()
    RCC_PLL_MUL7 = auto()
    RCC_PLL_MUL8 = auto()
    RCC_PLL_MUL9 = auto()
    RCC_PLL_MUL6_5 = auto()


class PLL_SOURCE(HALEnum):
    RCC_PLLSOURCE_HSE = auto()
    RCC_PLLSOURCE_HSI_DIV2 = auto()


PLL = cv.Schema(
    {
        cv.Required("state"): PLL_STATE.cv_enum(),
        cv.Required("source"): PLL_SOURCE.cv_enum(),
        cv.Required("mul"): PLL_MUL.cv_enum(),
    }
)


class HSE_PREDIV(HALEnum):
    RCC_HSE_PREDIV_DIV1 = auto()
    RCC_HSE_PREDIV_DIV2 = auto()
    RCC_HSE_PREDIV_DIV3 = auto()
    RCC_HSE_PREDIV_DIV4 = auto()
    RCC_HSE_PREDIV_DIV5 = auto()
    RCC_HSE_PREDIV_DIV6 = auto()
    RCC_HSE_PREDIV_DIV7 = auto()
    RCC_HSE_PREDIV_DIV8 = auto()
    RCC_HSE_PREDIV_DIV9 = auto()
    RCC_HSE_PREDIV_DIV10 = auto()
    RCC_HSE_PREDIV_DIV11 = auto()
    RCC_HSE_PREDIV_DIV12 = auto()
    RCC_HSE_PREDIV_DIV13 = auto()
    RCC_HSE_PREDIV_DIV14 = auto()
    RCC_HSE_PREDIV_DIV15 = auto()
    RCC_HSE_PREDIV_DIV16 = auto()
    DEFAULT = RCC_HSE_PREDIV_DIV1


OSCILLATOR_HSE = cv.Schema(
    {
        cv.Optional("state", default="RCC_HSE_ON"): HSE_STATE.cv_enum(),
        cv.Optional("prediv", default=HSE_PREDIV.DEFAULT.value): HSE_PREDIV.cv_enum(),
    }
)

CLOCK_CONFIG = cv.Schema(
    {
        cv.Required("oscillator"): cv.Schema(
            {
                cv.Optional("hsi"): optional_dict(OSCILLATOR_HSI),
                cv.Optional("hse"): optional_dict(OSCILLATOR_HSE),
                cv.Optional("lsi"): optional_dict(OSCILLATOR_LSI),
                cv.Optional("lse"): optional_dict(OSCILLATOR_LSE),
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
                    "apb1_clk_divider", default=RCC_HCLK_DIVIDER.DEFAULT.value
                ): RCC_HCLK_DIVIDER.cv_enum(),
                cv.Optional(
                    "apb2_clk_divider", default=RCC_HCLK_DIVIDER.DEFAULT.value
                ): RCC_HCLK_DIVIDER.cv_enum(),
            }
        ),
    }
)

CLOCK_DEFAULTS = {
    "oscillator": {
        "hse": {},
        "pll": {
            "state": PLL_STATE.RCC_PLL_ON.value,
            "source": PLL_SOURCE.RCC_PLLSOURCE_HSE.value,
            "mul": PLL_MUL.RCC_PLL_MUL9.value,
        },
    },
    "clock": {
        "sys_clk_source": SYSCLKSOURCE.RCC_SYSCLKSOURCE_PLLCLK.value,
        "flash_latency": FLASH_LATENCY.FLASH_LATENCY_2.value,
        "apb1_clk_divider": RCC_HCLK_DIVIDER.RCC_HCLK_DIV2.value,
    },
}
