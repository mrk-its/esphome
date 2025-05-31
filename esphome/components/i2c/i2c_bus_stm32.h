#pragma once

#ifdef USE_STM32

#include "i2c_bus.h"
#include "esphome/core/component.h"
#include "esphome/components/stm32/core.h"
#include "esphome/components/stm32/gpio.h"

namespace esphome {
namespace i2c {

enum RecoveryCode {
  RECOVERY_FAILED_SCL_LOW,
  RECOVERY_FAILED_SDA_LOW,
  RECOVERY_COMPLETED,
};

class STM32I2CBus : public I2CBus, public Component {
 public:
  STM32I2CBus() : hi2c1_{0} {}
  void setup() override;
  void dump_config() override;
  ErrorCode readv(uint8_t address, ReadBuffer *buffers, size_t cnt) override;
  ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t cnt, bool stop) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { scan_ = scan; }
  void set_sda_pin(uint8_t sda_pin) { sda_pin_.set_pin(sda_pin); }
  void set_scl_pin(uint8_t scl_pin) { scl_pin_.set_pin(scl_pin); }
  void set_frequency(uint32_t frequency) {}
  void set_timeout(uint32_t timeout) { timeout_ = timeout; }

 private:
  void recover_();
  RecoveryCode recovery_result_;

 protected:
  I2C_HandleTypeDef hi2c1_;
  stm32::STM32GPIOPin sda_pin_;
  stm32::STM32GPIOPin scl_pin_;
  uint32_t timeout_ = 0;
  bool initialized_ = false;
};

}  // namespace i2c
}  // namespace esphome

#endif  // USE_STM32