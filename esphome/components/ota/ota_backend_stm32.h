#pragma once
#ifdef USE_STM32
#include "ota_backend.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ota {

class STM32OTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;
  bool supports_compression() override { return false; }

 private:
  uint32_t dest_addr_;
  uint32_t image_size_;
  uint8_t buffer_[8];
};

}  // namespace ota
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
