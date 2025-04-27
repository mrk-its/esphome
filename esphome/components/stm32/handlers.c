#ifdef USE_STM32

void SysTick_Handler(void) { HAL_IncTick(); }

void Error_Handler(void) {
  while (1) {
  }
}

#endif  // USE_STM32