#pragma once

// Public CANopen types for lambdas and other components. No CANopenNode headers here.

#include <cstdint>

namespace esphome::canopen {

// NMT state as sent in a heartbeat (CiA 301); also used for our own state
enum class NMTState : uint8_t {
  NMT_STATE_BOOTUP = 0,
  NMT_STATE_STOPPED = 4,
  NMT_STATE_OPERATIONAL = 5,
  NMT_STATE_PRE_OPERATIONAL = 127,
  NMT_STATE_UNKNOWN = 255,
};

const char *nmt_state_to_string(NMTState state);

// Events of a monitored node (CANopenComponent::monitor_node(), from CANopenNode's heartbeat consumer):
// ONLINE: first heartbeat, or the first after a timeout; OFFLINE: no heartbeat within the timeout;
// NMT_CHANGED: the state in the heartbeat changed (also unknown -> state right after ONLINE, state -> unknown after
// OFFLINE); RESET: boot-up message, the node restarted and lost its runtime configuration.
enum class NodeEvent : uint8_t { NODE_EVENT_ONLINE, NODE_EVENT_OFFLINE, NODE_EVENT_NMT_CHANGED, NODE_EVENT_RESET };

const char *node_event_to_string(NodeEvent event);

}  // namespace esphome::canopen
