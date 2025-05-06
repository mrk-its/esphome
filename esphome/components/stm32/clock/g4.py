from .l4 import (  # noqa: F401
    CLOCK_CONFIG,
    FLASH_LATENCY,
    PLL_SOURCE,
    PLL_STATE,
    PLLP,
    PLLQ,
    PLLR,
    PWR_REGULATOR_VOLTAGE_SCALE,
    SYSCLKSOURCE,
)

CLOCK_DEFAULTS = (
    {
        "control_voltage_scaling": PWR_REGULATOR_VOLTAGE_SCALE.PWR_REGULATOR_VOLTAGE_SCALE1_BOOST.value,
        "oscillator": {
            "hsi": {},
            "pll": {
                "state": PLL_STATE.RCC_PLL_ON.value,
                "source": PLL_SOURCE.RCC_PLLSOURCE_HSI.value,
                "pllm": 4,
                "plln": 85,
                "pllp": PLLP.RCC_PLLP_DIV2.value,
                "pllq": PLLQ.RCC_PLLQ_DIV2.value,
                "pllr": PLLR.RCC_PLLR_DIV2.value,
            },
        },
        "clock": {
            "sys_clk_source": SYSCLKSOURCE.RCC_SYSCLKSOURCE_PLLCLK.value,
            "flash_latency": FLASH_LATENCY.FLASH_LATENCY_4.value,
        },
    },
)
