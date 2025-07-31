#ifdef USE_STM32

#include "stm32_hal.h"

IRQn_Type EXTI_IRQ_NR[16] = {
#if defined(F0) || defined(G0) || defined(L0)
    EXTI0_1_IRQn,  EXTI0_1_IRQn,  EXTI2_3_IRQn,  EXTI2_3_IRQn,  EXTI4_15_IRQn, EXTI4_15_IRQn,
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn,
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn,
#elif defined(U5) || defined(L5)
    EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn,  EXTI3_IRQn,  EXTI4_IRQn,  EXTI5_IRQn,  EXTI6_IRQn,  EXTI7_IRQn,
    EXTI8_IRQn, EXTI9_IRQn, EXTI10_IRQn, EXTI11_IRQn, EXTI12_IRQn, EXTI13_IRQn, EXTI14_IRQn, EXTI15_IRQn,
#else
    EXTI0_IRQn,     EXTI1_IRQn,
#if defined(F3)
    EXTI2_TSC_IRQn,
#else
    EXTI2_IRQn,
#endif
    EXTI3_IRQn,     EXTI4_IRQn,     EXTI9_5_IRQn,   EXTI9_5_IRQn,   EXTI9_5_IRQn,   EXTI9_5_IRQn,   EXTI9_5_IRQn,
    EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn,
#endif
};

#if defined(F0) || defined(G0) || defined(L0)
void EXTI0_1_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}
void EXTI2_3_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
}
void EXTI4_15_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_14);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);
}
#else
void EXTI0_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0); }
void EXTI1_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1); }
#if defined(F3)
void EXTI2_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2); }
#else
void EXTI2_TSC_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2); }
#endif
void EXTI3_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3); }
void EXTI4_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4); }

#if defined(U5) || defined(L5)
void EXTI5_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5); }
void EXTI6_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6); }
void EXTI7_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7); }
void EXTI8_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8); }
void EXTI9_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9); }
void EXTI10_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10); }
void EXTI11_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11); }
void EXTI12_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12); }
void EXTI13_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13); }
void EXTI14_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_14); }
void EXTI15_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15); }
#else
void EXTI9_5_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);
}
void EXTI15_10_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_14);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);
}
#endif
#endif

static void (*INT_HANDLER[16])(void *) = {0};
static void *INT_HANDLER_ARG[16] = {0};

void stm32_exti_set_handler(uint8_t index, void (*handler)(void *), void *arg) {
  HAL_NVIC_DisableIRQ(EXTI_IRQ_NR[index]);

  INT_HANDLER[index] = handler;
  INT_HANDLER_ARG[index] = arg;

  HAL_NVIC_SetPriority(EXTI_IRQ_NR[index], 0, 0);
  HAL_NVIC_EnableIRQ(EXTI_IRQ_NR[index]);
}

void HAL_GPIO_EXTI_Callback(uint16_t pin) {
  uint16_t mask = 1;
  for (uint8_t i = 0; i < 16; i++, mask *= 2) {
    if (pin & mask) {
      void (*handler)(void *) = INT_HANDLER[i];
      if (handler) {
        handler(INT_HANDLER_ARG[i]);
      }
      break;
    }
  }
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t pin) { HAL_GPIO_EXTI_Callback(pin); }

void HAL_GPIO_EXTI_Falling_Callback(uint16_t pin) { HAL_GPIO_EXTI_Callback(pin); }

#endif
