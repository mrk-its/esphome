#pragma once

// Public CANopen types for lambdas and other components. No CANopenNode headers here.

#include <cstdint>

namespace esphome {
struct LogString;
}  // namespace esphome

namespace esphome::canopen {

// NMT state as sent in a heartbeat (CiA 301); also used for our own state
enum class NMTState : uint8_t {
  NMT_STATE_BOOTUP = 0,
  NMT_STATE_STOPPED = 4,
  NMT_STATE_OPERATIONAL = 5,
  NMT_STATE_PRE_OPERATIONAL = 127,
  NMT_STATE_UNKNOWN = 255,
};

const LogString *nmt_state_to_string(NMTState state);

}  // namespace esphome::canopen
