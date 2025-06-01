#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/one_wire/one_wire.h"

namespace esphome {
namespace ds2484 {

class DS2484OneWireBus : public Component, public one_wire::OneWireBus, public i2c::I2CDevice {
 public:
  virtual bool reset() override;
  virtual void write8(uint8_t) override;
  virtual void write64(uint64_t) override;
  virtual uint8_t read8() override;
  virtual uint64_t read64() override;
  virtual void reset_search() override;

 protected:
  bool one_wire_triple(bool *branch, bool *id_bit, bool *cmp_id_bit);
  void reset_search() override;
  uint64_t search_int() override;

  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint64_t address_;
};
}  // namespace ds2484
}  // namespace esphome
