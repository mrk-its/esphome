#include "ds2484.h"

namespace esphome {
namespace ds2484 {
static const char *const TAG = "ds2484.onewire";

bool DS2484OneWireBus::reset() {
  uint8_t buffer[1] = {0xb4};
  return this->write(buffer, 1) == i2c::ERROR_OK;
};

void DS2484OneWireBus::write8(uint8_t value) {
  uint8_t buffer[2] = {0xa5, value};
  this->write(buffer, 2);
};

void DS2484OneWireBus::write64(uint64_t value) {
  for (int i = 0; i < 8; i++) {
    write8((value >> (i * 8)) & 0xff);
  }
}

uint8_t DS2484OneWireBus::read8() {
  uint8_t cmd = 0x96;
  uint8_t response = 0;
  if (this->write(&cmd, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "can't write read8 cmd");
  }
  if (this->read(&response, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "can't read read8 response");
  }
  return response;
}

uint64_t DS2484OneWireBus::read64() {
  uint8_t response = 0;
  for (int i = 0; i < 8; i++) {
    response |= (read8() << (i * 8));
  }
  return response;
}

void DS2484OneWireBus::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->address_ = 0;
}

bool DS2484OneWireBus::one_wire_triple(bool *branch, bool *id_bit, bool *cmp_id_bit) {
  uint8_t buffer[2] = {0x78, branch ? 0x80 : 0};
  uint8_t status;
  if (write(buffer, 2) != i2c::ERROR_OK) {
    return false;
  }
  if (read(&status, 1) != i2c::ERROR_OK) {
    return false;
  }
  *id_bit = bool(status & 0x20);
  *cmp_id_bit = bool(status & 0x40);
  *branch = bool(status & 0x80);
  return true;
}

uint64_t IRAM_ATTR DS2484OneWireBus::search_int() {
  if (this->last_device_flag_)
    return 0u;

  uint8_t last_zero = 0;
  uint64_t bit_mask = 1;
  uint64_t address = this->address_;

  // Initiate search
  for (int bit_number = 1; bit_number <= 64; bit_number++, bit_mask <<= 1) {
    bool branch;

    // compute branch value for the case when there is a discrepancy
    // (there are devices with both 0s and 1s at this bit)
    if (bit_number < this->last_discrepancy_) {
      branch = (address & bit_mask) > 0;
    } else {
      branch = bit_number == this->last_discrepancy_;
    }

    if (!branch) {
      last_zero = bit_number;
    }

    bool id_bit, cmp_id_bit;
    if (!one_wire_triple(&branch, &id_bit, &cmp_id_bit)) {
      return 0;
    }

    if (id_bit && cmp_id_bit) {
      // No devices participating in search
      return 0;
    }

    if (branch) {
      address |= bit_mask;
    } else {
      address &= ~bit_mask;
    }
  }

  this->last_discrepancy_ = last_zero;
  if (this->last_discrepancy_ == 0) {
    // we're at root and have no choices left, so this was the last one.
    this->last_device_flag_ = true;
  }

  this->address_ = address;
  return address;
}

}  // namespace ds2484
}  // namespace esphome
