#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

#include "esphome/components/canopen/canopen_node.h"
#include "fake_canbus.h"

namespace esphome::canopen::testing {

class RecordingNode : public CANopenNode {
 public:
  using CANopenNode::CANopenNode;
  std::vector<std::string> calls;
  std::vector<uint16_t> emcy_codes;

 protected:
  void on_node_online() override { this->calls.emplace_back("online"); }
  void on_node_offline() override { this->calls.emplace_back("offline"); }
  void on_node_nmt_changed(NMTState state) override {
    this->calls.emplace_back(std::string("nmt ") + LOG_STR_ARG(nmt_state_to_string(state)));
  }
  void on_node_reset() override { this->calls.emplace_back("reset"); }
  void on_node_emcy(const Emergency &emergency) override { this->emcy_codes.push_back(emergency.error_code); }
};

TEST(CanopenNode, HooksForOwnNodeOnly) {
  TestHub hub{nullptr};
  RecordingNode drive(&hub, 0x21);
  RecordingNode other(&hub, 0x22);

  hub.node_event(0x21, NodeEvent::NODE_EVENT_ONLINE, NMTState::NMT_STATE_PRE_OPERATIONAL);
  hub.node_event(0x21, NodeEvent::NODE_EVENT_NMT_CHANGED, NMTState::NMT_STATE_OPERATIONAL);
  hub.node_event(0x21, NodeEvent::NODE_EVENT_RESET, NMTState::NMT_STATE_BOOTUP);
  hub.node_event(0x21, NodeEvent::NODE_EVENT_OFFLINE, NMTState::NMT_STATE_UNKNOWN);
  hub.emcy(0x21, Emergency{0x8110, 0x11, {}});
  hub.node_event(0x23, NodeEvent::NODE_EVENT_ONLINE, NMTState::NMT_STATE_OPERATIONAL);
  hub.emcy(0x23, Emergency{0x1000, 0x01, {}});

  EXPECT_EQ(drive.calls, (std::vector<std::string>{"online", "nmt operational", "reset", "offline"}));
  EXPECT_EQ(drive.emcy_codes, std::vector<uint16_t>{0x8110});
  EXPECT_TRUE(other.calls.empty());
  EXPECT_TRUE(other.emcy_codes.empty());
}

TEST(CanopenHub, NmtStateBeforeSetup) {
  TestHub hub{nullptr};
  EXPECT_EQ(hub.get_nmt_state(), NMTState::NMT_STATE_UNKNOWN);
  EXPECT_FALSE(hub.set_nmt_state(NMTState::NMT_STATE_OPERATIONAL));
  uint8_t value = 1;
  EXPECT_EQ(hub.write_od(0x1001, 0, &value, 1), OdResult::OD_RESULT_DEVICE_STATE);
}

TEST(CanopenNode, NodeIdAndUnmonitoredState) {
  TestHub hub{nullptr};
  RecordingNode drive(&hub, 0x21);

  EXPECT_EQ(drive.get_node_id(), 0x21);
  EXPECT_EQ(drive.get_canopen(), &hub);
  // the hub isn't set up (no heartbeat consumer): unknown, offline
  EXPECT_FALSE(drive.is_online());
  EXPECT_EQ(drive.get_nmt_state(), NMTState::NMT_STATE_UNKNOWN);
  EXPECT_FALSE(drive.is_operational());
}

TEST(CanopenNode, ForwardsWithNodeIdAndContext) {
  TestHub hub{nullptr};
  RecordingNode drive(&hub, 0x21);
  struct Result {
    void *context{nullptr};
    SDOResult result{SDOResult::SDO_RESULT_SUCCESS};
    int calls{0};
  } result;
  static Result *current = nullptr;
  current = &result;
  auto record = +[](void *context, const SDOReply &reply) {
    current->context = context;
    current->result = reply.result;
    current->calls++;
  };

  // the hub failed: rejected, called back synchronously, with `this` as the default context
  hub.mark_failed();
  EXPECT_FALSE(drive.read_sdo<uint16_t>(0x1017, 0, record));
  EXPECT_EQ(result.calls, 1);
  EXPECT_EQ(result.context, &drive);
  EXPECT_EQ(result.result, SDOResult::SDO_RESULT_REJECTED);

  int own_context = 0;
  EXPECT_FALSE(drive.write_sdo<uint16_t>(0x6040, 0, 0x0F, record, &own_context));
  EXPECT_EQ(result.calls, 2);
  EXPECT_EQ(result.context, &own_context);

  drive.cancel_sdo();
  EXPECT_FALSE(drive.send_nmt(NMTCommand::NMT_COMMAND_START));
  current = nullptr;
}

struct Call {
  void *context;
  SDOReply reply;
  std::string data;
};
static std::vector<Call> *calls = nullptr;
static void record_call(void *context, const SDOReply &reply) {
  std::string data;
  if (reply.data != nullptr)
    data.assign(reinterpret_cast<const char *>(reply.data), reply.len);
  calls->push_back({context, reply, data});
}

// on a running hub: the requests carry the node's id, the replies its context; cancel_sdo() drops only its own
// requests; NMT commands address the node; the state follows its heartbeats
TEST(CanopenNode, ForwardsOnARunningHub) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.monitor_node(0x21, 1000);
  RecordingNode drive(&hub, 0x21);
  RecordingNode other(&hub, 0x22);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  std::vector<Call> recorded;
  calls = &recorded;

  ASSERT_TRUE(drive.read_sdo<uint16_t>(0x1017, 0, record_call));
  hub.loop();
  const auto *request = bus.find(0x621);
  ASSERT_NE(request, nullptr);
  EXPECT_EQ(request->data, (std::vector<uint8_t>{0x40, 0x17, 0x10, 0x00, 0, 0, 0, 0}));
  hub.frame(0x5A1, {0x4B, 0x17, 0x10, 0x00, 0xE8, 0x03, 0, 0});
  hub.loop();
  ASSERT_EQ(recorded.size(), 1u);
  EXPECT_EQ(recorded[0].context, &drive);
  ASSERT_TRUE(recorded[0].reply.ok());
  EXPECT_EQ(recorded[0].reply.as<uint16_t>(), 1000);

  // a buffer read into the caller's buffer
  static std::array<uint8_t, 8> buffer{};
  int own_context = 0;
  ASSERT_TRUE(drive.read_sdo_data(0x1008, 0, buffer.data(), buffer.size(), record_call, &own_context));
  hub.loop();
  hub.frame(0x5A1, {0x43, 0x08, 0x10, 0x00, 'a', 'b', 'c', 'd'});
  hub.loop();
  ASSERT_EQ(recorded.size(), 2u);
  EXPECT_EQ(recorded[1].context, &own_context);
  ASSERT_TRUE(recorded[1].reply.ok());
  EXPECT_EQ(recorded[1].reply.data, buffer.data());
  EXPECT_EQ(recorded[1].data, "abcd");

  // the other profile's transfer is active, the drive's requests wait: cancelling the drive's keeps the other's
  bus.sent.clear();
  ASSERT_TRUE(other.read_sdo<uint16_t>(0x1017, 0, record_call));
  hub.loop();
  ASSERT_TRUE(drive.read_sdo<uint16_t>(0x1017, 0, record_call));
  ASSERT_TRUE(drive.write_sdo<uint16_t>(0x6040, 0, 0x0F, record_call));
  drive.cancel_sdo();
  hub.frame(0x5A2, {0x4B, 0x17, 0x10, 0x00, 0xD0, 0x07, 0, 0});
  for (int i = 0; i < 5; i++)
    hub.loop();
  ASSERT_EQ(recorded.size(), 3u);
  EXPECT_EQ(recorded[2].context, &other);
  EXPECT_EQ(recorded[2].reply.as<uint16_t>(), 2000);
  EXPECT_EQ(bus.count(0x622), 1u);
  EXPECT_EQ(bus.count(0x621), 0u);
  calls = nullptr;

  ASSERT_TRUE(drive.send_nmt(NMTCommand::NMT_COMMAND_START));
  hub.loop();
  const auto *nmt = bus.find(0x000);
  ASSERT_NE(nmt, nullptr);
  EXPECT_EQ(nmt->data, (std::vector<uint8_t>{0x01, 0x21}));

  hub.frame(0x721, {0x05});
  EXPECT_TRUE(drive.is_online());
  EXPECT_TRUE(drive.is_operational());
  EXPECT_FALSE(other.is_online());
  hub.frame(0x721, {0x7F});
  EXPECT_TRUE(drive.is_online());
  EXPECT_EQ(drive.get_nmt_state(), NMTState::NMT_STATE_PRE_OPERATIONAL);
  EXPECT_FALSE(drive.is_operational());
}

// a profile with another base before CANopenNode, as components are (Component, CANopenNode)
struct OtherBase {
  virtual ~OtherBase() = default;
  int data{0};
};
class MultiNode : public OtherBase, public CANopenNode {
 public:
  using CANopenNode::CANopenNode;
};

TEST(CanopenNode, FromContextWithOtherBases) {
  TestHub hub{nullptr};
  MultiNode drive(&hub, 0x21);
  static void *seen = nullptr;
  seen = nullptr;

  // rejected by the failed hub: the callback runs right away
  hub.mark_failed();
  drive.read_sdo<uint16_t>(0x1017, 0, +[](void *context, const SDOReply &) { seen = context; });
  ASSERT_NE(seen, nullptr);
  // the default context is the CANopenNode subobject, not the start of MultiNode
  EXPECT_NE(seen, static_cast<void *>(&drive));
  EXPECT_EQ(CANopenNode::from_context<MultiNode>(seen), &drive);
}

}  // namespace esphome::canopen::testing
