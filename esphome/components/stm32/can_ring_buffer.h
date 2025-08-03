#pragma once
#ifdef USE_STM32

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace stm32 {

class CanFrameRingBuffer {
 public:
  CanFrameRingBuffer(size_t capacity) : size_{capacity + 1} { this->buffer_ = new canbus::CanFrame[this->size_]; }
  bool is_empty() { return this->read_index_ == this->write_index_; }
  bool is_full() { return (this->write_index_ + 1) % this->size_ == this->read_index_; }
  size_t size() { return (this->write_index_ - this->read_index_ + this->size_) % this->size_; }
  bool get(canbus::CanFrame *frame) {
    if ((this->is_empty())) {
      return false;
    }
    *frame = this->buffer_[this->read_index_];
    this->read_index_ = (this->read_index_ + 1) % this->size_;
    return true;
  }
  bool add(canbus::CanFrame *frame) {
    if (this->is_full()) {
      return false;
    }
    this->buffer_[this->write_index_] = *frame;
    this->write_index_ = (this->write_index_ + 1) % this->size_;
    return true;
  }

 private:
  size_t size_;
  size_t read_index_ = 0;
  size_t write_index_ = 0;
  canbus::CanFrame *buffer_;
};
}  // namespace stm32
}  // namespace esphome
#endif
