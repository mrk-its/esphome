#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/canopen/canopen.h"
#include "fake_canbus.h"

namespace esphome::canopen::testing {

struct Value : OdObject {
  uint32_t value{0};
  int writes{0};
  OdResult od_write(OdStream stream, const void *buf, size_t count, size_t *done) {
    this->writes++;
    return stream.write_original(buf, count, done);
  }
};

TEST(CanopenPdo, TpdoAndRpdo) {
  FakeCanbus bus;
  TestHub hub;
  hub.set_canbus(&bus);
  Value state, command;
  state.value = 0x11223344;
  auto &od = hub.get_od();
  od.add_var(0x2011, OD_READ | OD_TPDO, &state.value, 4);
  od.set_object(0x2011, &state);
  od.add_var(0x2012, OD_READ_WRITE | OD_RPDO, &command.value, 4);
  od.set_object(0x2012, &command);
  ASSERT_TRUE(hub.configure_tpdo(0, 0x190, {{0x2011, 0, 32}}));
  // TPDO 1 is not configured: a disabled placeholder, so the records stay contiguous
  ASSERT_TRUE(hub.configure_tpdo(2, 0x290, {{0x2011, 0, 32}}));
  ASSERT_TRUE(hub.configure_rpdo(0, 0x1B2, {{0x2012, 0, 32}}));
  // no mappings: disabled (an enabled empty PDO would be a mapping error, EMCY)
  ASSERT_TRUE(hub.configure_tpdo(3, 0x390, {}));
  hub.setup();
  ASSERT_FALSE(hub.is_failed());

  uint32_t cob_id = 0;
  EXPECT_EQ(OD_get_u32(od.find(0x1801), 1, &cob_id, true), ODR_OK);
  EXPECT_EQ(cob_id, 0x80000000u);
  uint32_t mapping = 0;
  EXPECT_EQ(OD_get_u32(od.find(0x1A00), 1, &mapping, true), ODR_OK);
  EXPECT_EQ(mapping, 0x20110020u);
  EXPECT_EQ(OD_get_u32(od.find(0x1400), 1, &cob_id, true), ODR_OK);
  EXPECT_EQ(cob_id, 0x1B2u);

  // NMT -> operational; every TPDO goes out once (CANopenNode's initial send request)
  hub.loop();
  EXPECT_EQ(hub.get_nmt_state(), NMTState::NMT_STATE_OPERATIONAL);
  ASSERT_EQ(bus.count(0x190), 1u);
  EXPECT_EQ(bus.find(0x190)->data, (std::vector<uint8_t>{0x44, 0x33, 0x22, 0x11}));
  EXPECT_EQ(bus.count(0x290), 1u);
  EXPECT_EQ(bus.count(0x390), 0u);
  EXPECT_EQ(OD_get_u32(od.find(0x1803), 1, &cob_id, true), ODR_OK);
  EXPECT_EQ(cob_id, 0x80000390u);
  EXPECT_EQ(bus.count(0x90), 0u);  // no EMCY (0x80 + node id)

  // a changed object: the TPDO that maps it (one request flag per sub-entry: only the first of TPDO 0 / 2)
  bus.sent.clear();
  state.value = 5;
  ASSERT_TRUE(hub.request_tpdo(0x2011, 0));
  hub.loop();
  EXPECT_EQ(bus.count(0x190), 1u);
  EXPECT_EQ(bus.count(0x290), 0u);
  EXPECT_EQ(bus.find(0x190)->data, (std::vector<uint8_t>{5, 0, 0, 0}));

  // one TPDO
  bus.sent.clear();
  hub.request_tpdo(static_cast<uint8_t>(2));
  hub.loop();
  EXPECT_EQ(bus.count(0x190), 0u);
  EXPECT_EQ(bus.count(0x290), 1u);

  // RPDO data goes through the handler
  hub.frame(0x1B2, {0x78, 0x56, 0x34, 0x12});
  EXPECT_EQ(command.value, 0x12345678u);
  EXPECT_EQ(command.writes, 1);
  // too short for the mapping: not written
  hub.frame(0x1B2, {0x01});
  hub.loop();
  EXPECT_EQ(command.value, 0x12345678u);
  EXPECT_EQ(command.writes, 1);

  // no handler: no request flags
  EXPECT_FALSE(hub.request_tpdo(0x1001, 0));
}

TEST(CanopenPdo, NotOperational) {
  FakeCanbus bus;
  TestHub hub;
  hub.set_canbus(&bus);
  hub.set_start_operational(false);
  uint32_t value = 1;
  hub.get_od().add_var(0x2011, OD_READ | OD_TPDO, &value, 4);
  ASSERT_TRUE(hub.configure_tpdo(0, 0x190, {{0x2011, 0, 32}}));
  hub.setup();
  hub.loop();
  EXPECT_EQ(hub.get_nmt_state(), NMTState::NMT_STATE_PRE_OPERATIONAL);
  EXPECT_EQ(bus.count(0x190), 0u);
  // the initial send request is still pending
  hub.set_nmt_state(NMTState::NMT_STATE_OPERATIONAL);
  hub.loop();
  hub.loop();
  EXPECT_EQ(bus.count(0x190), 1u);
}

TEST(CanopenPdo, InvalidConfiguration) {
  TestHub hub;
  EXPECT_FALSE(hub.configure_tpdo(CANopenComponent::MAX_PDO, 0x190, {}));
  EXPECT_FALSE(hub.configure_tpdo(0, 0x190, {{0x2011, 0, 12}}));                   // not whole bytes
  EXPECT_FALSE(hub.configure_tpdo(0, 0x190, {{0x2011, 0, 32}, {0x2012, 0, 40}}));  // more than 8 bytes
  EXPECT_FALSE(hub.configure_rpdo(0, 0x000, {{0x2012, 0, 8}}));                    // no CAN ID
  EXPECT_TRUE(hub.configure_rpdo(0, 0x1B2, {{0x2012, 0, 8}}));
}

}  // namespace esphome::canopen::testing
