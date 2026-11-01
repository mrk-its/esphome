#ifdef USE_HOST
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>

#include "esphome/components/canopen/canopen.h"
#include "esphome/components/host/preferences.h"
#include "esphome/core/application.h"
#include "fake_canbus.h"

namespace esphome::canopen::testing {
namespace fs = std::filesystem;

static constexpr uint32_t SAVE = 0x65766173;  // "save"
static constexpr uint32_t LOAD = 0x64616F6C;  // "load"

// One boot of a node with one RPDO (default COB-ID cob_id): a new instance is a reboot with the same preferences.
struct Node {
  FakeCanbus bus;
  TestHub hub;
  uint32_t command{0};

  explicit Node(uint32_t cob_id = 0x1B2) {
    this->hub.set_canbus(&this->bus);
    auto &od = this->hub.get_od();
    od.add_var(0x2012, OD_READ_WRITE | OD_RPDO, &this->command, 4);
    this->hub.configure_rpdo(0, cob_id, {{0x2012, 0, 32}});
    this->hub.setup();
  }
  uint16_t event_timer() {
    uint16_t value = 0;
    EXPECT_EQ(OD_get_u16(this->hub.get_od().find(0x1400), 5, &value, true), ODR_OK);
    return value;
  }
  OdResult write_u16(uint16_t index, uint8_t sub, uint16_t value) {
    return this->hub.get_od().write(index, sub, &value, sizeof(value));
  }
  OdResult write_u32(uint16_t index, uint8_t sub, uint32_t value) {
    return this->hub.get_od().write(index, sub, &value, sizeof(value));
  }
};

class CanopenStorage : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = fs::temp_directory_path() / "esphome_canopen_storage_test";
    fs::remove_all(this->temp_dir_);
    fs::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    App.pre_setup("test_canopen_storage", strlen("test_canopen_storage"), "", 0);
    host::setup_preferences();
    host::host_preferences->reset();
  }
  void TearDown() override {
    unsetenv("ESPHOME_PREFDIR");
    fs::remove_all(this->temp_dir_);
  }
  fs::path temp_dir_;
};

TEST_F(CanopenStorage, SaveLoadAndRestoreDefaults) {
  {
    auto a = std::make_unique<Node>();
    ASSERT_FALSE(a->hub.is_failed());
    EXPECT_EQ(a->event_timer(), 0u);
    uint32_t caps = 0;
    EXPECT_EQ(OD_get_u32(a->hub.get_od().find(0x1010), 2, &caps, true), ODR_OK);
    EXPECT_EQ(caps, 1u);  // saves on command
    EXPECT_EQ(OD_get_u32(a->hub.get_od().find(0x1011), 2, &caps, true), ODR_OK);
    EXPECT_EQ(caps, 1u);  // restores
    // the host changes the RPDO (over SDO: through the same handlers) and saves the communication parameters
    EXPECT_EQ(a->write_u16(0x1400, 5, 500), OdResult::OD_RESULT_OK);
    EXPECT_NE(a->write_u32(0x1010, 2, 0x12345678), OdResult::OD_RESULT_OK);  // not the "save" signature
    EXPECT_EQ(a->write_u32(0x1010, 2, SAVE), OdResult::OD_RESULT_OK);
  }
  {
    // reboot: the stored parameters
    auto b = std::make_unique<Node>();
    ASSERT_FALSE(b->hub.is_failed());
    EXPECT_EQ(b->event_timer(), 500u);
    // restore the defaults: from the next boot on
    EXPECT_EQ(b->write_u32(0x1011, 2, LOAD), OdResult::OD_RESULT_OK);
    EXPECT_EQ(b->event_timer(), 500u);
  }
  {
    auto c = std::make_unique<Node>();
    EXPECT_EQ(c->event_timer(), 0u);
    // "all" (sub 1) stores them too
    EXPECT_EQ(c->write_u16(0x1400, 5, 700), OdResult::OD_RESULT_OK);
    EXPECT_EQ(c->write_u32(0x1010, 1, SAVE), OdResult::OD_RESULT_OK);
  }
  {
    auto d = std::make_unique<Node>();
    EXPECT_EQ(d->event_timer(), 700u);
  }
  {
    // a firmware with other defaults: the stored parameters were made for the old ones, ignored
    auto e = std::make_unique<Node>(0x1B3);
    ASSERT_FALSE(e->hub.is_failed());
    EXPECT_EQ(e->event_timer(), 0u);
  }
}

}  // namespace esphome::canopen::testing
#endif
