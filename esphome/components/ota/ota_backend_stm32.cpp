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
const uint32_t FLASH_BANK2_ADDR = FLASH_BASE + FLASH_BANK_SIZE;
#if defined(L4)
const uint32_t BLOCK_MASK = 7;
#define HAL_FLASH_PROGRAM(dest_addr, src_addr) \
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dest_addr, *((uint64_t *) src_addr))
#elif defined(U5)
const uint32_t BLOCK_MASK = 15;
#define HAL_FLASH_PROGRAM(dest_addr, src_addr) \
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, dest_addr, (uint32_t) src_addr)
#else
#error stm32 series not supported
#endif

OTAResponseTypes STM32OTABackend::begin(size_t image_size) {
  uint8_t active_bank = esphome::stm32::get_active_flash_bank();
  // always write to memory address of bank 2 (0x08080000)
  // as HAL_FLASH_Program takes bank swapping into account!
  dest_addr_ = FLASH_BANK2_ADDR;
  image_size_ = image_size;
  md5_.init();
  HAL_FLASH_Unlock();
  ESP_LOGI(TAG, "starting ota, image_size: %u, active_bank: %d", image_size, active_bank);
  return OTA_RESPONSE_OK;
}

void STM32OTABackend::set_update_md5(const char *expected_md5) { memcpy(this->expected_bin_md5_, expected_md5, 32); }

OTAResponseTypes STM32OTABackend::write(uint8_t *data, size_t len) {
  uint8_t active_bank = esphome::stm32::get_active_flash_bank();
  FLASH_EraseInitTypeDef flash_erase = {0};
  flash_erase.TypeErase = FLASH_TYPEERASE_PAGES;
  flash_erase.NbPages = 1;
  flash_erase.Banks = active_bank == FLASH_BANK_1 ? FLASH_BANK_2 : FLASH_BANK_1;
  md5_.add(data, len);
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
        HAL_FLASH_PROGRAM(flash_addr, buffer_);
        // HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, flash_addr, *((uint64_t *) buffer_));
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
    HAL_FLASH_PROGRAM(flash_addr, buffer_);
  }
  HAL_FLASH_Lock();

  this->md5_.calculate();
  if (!this->md5_.equals_hex(this->expected_bin_md5_)) {
    ESP_LOGE(TAG, "invalid md5 sum, aborting");
    this->abort();
    return OTA_RESPONSE_ERROR_MD5_MISMATCH;
  }

  ESP_LOGI(TAG, "successfully uploaded %lu bytes, swap the flash banks", image_size_);
  ::esphome::stm32::swap_flash_banks();
  return OTA_RESPONSE_OK;
}

void STM32OTABackend::abort() {}

}  // namespace ota
}  // namespace esphome
#else
#error Dual-Bank Flash is not present on this board
#endif  // FLASH_BANK_2
#endif  // USE_STM32
