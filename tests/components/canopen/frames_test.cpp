#include <gtest/gtest.h>

#include "esphome/components/canopen/types.h"

namespace esphome::canopen::testing {

TEST(CanopenDecodeHeartbeat, NodeAndState) {
  uint8_t node = 0;
  NMTState state = NMTState::NMT_STATE_UNKNOWN;
  const uint8_t op[] = {0x05}, preop[] = {0x7F}, boot[] = {0x00}, stopped[] = {0x04};
  ASSERT_TRUE(decode_heartbeat(0x701, op, 1, node, state));
  EXPECT_EQ(node, 1);
  EXPECT_EQ(state, NMTState::NMT_STATE_OPERATIONAL);
  ASSERT_TRUE(decode_heartbeat(0x77F, preop, 1, node, state));
  EXPECT_EQ(node, 0x7F);
  EXPECT_EQ(state, NMTState::NMT_STATE_PRE_OPERATIONAL);
  ASSERT_TRUE(decode_heartbeat(0x710, boot, 1, node, state));
  EXPECT_EQ(state, NMTState::NMT_STATE_BOOTUP);
  ASSERT_TRUE(decode_heartbeat(0x710, stopped, 1, node, state));
  EXPECT_EQ(state, NMTState::NMT_STATE_STOPPED);
}

TEST(CanopenDecodeHeartbeat, UnknownStateAndToggleBit) {
  uint8_t node = 0;
  NMTState state = NMTState::NMT_STATE_BOOTUP;
  const uint8_t odd[] = {0x42}, toggled[] = {0x85};
  ASSERT_TRUE(decode_heartbeat(0x710, odd, 1, node, state));
  EXPECT_EQ(state, NMTState::NMT_STATE_UNKNOWN);
  // bit 7 is the node guarding toggle bit
  ASSERT_TRUE(decode_heartbeat(0x710, toggled, 1, node, state));
  EXPECT_EQ(state, NMTState::NMT_STATE_OPERATIONAL);
}

TEST(CanopenDecodeHeartbeat, OtherFrames) {
  uint8_t node = 0;
  NMTState state;
  const uint8_t op[] = {0x05};
  EXPECT_FALSE(decode_heartbeat(0x700, op, 1, node, state));  // node 0 doesn't exist
  EXPECT_FALSE(decode_heartbeat(0x780, op, 1, node, state));
  EXPECT_FALSE(decode_heartbeat(0x701, op, 0, node, state));
  EXPECT_FALSE(decode_heartbeat(0x581, op, 1, node, state));
}

TEST(CanopenDecodeEmcy, Fields) {
  uint8_t node = 0;
  Emergency e{};
  const uint8_t emcy[] = {0x10, 0x81, 0x11, 1, 2, 3, 4, 5};
  ASSERT_TRUE(decode_emcy(0x8A, emcy, 8, node, e));
  EXPECT_EQ(node, 0x0A);
  EXPECT_EQ(e.error_code, 0x8110);
  EXPECT_EQ(e.error_register, 0x11);
  EXPECT_EQ(e.manufacturer_data, (std::array<uint8_t, 5>{1, 2, 3, 4, 5}));
  ASSERT_TRUE(decode_emcy(0xFF, emcy, 8, node, e));
  EXPECT_EQ(node, 0x7F);
}

TEST(CanopenDecodeEmcy, OtherFrames) {
  uint8_t node = 0;
  Emergency e{};
  const uint8_t emcy[8] = {};
  EXPECT_FALSE(decode_emcy(0x80, emcy, 8, node, e));  // SYNC
  EXPECT_FALSE(decode_emcy(0x100, emcy, 8, node, e));
  EXPECT_FALSE(decode_emcy(0x81, emcy, 7, node, e));
  EXPECT_FALSE(decode_emcy(0x181, emcy, 8, node, e));  // TPDO1
}

}  // namespace esphome::canopen::testing
