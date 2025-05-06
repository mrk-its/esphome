from esphome import config_validation as cv

from ..utils import HALEnum, auto


class FLASH_LATENCY(HALEnum):
    FLASH_LATENCY_0 = auto()
    FLASH_LATENCY_1 = auto()
    FLASH_LATENCY_2 = auto()
    FLASH_LATENCY_3 = auto()
    FLASH_LATENCY_4 = auto()
    FLASH_LATENCY_5 = auto()
    FLASH_LATENCY_6 = auto()
    FLASH_LATENCY_7 = auto()
    FLASH_LATENCY_8 = auto()
    FLASH_LATENCY_9 = auto()
    FLASH_LATENCY_10 = auto()
    FLASH_LATENCY_11 = auto()
    FLASH_LATENCY_12 = auto()
    FLASH_LATENCY_13 = auto()
    FLASH_LATENCY_14 = auto()
    FLASH_LATENCY_15 = auto()


class HSI_STATE(HALEnum):
    RCC_HSI_OFF = auto()
    RCC_HSI_ON = auto()
    DEFAULT = RCC_HSI_ON


HSI_CALIBRATION_VALUE = cv.int_range(0, 31)

OSCILLATOR_HSI = cv.Schema(
    {
        cv.Optional("state", default=HSI_STATE.RCC_HSI_ON.value): HSI_STATE.cv_enum(),
        cv.Optional("calibration_value", default="RCC_HSICALIBRATION_DEFAULT"): cv.Any(
            HSI_CALIBRATION_VALUE, cv.one_of("RCC_HSICALIBRATION_DEFAULT")
        ),
    }
)


class HSI48_STATE(HALEnum):
    RCC_HSI48_OFF = auto()
    RCC_HSI48_ON = auto()


OSCILLATOR_HSI48 = cv.Schema(
    {
        cv.Optional(
            "state", default=HSI48_STATE.RCC_HSI48_ON.value
        ): HSI48_STATE.cv_enum(),
    }
)


class AHB_CLK_DIVIDER(HALEnum):
    RCC_SYSCLK_DIV1 = auto()
    RCC_SYSCLK_DIV2 = auto()
    RCC_SYSCLK_DIV4 = auto()
    RCC_SYSCLK_DIV8 = auto()
    RCC_SYSCLK_DIV16 = auto()
    RCC_SYSCLK_DIV32 = auto()
    RCC_SYSCLK_DIV64 = auto()
    RCC_SYSCLK_DIV128 = auto()
    RCC_SYSCLK_DIV256 = auto()
    RCC_SYSCLK_DIV512 = auto()
    DEFAULT = RCC_SYSCLK_DIV1


class HSE_STATE(HALEnum):
    RCC_HSE_OFF = auto()
    RCC_HSE_ON = auto()
    RCC_HSE_BYPASS = auto()
    DEFAULT = RCC_HSE_ON


class LSI_STATE(HALEnum):
    RCC_LSI_OFF = auto()
    RCC_LSI_ON = auto()
    DEFAULT = RCC_LSI_ON


class LSE_STATE(HALEnum):
    RCC_LSE_OFF = auto()
    RCC_LSE_ON = auto()
    RCC_LSE_BYPASS = auto()
    DEFAULT = RCC_LSE_ON


class MSI_STATE(HALEnum):
    RCC_MSI_OFF = auto()
    RCC_MSI_ON = auto()
    DEFAULT = RCC_MSI_ON


OSCILLATOR_HSE = cv.Schema(
    {
        cv.Optional("state", default=HSE_STATE.DEFAULT.value): HSE_STATE.cv_enum(),
    }
)

OSCILLATOR_LSI = cv.Schema(
    {
        cv.Optional("state", default=LSI_STATE.DEFAULT.value): LSI_STATE.cv_enum(),
    }
)

OSCILLATOR_LSE = cv.Schema(
    {
        cv.Optional("state", default=LSE_STATE.DEFAULT.value): LSE_STATE.cv_enum(),
    }
)

MSI_CALIBRATION_VALUE = cv.int_range(0x00, 0xFF)

MSI_CLOCK_RANGE = cv.one_of(*(f"RCC_MSIRANGE_{i}" for i in range(12)))

OSCILLATOR_MSI = cv.Schema(
    {
        cv.Optional("state", default="RCC_MSI_ON"): MSI_STATE,
        cv.Optional(
            "calibration_value", default="RCC_MSICALIBRATION_DEFAULT"
        ): MSI_CALIBRATION_VALUE,
        cv.Required("clock_range"): MSI_CLOCK_RANGE,
    }
)


class PLL_STATE(HALEnum):
    RCC_PLL_NONE = auto()
    RCC_PLL_OFF = auto()
    RCC_PLL_ON = auto()


class RCC_HCLK_DIVIDER(HALEnum):
    RCC_HCLK_DIV1 = auto()
    RCC_HCLK_DIV2 = auto()
    RCC_HCLK_DIV4 = auto()
    RCC_HCLK_DIV8 = auto()
    RCC_HCLK_DIV16 = auto()
    DEFAULT = RCC_HCLK_DIV1


class PLL_SOURCE(HALEnum):
    RCC_PLLSOURCE_NONE = auto()
    RCC_PLLSOURCE_MSI = auto()
    RCC_PLLSOURCE_HSI = auto()
    RCC_PLLSOURCE_HSE = auto()


class PLLP(HALEnum):
    RCC_PLLP_DIV1 = auto()
    RCC_PLLP_DIV2 = auto()
    RCC_PLLP_DIV3 = auto()
    RCC_PLLP_DIV4 = auto()
    RCC_PLLP_DIV5 = auto()
    RCC_PLLP_DIV6 = auto()
    RCC_PLLP_DIV7 = auto()
    RCC_PLLP_DIV8 = auto()
    RCC_PLLP_DIV9 = auto()
    RCC_PLLP_DIV10 = auto()
    RCC_PLLP_DIV11 = auto()
    RCC_PLLP_DIV12 = auto()
    RCC_PLLP_DIV13 = auto()
    RCC_PLLP_DIV14 = auto()
    RCC_PLLP_DIV15 = auto()
    RCC_PLLP_DIV16 = auto()
    RCC_PLLP_DIV17 = auto()
    RCC_PLLP_DIV18 = auto()
    RCC_PLLP_DIV19 = auto()
    RCC_PLLP_DIV20 = auto()
    RCC_PLLP_DIV21 = auto()
    RCC_PLLP_DIV22 = auto()
    RCC_PLLP_DIV23 = auto()
    RCC_PLLP_DIV24 = auto()
    RCC_PLLP_DIV25 = auto()
    RCC_PLLP_DIV26 = auto()
    RCC_PLLP_DIV27 = auto()
    RCC_PLLP_DIV28 = auto()
    RCC_PLLP_DIV29 = auto()
    RCC_PLLP_DIV30 = auto()
    RCC_PLLP_DIV31 = auto()
    RCC_PLLP_DIV32 = auto()
    DEFAULT = RCC_PLLP_DIV1


class PLLQ(HALEnum):
    RCC_PLLQ_DIV2 = auto()
    RCC_PLLQ_DIV4 = auto()
    RCC_PLLQ_DIV6 = auto()
    RCC_PLLQ_DIV8 = auto()


class PLLR(HALEnum):
    RCC_PLLR_DIV2 = auto()
    RCC_PLLR_DIV4 = auto()
    RCC_PLLR_DIV6 = auto()
    RCC_PLLR_DIV8 = auto()


PLL = cv.Schema(
    {
        cv.Required("state"): PLL_STATE.cv_enum(),
        cv.Required("source"): PLL_SOURCE.cv_enum(),
        cv.Required("pllm"): cv.int_,
        cv.Required("plln"): cv.int_,
        cv.Required("pllp"): PLLP.cv_enum(),
        cv.Required("pllq"): PLLQ.cv_enum(),
        cv.Required("pllr"): PLLR.cv_enum(),
    }
)


class PWR_REGULATOR_VOLTAGE_SCALE(HALEnum):
    PWR_REGULATOR_VOLTAGE_SCALE1_BOOST = auto()
    PWR_REGULATOR_VOLTAGE_SCALE1 = auto()
    PWR_REGULATOR_VOLTAGE_SCALE2 = auto()
