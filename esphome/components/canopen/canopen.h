#pragma once

#include <array>
#include <initializer_list>
#include <memory>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
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
  // Identity (0x1018) and device information (0x1008 device name, 0x1009 hardware version, 0x100A software
  // version). Before setup() only; the strings must outlive the component. Defaults: identity 0, the node's name,
  // no hardware version, ESPHOME_VERSION.
  void set_identity(uint32_t vendor_id, uint32_t product_code, uint32_t revision_number, uint32_t serial_number) {
    this->identity_ = {vendor_id, product_code, revision_number, serial_number};
  }
  void set_device_name(const char *name) { this->device_name_ = name; }
  // NMT state after boot: OPERATIONAL (default) or PRE_OPERATIONAL (until an NMT master, or set_nmt_state(), starts
  // the node)
  void set_start_operational(bool operational) { this->start_operational_ = operational; }
  void set_hardware_version(const char *version) { this->hardware_version_ = version; }
  void set_software_version(const char *version) { this->software_version_ = version; }
  void set_response_timeout(uint16_t ms) { this->response_timeout_ms_ = ms; }

  // Object dictionary for other components: entries and handlers (OdObject) are added before setup() only, since
  // CANopenNode can't add entries after its init. The storage must outlive the component.
  ObjectDictionary &get_od() { return this->od_; }
  // Writes a value to our own OD through its handler, as an SDO download does (e.g. for commands received in other
  // frames). Synchronous; needs write access.
  OdResult write_od(uint16_t index, uint8_t sub, const void *data, size_t len) {
    return this->od_.write(index, sub, data, len);
  }

  // Our own NMT state (UNKNOWN before setup) and its changes, by an NMT master or set_nmt_state()
  NMTState get_nmt_state() const { return this->nmt_state_; }
  template<typename F> void add_on_nmt_state_callback(F &&callback) {
    this->nmt_state_callback_.add(std::forward<F>(callback));
  }
  // Moves our own node to OPERATIONAL, PRE_OPERATIONAL or STOPPED (no NMT master needed)
  bool set_nmt_state(NMTState state);

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

  // every heartbeat / EMCY frame on the bus, from any node (our own frames aren't received)
  template<typename F> void add_on_heartbeat_callback(F &&callback) {
    this->heartbeat_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_emcy_callback(F &&callback) { this->emcy_callback_.add(std::forward<F>(callback)); }

  // Raw frames the OD doesn't describe (e.g. other devices' PDOs). The callback gets every standard-ID data frame
  // with (can_id ^ id) & mask == 0, after CANopenNode has seen it. Can be called before or after setup().
  void add_frame_listener(uint32_t can_id, uint32_t mask, FrameCallback callback, void *context);
  // Sends a standard-ID data frame now. Returns false if the canbus platform refuses it (e.g. TX buffers full); it
  // isn't queued or retried, unlike CANopenNode's own frames.
  bool send_frame(uint32_t can_id, const uint8_t *data, uint8_t len);

#ifdef USE_CANOPEN_PDO
  // Our own PDOs (canopen.enable_pdo() in codegen). Before setup() only. The hub creates the communication and
  // mapping records 0x1400 / 0x1600 (RPDO) and 0x1800 / 0x1A00 (TPDO) for PDO 0..n; PDOs that aren't configured stay
  // disabled, and the records can be changed over SDO (CiA 301 order: disable, mapping, enable). COB-IDs are full CAN
  // IDs (CANopenNode only adds the node id to a bare predefined base, 0x180 / 0x200 / ..., of PDO 0-3). Mapped
  // objects need OD_TPDO / OD_RPDO; RPDO data reaches their handler (ObjectDictionary::set_object()). Event-driven
  // transmission types (254, 255); PDOs only run in OPERATIONAL. A PDO configured without mappings starts disabled.
  static constexpr uint8_t MAX_PDO = 32;
  bool configure_tpdo(uint8_t n, uint32_t cob_id, std::initializer_list<PdoMapping> mappings,
                      uint16_t event_timer_ms = 0, uint16_t inhibit_time_100us = 0, uint8_t transmission_type = 254);
  bool configure_rpdo(uint8_t n, uint32_t cob_id, std::initializer_list<PdoMapping> mappings,
                      uint16_t event_timer_ms = 0, uint8_t transmission_type = 254);
  // sends TPDO n at the next processing
  void request_tpdo(uint8_t n);
  // sends the TPDO that maps index:sub (an entry with a handler, sub < 32). CANopenNode has one request flag per
  // sub-entry: if several TPDOs map it, only the first one processed is sent.
  bool request_tpdo(uint16_t index, uint8_t sub);
#endif

#ifdef USE_CANOPEN_SDO_CLIENT
  // SDO client (one channel, requests run one after another in FIFO order). Every request ends in exactly one
  // callback: accepted ones from loop() / frame processing after the transfer, rejected ones (queue full, invalid
  // arguments, stack not running) synchronously before the call returns false. Requests to our own node id are
  // served from our object dictionary, without CAN frames.

  // value read; expected_size 1/2/4 checks the width (SIZE_MISMATCH), 0 accepts any width up to 4
  bool read_sdo(uint8_t node_id, uint16_t index, uint8_t sub, uint8_t expected_size, SDOCallback callback,
                void *context);
  template<typename T>
  bool read_sdo(uint8_t node_id, uint16_t index, uint8_t sub, SDOCallback callback, void *context) {
    return this->read_sdo(node_id, index, sub, sizeof(T), callback, context);
  }
  // value write of size 1/2/4 bytes (little endian)
  bool write_sdo(uint8_t node_id, uint16_t index, uint8_t sub, uint32_t value, uint8_t size,
                 SDOCallback callback = nullptr, void *context = nullptr);
  template<typename T>
  bool write_sdo(uint8_t node_id, uint16_t index, uint8_t sub, T value, SDOCallback callback = nullptr,
                 void *context = nullptr) {
    static_assert(sizeof(T) <= 4, "SDO values are at most 4 bytes");
    uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(T));
    return this->write_sdo(node_id, index, sub, raw, sizeof(T), callback, context);
  }
  // buffer transfers (strings, domains): segmented, block for more than 32 bytes; reads up to 1024 bytes.
  // write_sdo_data() doesn't copy: data must stay valid until the callback.
  bool read_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, SDOCallback callback, void *context);
  bool write_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, const uint8_t *data, size_t len,
                      SDOCallback callback = nullptr, void *context = nullptr);
  // Drops all queued requests of context and aborts its transfer in flight, without callbacks.
  void cancel_sdo(void *context);
#endif
#ifdef USE_CANOPEN_NMT_MASTER
  // NMT command to node_id (0 = all nodes; for our own id or 0 it also applies to us)
  bool send_nmt(uint8_t node_id, NMTCommand command);
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  // used by the CANopenNode driver (driver_can.cpp)
  canbus::Canbus *get_canbus() { return this->canbus_; }

 protected:
  void on_frame_(uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data);
  void process_();
#ifdef USE_CANOPEN_SDO_CLIENT
  enum class SDOKind : uint8_t { SDO_KIND_READ, SDO_KIND_WRITE, SDO_KIND_READ_DATA, SDO_KIND_WRITE_DATA };
  struct SDORequest {
    uint8_t node_id;
    uint16_t index;
    uint8_t sub;
    SDOKind kind;
    uint8_t size;  // read: expected size (0 = any); write: value size
    uint32_t value;
    const uint8_t *data;  // WRITE_DATA, owned by the caller
    size_t len;
    SDOCallback callback;
    void *context;
  };
  static constexpr size_t SDO_QUEUE_SIZE = 16;
  static constexpr size_t SDO_MAX_UPLOAD = 1024;
  bool queue_sdo_(SDORequest &&request);
  void process_sdo_(uint32_t dt_us);
  bool start_sdo_();
  void finish_sdo_(SDOResult result, uint32_t abort_code);
  void abort_sdo_();
#endif
  void check_frame_listener_(uint32_t can_id, uint32_t mask);
#ifdef USE_CANOPEN_PDO
  struct PdoParams {
    uint32_t cob_id{0x80000000};  // not valid
    uint8_t transmission_type{254};
    uint16_t inhibit_time{0};  // TPDO only, 100 us
    uint16_t event_timer{0};   // ms
    uint8_t map_count{0};
    std::array<uint32_t, 8> map{};
  };
  bool configure_pdo_(std::vector<PdoParams> &pdos, const LogString *kind, uint8_t n, uint32_t cob_id,
                      std::initializer_list<PdoMapping> mappings, uint16_t event_timer_ms, uint16_t inhibit_time_100us,
                      uint8_t transmission_type);
  void add_pdo_map_(uint16_t index, PdoParams &pdo);
#endif
  void node_event_(uint8_t node_id, NodeEvent event, NMTState state);

  static void on_hb_started(uint8_t node_id, uint8_t idx, void *object);
  static void on_hb_nmt_changed(uint8_t node_id, uint8_t idx, CO_NMT_internalState_t state, void *object);
  static void on_hb_timeout(uint8_t node_id, uint8_t idx, void *object);
  static void on_hb_remote_reset(uint8_t node_id, uint8_t idx, void *object);

  canbus::Canbus *canbus_{nullptr};
  uint8_t node_id_;
  CO_t *co_{nullptr};
  CO_config_t config_{};  // must outlive co_
  ObjectDictionary od_;

  // OD storage
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
  uint8_t err_reg_{0};             // 0x1001
  uint32_t cob_emcy_{0x80};        // 0x1014, CANopenNode adds the node id
  uint16_t hb_producer_ms_{5000};  // 0x1017
  uint8_t hb_cons_count_{0};       // 0x1016:0
  std::vector<uint32_t> hb_cons_;  // 0x1016:1.., (node_id << 16) | timeout_ms

  uint32_t last_us_{0};
  bool start_operational_{true};
  NMTState nmt_state_{NMTState::NMT_STATE_UNKNOWN};
  LazyCallbackManager<void(NMTState)> nmt_state_callback_;
  uint16_t response_timeout_ms_{500};
#ifdef USE_CANOPEN_SDO_CLIENT
  // OD 0x1280 (SDO client parameter), set up per request
  uint8_t sdo_cli_highest_sub_{3};
  uint32_t sdo_cli_cob_tx_{0x80000000};
  uint32_t sdo_cli_cob_rx_{0x80000000};
  uint8_t sdo_cli_node_{0};
  std::array<SDORequest, SDO_QUEUE_SIZE> sdo_queue_{};
  size_t sdo_head_{0};
  size_t sdo_count_{0};
  SDORequest sdo_active_{};
  bool sdo_busy_{false};
  size_t sdo_offset_{0};  // WRITE/WRITE_DATA: bytes handed to the client fifo
  // READ/READ_DATA: bytes received (also those beyond the buffer, for the size checks), kept in sdo_rx_value_ (READ)
  // or sdo_rx_buf_ (READ_DATA, SDO_MAX_UPLOAD bytes, allocated on the first buffer read and kept)
  size_t sdo_rx_len_{0};
  std::array<uint8_t, 4> sdo_rx_value_{};
  std::unique_ptr<uint8_t[]> sdo_rx_buf_;
  uint8_t sdo_abort_node_{0};  // node of the last cancelled transfer, see ABORT_GUARD_MS
  uint32_t sdo_abort_ms_{0};
#endif
#ifdef USE_CANOPEN_PDO
  // index = PDO number; the vectors are sized before setup() and not resized afterwards (the OD points into them)
  std::vector<PdoParams> tpdos_;
  std::vector<PdoParams> rpdos_;
  uint8_t tpdo_comm_highest_{5};
  uint8_t rpdo_comm_highest_{5};
#endif
  LazyCallbackManager<void(uint8_t, NodeEvent, NMTState)> node_event_callback_;
  struct FrameListener {
    uint32_t can_id;
    uint32_t mask;
    FrameCallback callback;
    void *context;
  };
  std::vector<FrameListener> frame_listeners_;
  LazyCallbackManager<void(uint8_t, NMTState)> heartbeat_callback_;
  LazyCallbackManager<void(uint8_t, const Emergency &)> emcy_callback_;
};

}  // namespace esphome::canopen
