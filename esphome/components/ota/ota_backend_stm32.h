#pragma once
#ifdef USE_STM32
#include "ota_backend.h"
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/flash/stm32_flash_api_extensions.h>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/md5/md5.h"

namespace esphome {
namespace ota {

#define FLASHMEM DT_NODELABEL(flash0)

const uint32_t ZEPHYR_FLASH_SIZE = DT_REG_SIZE(FLASHMEM);
const uint32_t ZEPHYR_FLASH_BANK_SIZE = ZEPHYR_FLASH_SIZE / 2;
const uint32_t ZEPHYR_FLASH_BANK_MASK = (ZEPHYR_FLASH_BANK_SIZE - 1);
const uint32_t ZEPHYR_FLASH_BANK2_ADDR = 0 + ZEPHYR_FLASH_BANK_SIZE;
const uint32_t ZEPHYR_FLASH_BLOCK_SIZE = DT_PROP(FLASHMEM, write_block_size);
const uint32_t ZEPHYR_FLASH_BLOCK_MASK = ZEPHYR_FLASH_BLOCK_SIZE - 1;
const uint32_t ZEPHYR_ERASE_BLOCK_SIZE = DT_PROP(FLASHMEM, erase_block_size);

class STM32OTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;
  bool supports_compression() override { return false; }

 private:
  const device *flash_dev_{};
  uint32_t dest_addr_;
  uint32_t image_size_;
  uint8_t buffer_[ZEPHYR_FLASH_BLOCK_SIZE];
  char expected_bin_md5_[32];
  md5::MD5Digest md5_{};
};

}  // namespace ota
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
