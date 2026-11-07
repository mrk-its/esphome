#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "fake_canbus.h"

namespace esphome::canopen::testing {

class SdoCancellation : public ::testing::TestWithParam<bool> {
 protected:
  FakeCanbus bus_;
  TestHub hub_;
  int callbacks_{0};

  void SetUp() override {
    this->hub_.set_canbus(&this->bus_);
    this->hub_.setup();
    ASSERT_FALSE(this->hub_.is_failed());
    this->hub_.loop();  // Send the boot-up heartbeat before blocking the bus.
    this->bus_.sent.clear();
  }

  static void on_result(void *context, const SDOReply &) { (*static_cast<int *>(context))++; }

  void queue_request_() {
    if (GetParam()) {
      ASSERT_TRUE(this->hub_.write_sdo<uint16_t>(0x21, 0x6040, 0, 0x0F, on_result, &this->callbacks_));
    } else {
      ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &this->callbacks_));
    }
  }
};

TEST_P(SdoCancellation, UnsentRequestIsReplacedWithAbort) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 0u);

  this->hub_.cancel_sdo(&this->callbacks_);
  this->bus_.busy = false;
  this->hub_.loop();
  this->hub_.loop();

  ASSERT_EQ(this->bus_.count(0x621), 1u);
  const auto *frame = this->bus_.find(0x621);
  ASSERT_NE(frame, nullptr);
  const uint8_t index_low = GetParam() ? 0x40 : 0x41;
  EXPECT_EQ(frame->data, (std::vector<uint8_t>{0x80, index_low, 0x60, 0, 0, 0, 0, 0x08}));
  EXPECT_EQ(this->callbacks_, 0);
}

TEST_P(SdoCancellation, PendingAbortSurvivesNextRequestAndOtherTraffic) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  // A second software TX buffer must survive cancelling the SDO client.
  ASSERT_TRUE(this->hub_.send_nmt(0x23, NMTCommand::NMT_COMMAND_START));
  this->hub_.cancel_sdo(&this->callbacks_);

  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();  // The abort is still blocked; do not reuse its buffer for node 0x22.
  this->bus_.busy = false;
  this->hub_.loop();
  this->hub_.loop();

  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.find(0x621)->data[0], 0x80);
  EXPECT_EQ(this->bus_.count(0), 1u);
  EXPECT_EQ(this->bus_.count(0x622), 1u);
  this->hub_.frame(0x5A2, {0x4B, 0x41, 0x60, 0, 0x27, 0, 0, 0});
  EXPECT_EQ(other_callbacks, 1);
  EXPECT_EQ(this->callbacks_, 0);
}

TEST_P(SdoCancellation, CancellingQueuedContextPreservesActiveRequest) {
  this->queue_request_();
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  int queued_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &queued_callbacks));
  this->hub_.cancel_sdo(&queued_callbacks);

  if (GetParam()) {
    this->hub_.frame(0x5A1, {0x60, 0x40, 0x60, 0, 0, 0, 0, 0});
  } else {
    this->hub_.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x27, 0, 0, 0});
  }
  this->hub_.loop();
  EXPECT_EQ(this->callbacks_, 1);
  EXPECT_EQ(queued_callbacks, 0);
  EXPECT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.count(0x622), 0u);
}

TEST_P(SdoCancellation, AbortGuardStartsAfterRecoveryWithoutIntermediateLoops) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  this->hub_.cancel_sdo(&this->callbacks_);
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &other_callbacks));
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  this->bus_.busy = false;
  this->hub_.loop();
  ASSERT_EQ(this->bus_.count(0x621), 1u);
  EXPECT_EQ(this->bus_.find(0x621)->data[0], 0x80);
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 2u);
}

TEST_P(SdoCancellation, AbortGuardSurvivesRecoveryWithEmptyQueue) {
  this->bus_.busy = true;
  this->queue_request_();
  this->hub_.loop();
  this->hub_.cancel_sdo(&this->callbacks_);
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  this->bus_.busy = false;
  this->hub_.loop();
  int other_callbacks = 0;
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &other_callbacks));
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 1u);
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  this->hub_.loop();
  EXPECT_EQ(this->bus_.count(0x621), 2u);
}

INSTANTIATE_TEST_SUITE_P(ReadAndWrite, SdoCancellation, ::testing::Bool());

}  // namespace esphome::canopen::testing
