#pragma once
#ifdef USE_STM32

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace stm32_fdcan {
const char *const TAG = "stm32_fdcan";

template<size_t CAPACITY> class CanFrameCircBuffer {
 public:
  CanFrameCircBuffer(){};
  const static size_t SIZE = CAPACITY + 1;

  bool is_empty() { return read_index == write_index; }
  bool is_full() { return (write_index + 1) % SIZE == read_index; }
  size_t size() { return (write_index - read_index + SIZE) % SIZE; }
  bool get(canbus::CanFrame *frame) {
    if ((is_empty())) {
      return false;
    }
    *frame = buffer[read_index];
    read_index = (read_index + 1) % SIZE;
    return true;
  }
  bool add(canbus::CanFrame *frame) {
    if (is_full()) {
      return false;
    }
    buffer[write_index] = *frame;
    write_index = (write_index + 1) % SIZE;
    return true;
  }

 private:
  size_t read_index = 0;
  size_t write_index = 0;
  canbus::CanFrame buffer[SIZE];
};

extern "C" void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);

class OnInitializedTrigger : public Trigger<> {};

class STM32FDCan : public canbus::Canbus {
  friend void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);

 public:
  STM32FDCan() : hcan_{0}, tx_pin_{0}, rx_pin_{0}, on_initialized_{0} {}
  // virtual ~STM32FDCan() {

  // }

  void setup();
  // void loop() override;
  void set_tx_pin(InternalGPIOPin *tx_pin) { tx_pin_ = tx_pin; }
  void set_rx_pin(InternalGPIOPin *rx_pin) { rx_pin_ = rx_pin; }
  void loop() override;

  void add_trigger(OnInitializedTrigger *trigger) { on_initialized_ = trigger; }
  void set_instance(FDCAN_GlobalTypeDef *instance) { hcan_.Instance = instance; }

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;
  void push_can_frame(FDCAN_HandleTypeDef *hcan, struct canbus::CanFrame *frame);

 private:
  FDCAN_HandleTypeDef hcan_;
  InternalGPIOPin *tx_pin_;
  InternalGPIOPin *rx_pin_;
  OnInitializedTrigger *on_initialized_;
  CanFrameCircBuffer<31> rx_fifo;
};
}  // namespace stm32_fdcan
}  // namespace esphome
#endif
