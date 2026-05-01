#ifdef USE_STM32
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "ota_backend.h"
#include "ota_backend_stm32.h"

namespace esphome {
namespace ota {

static const char *const TAG = "ota.stm32";

std::unique_ptr<STM32OTABackend> make_ota_backend() { return make_unique<STM32OTABackend>(); }

OTAResponseTypes STM32OTABackend::begin(size_t image_size) {
#define FLASHDEV DT_NODELABEL(flash)
  this->flash_dev_ = DEVICE_DT_GET_OR_NULL(FLASHDEV);

  // uint8_t active_bank = esphome::stm32::get_active_flash_bank();
  // // always write to memory address of bank 2 (0x08080000)
  // // as HAL_FLASH_Program takes bank swapping into account!
  dest_addr_ = ZEPHYR_FLASH_BANK2_ADDR;
  image_size_ = image_size;
  md5_.init();
  // HAL_FLASH_Unlock();
  ESP_LOGI(TAG, "starting ota, image_size: %u", image_size);
  return OTA_RESPONSE_OK;
}

void STM32OTABackend::set_update_md5(const char *expected_md5) { memcpy(this->expected_bin_md5_, expected_md5, 32); }

OTAResponseTypes STM32OTABackend::write(uint8_t *data, size_t len) {
  // uint8_t active_bank = esphome::stm32::get_active_flash_bank();
  // FLASH_EraseInitTypeDef flash_erase = {0};
  // flash_erase.TypeErase = FLASH_TYPEERASE_PAGES;
  // flash_erase.NbPages = 1;
  // flash_erase.Banks = active_bank == FLASH_BANK_1 ? FLASH_BANK_2 : FLASH_BANK_1;
  md5_.add(data, len);
  while (len--) {
    if (!(this->dest_addr_ & (ZEPHYR_ERASE_BLOCK_SIZE - 1))) {
      if (flash_erase(this->flash_dev_, this->dest_addr_, ZEPHYR_ERASE_BLOCK_SIZE) != 0) {
        ESP_LOGE(TAG, "can't erase block at %x", this->dest_addr_);
      }
    }
    uint8_t offset = dest_addr_ & ZEPHYR_FLASH_BLOCK_MASK;
    if (!offset) {
      if (this->dest_addr_ & ZEPHYR_FLASH_BANK_MASK) {
        // buffer is full
        uint32_t flash_addr = ((dest_addr_ - 1) & ~ZEPHYR_FLASH_BLOCK_MASK);
        if (flash_write(this->flash_dev_, flash_addr, this->buffer_, ZEPHYR_FLASH_BLOCK_SIZE) != 0) {
          ESP_LOGE(TAG, "can't write to flash at %x", flash_addr);
        }
      }
      memset(this->buffer_, 0, ZEPHYR_FLASH_BLOCK_SIZE);
    }
    buffer_[offset] = *data++;
    dest_addr_++;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes STM32OTABackend::end() {
  if (dest_addr_ & ZEPHYR_FLASH_BANK_MASK) {
    uint32_t flash_addr = ((dest_addr_ - 1) & ~ZEPHYR_FLASH_BLOCK_MASK);
    if (flash_write(this->flash_dev_, flash_addr, this->buffer_, ZEPHYR_FLASH_BLOCK_SIZE) != 0) {
      ESP_LOGE(TAG, "can't write to flash at %x", flash_addr);
    }
  }

  this->md5_.calculate();
  if (!this->md5_.equals_hex(this->expected_bin_md5_)) {
    ESP_LOGE(TAG, "invalid md5 sum, aborting");
    this->abort();
    return OTA_RESPONSE_ERROR_MD5_MISMATCH;
  }

  ESP_LOGI(TAG, "successfully uploaded %lu bytes, swap the flash banks", image_size_);
  uint32_t ob = 0;
  int ret = flash_ex_op(this->flash_dev_, FLASH_STM32_EX_OP_OPTB_READ, NULL, &ob);
  if (!ret) {
    uint32_t new_ob = ob ^ (1 << 20);
    ESP_LOGI(TAG, "OB: %08x, new OB: %08x", ob, new_ob);
    ret = flash_ex_op(this->flash_dev_, FLASH_STM32_EX_OP_OPTB_WRITE, (uint32_t) new_ob, NULL);
    ESP_LOGI(TAG, "OB write err: %d", ret);
  } else {
    ESP_LOGE(TAG, "FLASH_STM32_OPTION_BYTES not enabled");
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  return OTA_RESPONSE_OK;
}

void STM32OTABackend::abort() {}

}  // namespace ota
}  // namespace esphome
#endif  // USE_STM32
