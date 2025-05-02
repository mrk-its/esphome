#ifdef USE_STM32
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/stm32/core.h"
#ifdef FLASH_BANK_2
#include "ota_backend.h"
#include "ota_backend_stm32.h"

namespace esphome {
namespace ota {

static const char *const TAG = "ota.stm32";

std::unique_ptr<ota::OTABackend> make_ota_backend() { return make_unique<ota::STM32OTABackend>(); }

const uint32_t FLASH_BANK_MASK = (FLASH_BANK_SIZE - 1);  // for 512kb BANK
const uint32_t BLOCK_MASK = 7;

OTAResponseTypes STM32OTABackend::begin(size_t image_size) {
  uint8_t active_bank = esphome::stm32::get_active_flash_bank();
  // always write to memory address of bank 2 (0x08080000)
  // as HAL_FLASH_Program takes bank swapping into account!
  dest_addr_ = FLASH_BANK2_END & ~FLASH_BANK_MASK;
  image_size_ = image_size;
  HAL_FLASH_Unlock();
  ESP_LOGI(TAG, "starting ota, image_size: %u, active_bank: %d", image_size, active_bank);
  return OTA_RESPONSE_OK;
}

void STM32OTABackend::set_update_md5(const char *md5) {}

OTAResponseTypes STM32OTABackend::write(uint8_t *data, size_t len) {
  uint8_t active_bank = esphome::stm32::get_active_flash_bank();
  FLASH_EraseInitTypeDef flash_erase = {0};
  flash_erase.TypeErase = FLASH_TYPEERASE_PAGES;
  flash_erase.NbPages = 1;
  flash_erase.Banks = active_bank == FLASH_BANK_1 ? FLASH_BANK_2 : FLASH_BANK_1;
  while (len--) {
    if (!(dest_addr_ & (FLASH_PAGE_SIZE - 1))) {
      // clear flash page at dest_addr
      flash_erase.Page = (dest_addr_ & FLASH_BANK_MASK) / FLASH_PAGE_SIZE;
      ESP_LOGD(TAG, "erasing page: %lu, bank: %lu", flash_erase.Page, flash_erase.Banks);
      uint32_t error_status;
      HAL_FLASHEx_Erase(&flash_erase, &error_status);
    }
    uint8_t offset = dest_addr_ & BLOCK_MASK;
    if (!offset) {
      if (dest_addr_ & FLASH_BANK_MASK) {
        // buffer is full
        uint32_t flash_addr = ((dest_addr_ - 1) & ~BLOCK_MASK);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, flash_addr, *((uint64_t *) buffer_));
      }
      *((uint64_t *) buffer_) = 0;
    }
    buffer_[offset] = *data++;
    dest_addr_++;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes STM32OTABackend::end() {
  if (dest_addr_ & FLASH_BANK_MASK) {
    uint32_t flash_addr = ((dest_addr_ - 1) & ~BLOCK_MASK);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, flash_addr, *((uint64_t *) buffer_));
  }
  uint32_t uploaded_size = (dest_addr_ & FLASH_BANK_MASK);
  // TODO: verify md5
  bool is_upload_ok = (uploaded_size == image_size_);
  ESP_LOGI(TAG, "uploaded: %lu, is_ok: %d", uploaded_size, is_upload_ok);
  if (is_upload_ok) {
    ESP_LOGI(TAG, "successfully uploaded %lu bytes, swapping the flash banks", uploaded_size);
    ::esphome::stm32::swap_flash_banks();
  } else {
    ESP_LOGE(TAG, "error: uploaded %lu (of %lu) bytes", uploaded_size, image_size_);
  }

  HAL_FLASH_Lock();
  return OTA_RESPONSE_OK;
}

void STM32OTABackend::abort() {}

}  // namespace ota
}  // namespace esphome
#else
#error Dual-Bank Flash is not present on this board
#endif  // FLASH_BANK_2
#endif  // USE_STM32
