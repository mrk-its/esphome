#include <cstring>
#include "md5.h"
#ifdef USE_MD5
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::md5 {

#if defined(USE_ARDUINO) && !defined(USE_RP2040) && !defined(USE_ESP32)
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  MD5Init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { MD5Update(&this->ctx_, data, len); }

void MD5Digest::calculate() { MD5Final(this->digest_, &this->ctx_); }
#endif  // USE_ARDUINO && !USE_RP2040

#ifdef USE_ESP32
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  esp_rom_md5_init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { esp_rom_md5_update(&this->ctx_, data, len); }

void MD5Digest::calculate() { esp_rom_md5_final(this->digest_, &this->ctx_); }
#endif  // USE_ESP32

#ifdef USE_RP2040
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  br_md5_init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { br_md5_update(&this->ctx_, data, len); }

void MD5Digest::calculate() { br_md5_out(&this->ctx_, this->digest_); }
#endif  // USE_RP2040

#ifdef USE_STM32
const char *TAG = "md5.stm32";
static bool hash_clock_enabled_ = false;
void MD5Digest::init() {
  if (!hash_clock_enabled_) {
    __HASH_CLK_ENABLE();
    hash_clock_enabled_ = true;
  }
  memset(this->digest_, 0, 16);
  memset(&this->ctx_, 0, sizeof(this->ctx_));
  ctx_.handle.Init.DataType = HASH_DATATYPE_8B;
  if (HAL_HASH_Init(&ctx_.handle) != HAL_OK) {
    ESP_LOGE(TAG, "Cannot initialize hash");
  }
}

void MD5Digest::add(const uint8_t *data, size_t len) {
  if (len == 0) {
    return;
  }

  size_t data_idx = 0;

  // 1. If there is already some data in the context buffer, try to fill it
  if (this->ctx_.buf_len > 0) {
    size_t needed_to_fill = 4 - this->ctx_.buf_len;
    size_t to_copy = std::min(len, needed_to_fill);

    std::memcpy(this->ctx_.buf + this->ctx_.buf_len, data, to_copy);
    this->ctx_.buf_len += to_copy;
    data_idx += to_copy;

    if (this->ctx_.buf_len == 4) {
      // The buffer is full (4 bytes), it can be passed to the HAL
      if (HAL_HASH_MD5_Accmlt(&this->ctx_.handle, this->ctx_.buf, 4) != HAL_OK) {
        ESP_LOGE(TAG, "HAL_HASH_MD5_Accmlt error");
      }
      this->ctx_.buf_len = 0;  // Reset the buffer
    }
  }

  // 2. Process the main data blocks (multiples of 4 bytes)
  size_t remaining_len = len - data_idx;
  size_t process_len = (remaining_len / 4) * 4;
  if (process_len > 0) {
    if (HAL_HASH_MD5_Accmlt(&this->ctx_.handle, data + data_idx, process_len) != HAL_OK) {
      ESP_LOGE(TAG, "HAL_HASH_MD5_Accmlt error");
    }
    data_idx += process_len;
  }

  // 3. Copy the remainder (0 to 3 bytes) into the context buffer
  remaining_len = len - data_idx;
  if (remaining_len > 0) {
    std::memcpy(this->ctx_.buf, data + data_idx, remaining_len);
    this->ctx_.buf_len = remaining_len;
  }
}

void MD5Digest::calculate() {
  // Pass the last data fragment (from the ctx_ buffer) and get the result
  HAL_StatusTypeDef status =
      HAL_HASH_MD5_Accmlt_End(&this->ctx_.handle, this->ctx_.buf, this->ctx_.buf_len, this->digest_, HAL_MAX_DELAY);

  if (status != HAL_OK) {
    ESP_LOGE(TAG, "HAL_HASH_MD5_Accmlt_End error");
  }

  // Reset the buffer length in the context
  this->ctx_.buf_len = 0;
}

#endif

#ifdef USE_HOST
MD5Digest::~MD5Digest() {
  if (this->ctx_) {
    EVP_MD_CTX_free(this->ctx_);
  }
}

void MD5Digest::init() {
  if (this->ctx_) {
    EVP_MD_CTX_free(this->ctx_);
  }
  this->ctx_ = EVP_MD_CTX_new();
  EVP_DigestInit_ex(this->ctx_, EVP_md5(), nullptr);
  this->calculated_ = false;
  memset(this->digest_, 0, 16);
}

void MD5Digest::add(const uint8_t *data, size_t len) {
  if (!this->ctx_) {
    this->init();
  }
  EVP_DigestUpdate(this->ctx_, data, len);
}

void MD5Digest::calculate() {
  if (!this->ctx_) {
    this->init();
  }
  if (!this->calculated_) {
    unsigned int len = 16;
    EVP_DigestFinal_ex(this->ctx_, this->digest_, &len);
    this->calculated_ = true;
  }
}
#else
MD5Digest::~MD5Digest() = default;
#endif  // USE_HOST

}  // namespace esphome::md5

#endif
