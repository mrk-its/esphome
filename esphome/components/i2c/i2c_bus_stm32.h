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
  STM32I2CBus() : i2c_handle_{0} {}
  void setup() override;
  void dump_config() override;
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { scan_ = scan; }
  void set_sda_pin(InternalGPIOPin *sda_pin) { sda_pin_ = sda_pin; }
  void set_scl_pin(InternalGPIOPin *scl_pin) { scl_pin_ = scl_pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_timeout(uint32_t timeout) { timeout_ = timeout; }
  void set_instance(I2C_TypeDef *instance) { i2c_handle_.Instance = instance; }

 private:
  void recover_();
  RecoveryCode recovery_result_;

 protected:
  I2C_HandleTypeDef i2c_handle_;
  InternalGPIOPin *sda_pin_ = 0;
  InternalGPIOPin *scl_pin_ = 0;
  uint32_t timeout_ = 0;
  uint32_t frequency_ = 0;
  bool initialized_ = false;
};

}  // namespace i2c
}  // namespace esphome

#endif  // USE_STM32
