#include <gtest/gtest.h>

#include <vector>

#include "fake_canbus.h"

// When a hub processes received frames and retries the frames the controller refused.
namespace esphome::canopen::testing {

struct RpdoTarget : OdObject {
  uint32_t value{0};
  int writes{0};
  OdResult od_write(OdStream stream, const void *buf, size_t count, size_t *done) {
    this->writes++;
    return stream.write_original(buf, count, done);
  }
};

// CANopenNode keeps one received frame per RPDO: a node processes each frame of a canbus pass before the next one
// overwrites it.
TEST(CanopenProcessing, NodeAppliesEveryRpdoOfABurst) {
  FakeCanbus bus;
  TestHub hub(&bus);
  RpdoTarget command;
  auto &od = hub.get_od();
  od.add_var(0x2012, OD_READ_WRITE | OD_RPDO, &command.value, 4);
  od.set_object(0x2012, &command);
  ASSERT_TRUE(hub.configure_rpdo(0, 0x1B2, {{0x2012, 0, 32}}));
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();  // operational
  bus.queue(0x1B2, {1, 0, 0, 0});
  bus.queue(0x1B2, {2, 0, 0, 0});
  bus.loop();
  EXPECT_EQ(command.writes, 2);
  EXPECT_EQ(command.value, 2u);
}

// A node processes received frames right away, but queued frames are retried only from loop(): on ESP32 each refused
// send can block.
TEST(CanopenProcessing, NodeRetriesQueuedFramesOnlyFromLoop) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  bus.busy = true;
  ASSERT_TRUE(hub.write_sdo<uint8_t>(0x21, 0x6060, 0, 1));
  hub.loop();
  bus.busy = false;
  hub.frame(0x701, {0x05});
  EXPECT_EQ(bus.count(0x621), 0u);
  hub.loop();
  EXPECT_EQ(bus.count(0x621), 1u);
}

// A client-only hub has nothing to do between requests: its loop is off until the next one.
TEST_F(SdoTest, ClientOnlyLoopIdlesBetweenRequests) {
  this->hub_.loop();
  EXPECT_TRUE(this->hub_.is_idle());
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  EXPECT_FALSE(this->hub_.is_idle());
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x37, 0x02, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  this->hub_.loop();
  EXPECT_TRUE(this->hub_.is_idle());
}

// A node always has work (heartbeat, PDO timers).
TEST(CanopenProcessing, NodeLoopStaysOn) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  hub.loop();
  EXPECT_FALSE(hub.is_idle());
}

}  // namespace esphome::canopen::testing
