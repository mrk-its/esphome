#pragma once

#ifdef USE_STM32
#include "esphome/core/preference_backend.h"

namespace esphome {
namespace stm32 {

void setup_preferences();

}  // namespace stm32
}  // namespace esphome

DECLARE_PREFERENCE_ALIASES(esphome::Preferences)

#endif
