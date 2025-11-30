#ifdef USE_STM32

#include "preferences.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace stm32 {

static const char *const TAG = "stm32.preferences";

static bool s_prevent_write = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class STM32Preferences : public ESPPreferences {
 public:
  uint32_t current_flash_offset = 0;

  STM32Preferences() {}
  void setup() {}

  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return make_preference(length, type);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override { return {}; }

  bool sync() override { return false; }

  bool reset() override { return false; }
};

void setup_preferences() {
  auto *prefs = new STM32Preferences();  // NOLINT(cppcoreguidelines-owning-memory)
  prefs->setup();
  global_preferences = prefs;
}
void preferences_prevent_write(bool prevent) { s_prevent_write = prevent; }

}  // namespace stm32

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_STM32
