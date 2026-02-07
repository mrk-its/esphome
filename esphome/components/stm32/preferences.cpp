#ifdef USE_STM32
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"

namespace esphome {
namespace stm32 {

static const char *const TAG = "stm32.preferences";

#define ESPHOME_SETTINGS_KEY "esphome"

class STM32PreferenceBackend : public ESPPreferenceBackend {
 public:
  STM32PreferenceBackend(uint32_t type) { this->type_ = type; }
  STM32PreferenceBackend(uint32_t type, std::vector<uint8_t> &&data) : data(std::move(data)) { this->type_ = type; }

  bool save(const uint8_t *data, size_t len) override {
    ESP_LOGVV(TAG, "save key: %u, len: %d", this->type_, len);
    return true;
  }

  bool load(uint8_t *data, size_t len) override { return false; }

  uint32_t get_type() const { return this->type_; }
  std::string get_key() const { return str_sprintf(ESPHOME_SETTINGS_KEY "/%" PRIx32, this->type_); }

  std::vector<uint8_t> data;

 protected:
  uint32_t type_ = 0;
};

class STM32Preferences : public ESPPreferences {
 public:
  void open() {}

  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return make_preference(length, type);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    for (auto *backend : this->backends_) {
      if (backend->get_type() == type) {
        return ESPPreferenceObject(backend);
      }
    }
    printf("type %u size %u\n", type, this->backends_.size());
    auto *pref = new STM32PreferenceBackend(type);  // NOLINT(cppcoreguidelines-owning-memory)
    ESP_LOGD(TAG, "Add new setting %s.", pref->get_key().c_str());
    this->backends_.push_back(pref);
    return ESPPreferenceObject(pref);
  }

  bool sync() override { return false; }

  bool reset() override {
    ESP_LOGD(TAG, "Reset settings");
    return true;
  }

 protected:
  std::vector<STM32PreferenceBackend *> backends_;

  static int export_settings(int (*cb)(const char *name, const void *value, size_t val_len)) { return 0; }
};

void setup_preferences() {
  auto *prefs = new STM32Preferences();  // NOLINT(cppcoreguidelines-owning-memory)
  global_preferences = prefs;
  prefs->open();
}

}  // namespace stm32

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif
