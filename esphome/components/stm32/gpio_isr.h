#pragma once
#ifdef USE_STM32

extern "C" void stm32_exti_set_handler(uint8_t index, void (*handler)(void *), void *arg);

#endif
