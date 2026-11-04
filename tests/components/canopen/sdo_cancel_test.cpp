#include <gtest/gtest.h>

#include "fake_canbus.h"

namespace esphome::canopen::testing {

class SdoCancellation : public ::testing::TestWithParam<bool> {
 protected:
  FakeCanbus bus;
  TestHub hub;
  int callbacks{0};

  void SetUp() override {
    this->hub.set_canbus(&this->bus);
    this->hub.setup();
    ASSERT_FALSE(this->hub.is_failed());
    this->hub.loop();  // Send the boot-up heartbeat before blocking the bus.
    this->bus.sent.clear();
  }

  static void on_result(void *context, const SDOReply &) { (*static_cast<int *>(context))++; }

  void queue_request() {
    if (GetParam()) {
      ASSERT_TRUE(this->hub.write_sdo<uint16_t>(0x21, 0x6040, 0, 0x0F, on_result, &this->callbacks));
    } else {
      ASSERT_TRUE(this->hub.read_sdo<uint16_t>(0x21, 0x6041, 0, on_result, &this->callbacks));
    }
  }
};

TEST_P(SdoCancellation, UnsentRequestIsReplacedWithAbort) {
  this->bus.busy = true;
  this->queue_request();
  this->hub.loop();
  ASSERT_EQ(this->bus.count(0x621), 0u);

  this->hub.cancel_sdo(&this->callbacks);
  this->bus.busy = false;
  this->hub.loop();
  this->hub.loop();

  ASSERT_EQ(this->bus.count(0x621), 1u);
  const auto *frame = this->bus.find(0x621);
  ASSERT_NE(frame, nullptr);
  const uint8_t index_low = GetParam() ? 0x40 : 0x41;
  EXPECT_EQ(frame->data, (std::vector<uint8_t>{0x80, index_low, 0x60, 0, 0, 0, 0, 0x08}));
  EXPECT_EQ(this->callbacks, 0);
}

TEST_P(SdoCancellation, PendingAbortSurvivesNextRequestAndOtherTraffic) {
  this->bus.busy = true;
  this->queue_request();
  this->hub.loop();
  // A second software TX buffer must survive cancelling the SDO client.
  ASSERT_TRUE(this->hub.send_nmt(0x23, NMTCommand::NMT_COMMAND_START));
  this->hub.cancel_sdo(&this->callbacks);

  int other_callbacks = 0;
  ASSERT_TRUE(this->hub.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &other_callbacks));
  this->hub.loop();  // The abort is still blocked; do not reuse its buffer for node 0x22.
  this->bus.busy = false;
  this->hub.loop();
  this->hub.loop();

  ASSERT_EQ(this->bus.count(0x621), 1u);
  EXPECT_EQ(this->bus.find(0x621)->data[0], 0x80);
  EXPECT_EQ(this->bus.count(0), 1u);
  EXPECT_EQ(this->bus.count(0x622), 1u);
  this->hub.frame(0x5A2, {0x4B, 0x41, 0x60, 0, 0x27, 0, 0, 0});
  EXPECT_EQ(other_callbacks, 1);
  EXPECT_EQ(this->callbacks, 0);
}

TEST_P(SdoCancellation, CancellingQueuedContextPreservesActiveRequest) {
  this->queue_request();
  this->hub.loop();
  ASSERT_EQ(this->bus.count(0x621), 1u);
  int queued_callbacks = 0;
  ASSERT_TRUE(this->hub.read_sdo<uint16_t>(0x22, 0x6041, 0, on_result, &queued_callbacks));
  this->hub.cancel_sdo(&queued_callbacks);

  if (GetParam()) {
    this->hub.frame(0x5A1, {0x60, 0x40, 0x60, 0, 0, 0, 0, 0});
  } else {
    this->hub.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x27, 0, 0, 0});
  }
  this->hub.loop();
  EXPECT_EQ(this->callbacks, 1);
  EXPECT_EQ(queued_callbacks, 0);
  EXPECT_EQ(this->bus.count(0x621), 1u);
  EXPECT_EQ(this->bus.count(0x622), 0u);
}

INSTANTIATE_TEST_SUITE_P(ReadAndWrite, SdoCancellation, ::testing::Bool());

}  // namespace esphome::canopen::testing
