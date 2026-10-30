#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/canopen/canopen_node.h"

namespace esphome::canopen::testing {

// the hub's event sources are protected: raise them as CANopenNode's heartbeat consumer / frame callbacks do
class TestHub : public CANopenComponent {
 public:
  TestHub() : CANopenComponent(0x10) {}
  void node_event(uint8_t node_id, NodeEvent event, NMTState state) { this->node_event_(node_id, event, state); }
  void emcy(uint8_t node_id, const Emergency &emergency) { this->emcy_callback_.call(node_id, emergency); }
};

class RecordingNode : public CANopenNode {
 public:
  using CANopenNode::CANopenNode;
  std::vector<std::string> calls;
  std::vector<uint16_t> emcy_codes;

 protected:
  void on_node_online() override { this->calls.emplace_back("online"); }
  void on_node_offline() override { this->calls.emplace_back("offline"); }
  void on_node_nmt_changed(NMTState state) override {
    this->calls.emplace_back(std::string("nmt ") + nmt_state_to_string(state));
  }
  void on_node_reset() override { this->calls.emplace_back("reset"); }
  void on_node_emcy(const Emergency &emergency) override { this->emcy_codes.push_back(emergency.error_code); }
};

TEST(CanopenNode, HooksForOwnNodeOnly) {
  TestHub hub;
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

TEST(CanopenNode, NodeIdAndUnmonitoredState) {
  TestHub hub;
  RecordingNode drive(&hub, 0x21);

  EXPECT_EQ(drive.get_node_id(), 0x21);
  EXPECT_EQ(drive.get_canopen(), &hub);
  // the hub isn't set up (no heartbeat consumer): unknown, offline
  EXPECT_FALSE(drive.is_online());
  EXPECT_EQ(drive.get_nmt_state(), NMTState::NMT_STATE_UNKNOWN);
  EXPECT_FALSE(drive.is_operational());
}

TEST(CanopenNode, ForwardsWithNodeIdAndContext) {
  TestHub hub;
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

  // the hub isn't set up: rejected, called back synchronously, with `this` as the default context
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

}  // namespace esphome::canopen::testing
