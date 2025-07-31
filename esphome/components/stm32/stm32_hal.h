#pragma once
#ifdef USE_STM32

#if F0
#include "stm32f0xx_hal.h"
#elif F1
#include "stm32f1xx_hal.h"
#elif F2
#include "stm32f2xx_hal.h"
#elif F3
#include "stm32f3xx_hal.h"
#elif F4
#include "stm32f4xx_hal.h"
#elif F7
#include "stm32f7xx_hal.h"
#elif H7
#include "stm32h7xx_hal.h"
#elif G0
#include "stm32g0xx_hal.h"
#elif G4
#include "stm32g4xx_hal.h"
#elif H5
#include "stm32h5xx_hal.h"
#elif H7
#include "stm32h7xx_hal.h"
#elif L0
#include "stm32l0xx_hal.h"
#elif L1
#include "stm32l1xx_hal.h"
#elif L4
#include "stm32l4xx_hal.h"
#elif L5
#include "stm32l5xx_hal.h"
#elif U3
#include "stm32u3xx_hal.h"
#elif U5
#include "stm32u5xx_hal.h"
#else
#error "Unsupported STM32 Family"
#endif

#endif
