#ifdef USE_STM32
#include "i2c_bus_stm32.h"
#include "esphome/core/log.h"

namespace esphome {
namespace i2c {
static const char *const TAG = "i2c.stm32";

void STM32I2CBus::setup() {
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  if (sda_pin_) {
    sda_pin_->setup();
  }

  if (scl_pin_) {
    scl_pin_->setup();
  }

#ifdef I2C1
  if (i2c_handle_.Instance == I2C1) {
#ifdef RCC_PERIPHCLK_I2C1
    PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_SYSCLK;
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
      Error_Handler();
    }
#endif
    __HAL_RCC_I2C1_CLK_ENABLE();
  }
#endif
#ifdef I2C2
  if (i2c_handle_.Instance == I2C2) {
#ifdef RCC_PERIPHCLK_I2C2
    PeriphClkInit.I2c1ClockSelection = RCC_I2C2CLKSOURCE_SYSCLK;
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
      Error_Handler();
    }
#endif
    __HAL_RCC_I2C2_CLK_ENABLE();
  }
#endif
#ifdef I2C3
  if (i2c_handle_.Instance == I2C3) {
#ifdef RCC_PERIPHCLK_I2C2
    PeriphClkInit.I2c1ClockSelection = RCC_I2C3CLKSOURCE_SYSCLK;
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C3;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
      Error_Handler();
    }
#endif
    __HAL_RCC_I2C3_CLK_ENABLE();
  }
#endif

#if defined(F1) || defined(F)
  i2c_handle_.Init.ClockSpeed = this->frequency_;
#else
  // TODO
  // following timing sets ~100kHz SCL on STM32U5 160Mhz sysclk
  i2c_handle_.Init.Timing = 0x30909DEC;

#endif

#ifdef I2C_DUTYCYCLE_2
  i2c_handle_.Init.DutyCycle = I2C_DUTYCYCLE_2;
#endif

  i2c_handle_.Init.OwnAddress1 = 0;
  i2c_handle_.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  i2c_handle_.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  i2c_handle_.Init.OwnAddress2 = 0;
#ifdef I2C_OA2_NOMASK
  i2c_handle_.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
#endif
  i2c_handle_.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  i2c_handle_.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&i2c_handle_) != HAL_OK) {
    Error_Handler();
  }

/** Configure Analogue filter
 */
#ifdef I2C_ANALOGFILTER_ENABLE
  if (HAL_I2CEx_ConfigAnalogFilter(&i2c_handle_, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }
#endif

  /** Configure Digital filter
   */
  // if (HAL_I2CEx_ConfigDigitalFilter(&i2c_handle_, 0) != HAL_OK) {
  //   Error_Handler();
  // }
  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning i2c bus for active devices...");
    this->i2c_scan_();
  }
}

ErrorCode STM32I2CBus::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                   uint8_t *read_buffer, size_t read_count) {
  if (read_count == 0 && write_count == 0) {
    // ESP_LOGV(TAG, "probe address: %02x", address);
    if (HAL_I2C_Master_Transmit(&i2c_handle_, address << 1, nullptr, 0, 20) == HAL_OK) {
      return ERROR_OK;
    }
    return ERROR_NOT_ACKNOWLEDGED;
  }
  // TODO: do read & write in single transaction
  if (write_count) {
    if (HAL_I2C_Master_Transmit(&i2c_handle_, address << 1, (uint8_t *) write_buffer, write_count, 20) != HAL_OK) {
      return ERROR_NOT_ACKNOWLEDGED;
    }
  }
  if (read_count) {
    if (HAL_I2C_Master_Receive(&i2c_handle_, address << 1, (uint8_t *) read_buffer, read_count, 20) != HAL_OK) {
      return ERROR_NOT_ACKNOWLEDGED;
    }
  }
  return ERROR_OK;
}

void STM32I2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  LOG_PIN("  SDA Pin: ", sda_pin_);
  LOG_PIN("  SCL Pin: ", scl_pin_);
  // ESP_LOGCONFIG(TAG, "  Frequency: %" PRIu32 " Hz", this->frequency_);
  if (timeout_ > 0) {
    ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 "us", this->timeout_);
  }
  if (this->scan_) {
    ESP_LOGI(TAG, "Results from i2c bus scan:");
    if (scan_results_.empty()) {
      ESP_LOGI(TAG, "Found no i2c devices!");
    } else {
      for (const auto &s : scan_results_) {
        if (s.second) {
          ESP_LOGI(TAG, "Found i2c device at address 0x%02X", s.first);
        } else {
          ESP_LOGE(TAG, "Unknown error at address 0x%02X", s.first);
        }
      }
    }
  }
}

}  // namespace i2c
}  // namespace esphome

#endif
