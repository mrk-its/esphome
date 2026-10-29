#pragma once

#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/canbus/canbus.h"

#include "od.h"
#include "types.h"

namespace esphome::canopen {

class CANopenComponent : public Component {
 public:
  explicit CANopenComponent(uint8_t node_id) : node_id_(node_id) {}

  void set_canbus(canbus::Canbus *canbus);
  void set_heartbeat_interval(uint32_t ms) { this->hb_producer_ms_ = ms; }

  // Heartbeat monitoring of another node (CANopenNode heartbeat consumer, OD 0x1016). Before setup() only (called
  // from codegen). Registering a node again keeps the shortest timeout.
  void monitor_node(uint8_t node_id, uint16_t timeout_ms);
  // events of all monitored nodes: (node_id, event, state)
  template<typename F> void add_on_node_event_callback(F &&callback) {
    this->node_event_callback_.add(std::forward<F>(callback));
  }
  // state of a monitored node from its last heartbeat; UNKNOWN / false for nodes that aren't monitored
  NMTState get_node_state(uint8_t node_id);
  bool is_node_online(uint8_t node_id);

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  // used by the CANopenNode driver (driver_can.cpp)
  canbus::Canbus *get_canbus() { return this->canbus_; }

 protected:
  void on_frame_(uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data);
  void process_();
  void node_event_(uint8_t node_id, NodeEvent event, NMTState state);

  static void on_hb_started(uint8_t node_id, uint8_t idx, void *object);
  static void on_hb_nmt_changed(uint8_t node_id, uint8_t idx, CO_NMT_internalState_t state, void *object);
  static void on_hb_timeout(uint8_t node_id, uint8_t idx, void *object);
  static void on_hb_remote_reset(uint8_t node_id, uint8_t idx, void *object);
  static void on_nmt_changed(CO_NMT_internalState_t state);

  canbus::Canbus *canbus_{nullptr};
  uint8_t node_id_;
  CO_t *co_{nullptr};
  CO_config_t config_{};  // must outlive co_
  ObjectDictionary od_;

  // OD storage
  uint32_t dev_type_{0};           // 0x1000
  uint8_t err_reg_{0};             // 0x1001
  uint32_t cob_emcy_{0x80};        // 0x1014, CANopenNode adds the node id
  uint16_t hb_producer_ms_{5000};  // 0x1017
  uint8_t hb_cons_count_{0};       // 0x1016:0
  std::vector<uint32_t> hb_cons_;  // 0x1016:1.., (node_id << 16) | timeout_ms

  uint32_t last_us_{0};
  LazyCallbackManager<void(uint8_t, NodeEvent, NMTState)> node_event_callback_;
};

}  // namespace esphome::canopen
