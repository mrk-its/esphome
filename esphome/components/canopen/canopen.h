#pragma once

#include <array>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/canbus/canbus.h"

#include "od.h"
#include "types.h"

namespace esphome::canopen {

NMTState to_nmt_state(CO_NMT_internalState_t state);

// A CANopen node (CiA 301) on CANopenNode: the NMT state machine (it follows NMT commands), heartbeat producer,
// emergency producer and an SDO server for its object dictionary, which holds the communication objects, identity
// (0x1018) and device information. Identity and device information are set before the hub's setup(), which builds
// the object dictionary and initialises CANopenNode: from codegen (setters run in main(), before App.setup()). The
// rest is runtime API, e.g. from loop() or automations: our own NMT state and its callback.
//
// The implementation is split by feature: canopen.cpp (setup, processing, NMT), driver_can.cpp (CANopenNode's CAN
// driver, frame reception and transmission), device_info.cpp (identity, device information, SDO server).
class CANopenComponent : public Component {
 public:
  explicit CANopenComponent(uint8_t node_id) : node_id_(node_id) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_canbus(canbus::Canbus *canbus);
  void set_heartbeat_interval(uint32_t ms) { this->hb_producer_ms_ = ms; }
  // NMT state after boot: OPERATIONAL (default) or PRE_OPERATIONAL (until an NMT master, or set_nmt_state(), starts
  // the node)
  void set_start_operational(bool operational) { this->start_operational_ = operational; }

  // Our own NMT state (UNKNOWN before setup) and its changes, by an NMT master or set_nmt_state()
  NMTState get_nmt_state() const { return this->nmt_state_; }
  template<typename F> void add_on_nmt_state_callback(F &&callback) {
    this->nmt_state_callback_.add(std::forward<F>(callback));
  }
  // Moves our own node to OPERATIONAL, PRE_OPERATIONAL or STOPPED (no NMT master needed)
  bool set_nmt_state(NMTState state);

  // Used by the CANopenNode driver (driver_can.cpp): sends a standard-ID frame (ident bit 11 = RTR) on the canbus.
  bool transmit(uint16_t ident, const uint8_t *data, uint8_t len);

  // --- Identity and device information (device_info.cpp) ---
  // Identity (0x1018) and device information (0x1008 device name, 0x1009 hardware version, 0x100A software
  // version). Before setup() only; the strings must outlive the component. Defaults: identity 0, the node's name,
  // no hardware version, ESPHOME_VERSION.
  void set_identity(uint32_t vendor_id, uint32_t product_code, uint32_t revision_number, uint32_t serial_number) {
    this->identity_ = {vendor_id, product_code, revision_number, serial_number};
  }
  void set_device_name(const char *name) { this->device_name_ = name; }
  void set_hardware_version(const char *version) { this->hardware_version_ = version; }
  void set_software_version(const char *version) { this->software_version_ = version; }

 protected:
  void on_frame_(uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data);
  void receive_(uint16_t ident, const uint8_t *data, uint8_t len);
  void process_();
  // The features' parts of setup(), called in this order: add_*_od_() add OD entries (before the OD is finalized),
  // config_*_() fill in their CO_config_t counts and entries, init_*_() run once CANopenNode is initialised.

  canbus::Canbus *canbus_{nullptr};
  uint8_t node_id_;
  CO_t *co_{nullptr};
  CO_config_t config_{};  // must outlive co_
  ObjectDictionary od_;

  uint8_t err_reg_{0};             // 0x1001
  uint32_t cob_emcy_{0x80};        // 0x1014, CANopenNode adds the node id
  uint16_t hb_producer_ms_{5000};  // 0x1017

  uint32_t last_us_{0};
  bool start_operational_{true};
  NMTState nmt_state_{NMTState::NMT_STATE_UNKNOWN};
  LazyCallbackManager<void(NMTState)> nmt_state_callback_;

  // --- Identity and device information (device_info.cpp) ---
  void add_device_info_od_();
  void config_device_info_(CO_config_t &cfg);
  void dump_device_info_config_();
  uint32_t dev_type_{0};                   // 0x1000
  const char *device_name_{nullptr};       // 0x1008
  const char *hardware_version_{nullptr};  // 0x1009, only if set
  const char *software_version_{nullptr};  // 0x100A
  uint8_t identity_highest_sub_{4};        // 0x1018
  std::array<uint32_t, 4> identity_{};     // 0x1018:1..4
  // 0x1200 (SDO server 1): CANopenNode fills in the default COB-IDs 0x600 / 0x580 + node id
  uint8_t sdo_srv_highest_sub_{2};
  uint32_t sdo_srv_cob_rx_{0};
  uint32_t sdo_srv_cob_tx_{0};
};

}  // namespace esphome::canopen
