#include "canopen.h"

#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

const LogString *nmt_state_to_string(NMTState state) {
  switch (state) {
    case NMTState::NMT_STATE_BOOTUP:
      return LOG_STR("bootup");
    case NMTState::NMT_STATE_STOPPED:
      return LOG_STR("stopped");
    case NMTState::NMT_STATE_OPERATIONAL:
      return LOG_STR("operational");
    case NMTState::NMT_STATE_PRE_OPERATIONAL:
      return LOG_STR("pre-operational");
    default:
      return LOG_STR("unknown");
  }
}

const LogString *node_event_to_string(NodeEvent event) {
  switch (event) {
    case NodeEvent::NODE_EVENT_ONLINE:
      return LOG_STR("online");
    case NodeEvent::NODE_EVENT_OFFLINE:
      return LOG_STR("offline");
    case NodeEvent::NODE_EVENT_NMT_CHANGED:
      return LOG_STR("NMT changed");
    case NodeEvent::NODE_EVENT_RESET:
      return LOG_STR("reset");
    default:
      return LOG_STR("unknown");
  }
}

static NMTState to_nmt_state(CO_NMT_internalState_t state) {
  switch (state) {
    case CO_NMT_INITIALIZING:
      return NMTState::NMT_STATE_BOOTUP;
    case CO_NMT_STOPPED:
      return NMTState::NMT_STATE_STOPPED;
    case CO_NMT_OPERATIONAL:
      return NMTState::NMT_STATE_OPERATIONAL;
    case CO_NMT_PRE_OPERATIONAL:
      return NMTState::NMT_STATE_PRE_OPERATIONAL;
    default:
      return NMTState::NMT_STATE_UNKNOWN;
  }
}

void CANopenComponent::monitor_node(uint8_t node_id, uint16_t timeout_ms) {
  if (this->co_ != nullptr) {
    ESP_LOGE(TAG, "monitor_node(0x%02x) after setup, ignored", node_id);
    return;
  }
  // CANopenNode rejects a node id that is in 0x1016 twice
  for (uint32_t &c : this->hb_cons_) {
    if ((c >> 16) == node_id) {
      if (timeout_ms < (c & 0xFFFF))
        c = ((uint32_t) node_id << 16) | timeout_ms;
      return;
    }
  }
  this->hb_cons_.push_back(((uint32_t) node_id << 16) | timeout_ms);
}

NMTState CANopenComponent::get_node_state(uint8_t node_id) {
  if (this->co_ == nullptr || this->hb_cons_.empty())
    return NMTState::NMT_STATE_UNKNOWN;
  int8_t idx = CO_HBconsumer_getIdxByNodeId(this->co_->HBcons, node_id);
  CO_NMT_internalState_t state;
  if (idx < 0 || CO_HBconsumer_getNmtState(this->co_->HBcons, idx, &state) != 0)
    return NMTState::NMT_STATE_UNKNOWN;
  return to_nmt_state(state);
}

bool CANopenComponent::is_node_online(uint8_t node_id) {
  if (this->co_ == nullptr || this->hb_cons_.empty())
    return false;
  int8_t idx = CO_HBconsumer_getIdxByNodeId(this->co_->HBcons, node_id);
  return idx >= 0 && CO_HBconsumer_getState(this->co_->HBcons, idx) == CO_HBconsumer_ACTIVE;
}

void CANopenComponent::set_canbus(canbus::Canbus *canbus) {
  this->canbus_ = canbus;
  canbus->add_callback([this](uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {
    this->on_frame_(can_id, extended_id, rtr, data);
  });
}

void CANopenComponent::setup() {
  auto &od = this->od_;
  od.add_var(0x1000, ODA_SDO_R, &this->dev_type_, sizeof(this->dev_type_));
  od.add_var(0x1001, ODA_SDO_R, &this->err_reg_, sizeof(this->err_reg_));
  if (this->device_name_ == nullptr)
    this->device_name_ = App.get_name().c_str();
  if (this->software_version_ == nullptr)
    this->software_version_ = ESPHOME_VERSION;
  od.add_string(0x1008, this->device_name_);
  if (this->hardware_version_ != nullptr)
    od.add_string(0x1009, this->hardware_version_);
  od.add_string(0x100A, this->software_version_);
  od.add_var(0x1014, ODA_SDO_RW, &this->cob_emcy_, sizeof(this->cob_emcy_));
  od.add_var(0x1017, ODA_SDO_RW, &this->hb_producer_ms_, sizeof(this->hb_producer_ms_));
  uint8_t n_cons = this->hb_cons_.size();
  if (n_cons) {
    this->hb_cons_count_ = n_cons;
    od.add_array(0x1016, ODA_SDO_RW, &this->hb_cons_count_, this->hb_cons_.data(), sizeof(uint32_t), n_cons);
  }
  od.add_record(0x1018, {{0, ODA_SDO_R, &this->identity_highest_sub_, 1},
                         {1, ODA_SDO_R, &this->identity_[0], 4},
                         {2, ODA_SDO_R, &this->identity_[1], 4},
                         {3, ODA_SDO_R, &this->identity_[2], 4},
                         {4, ODA_SDO_R, &this->identity_[3], 4}});
  od.add_record(0x1200, {{0, ODA_SDO_R, &this->sdo_srv_highest_sub_, 1},
                         {1, ODA_SDO_R, &this->sdo_srv_cob_rx_, 4},
                         {2, ODA_SDO_R, &this->sdo_srv_cob_tx_, 4}});
#ifdef USE_CANOPEN_SDO_CLIENT
  od.add_record(0x1280, {{0, ODA_SDO_R, &this->sdo_cli_highest_sub_, 1},
                         {1, ODA_SDO_RW, &this->sdo_cli_cob_tx_, 4},
                         {2, ODA_SDO_RW, &this->sdo_cli_cob_rx_, 4},
                         {3, ODA_SDO_RW, &this->sdo_cli_node_, 1}});
#endif
#ifdef USE_CANOPEN_PDO
  for (size_t n = 0; n < this->rpdos_.size(); n++) {
    PdoParams &p = this->rpdos_[n];
    od.add_record(0x1400 + n, {{0, ODA_SDO_R, &this->rpdo_comm_highest_, 1},
                               {1, ODA_SDO_RW, &p.cob_id, 4},
                               {2, ODA_SDO_RW, &p.transmission_type, 1},
                               {5, ODA_SDO_RW, &p.event_timer, 2}});
    this->add_pdo_map_(0x1600 + n, p);
  }
  for (size_t n = 0; n < this->tpdos_.size(); n++) {
    PdoParams &p = this->tpdos_[n];
    od.add_record(0x1800 + n, {{0, ODA_SDO_R, &this->tpdo_comm_highest_, 1},
                               {1, ODA_SDO_RW, &p.cob_id, 4},
                               {2, ODA_SDO_RW, &p.transmission_type, 1},
                               {3, ODA_SDO_RW, &p.inhibit_time, 2},
                               {5, ODA_SDO_RW, &p.event_timer, 2}});
    this->add_pdo_map_(0x1A00 + n, p);
  }
#endif
  if (!od.finalize()) {
    this->mark_failed(LOG_STR("invalid object dictionary"));
    return;
  }

  auto &cfg = this->config_;
  cfg.CNT_NMT = 1;
  cfg.ENTRY_H1017 = od.find(0x1017);
  cfg.CNT_EM = 1;
  cfg.ENTRY_H1001 = od.find(0x1001);
  cfg.ENTRY_H1014 = od.find(0x1014);
  cfg.CNT_ARR_1003 = 8;  // EM fifo size, needs >= 1
  cfg.CNT_HB_CONS = n_cons ? 1 : 0;
  cfg.CNT_ARR_1016 = n_cons;
  cfg.ENTRY_H1016 = od.find(0x1016);
#ifdef USE_CANOPEN_PDO
  // CO_CANopenInitPDO() steps from the first record: 0x1400.. / 0x1600.. / 0x1800.. / 0x1A00.. are contiguous
  cfg.CNT_RPDO = this->rpdos_.size();
  cfg.ENTRY_H1400 = od.find(0x1400);
  cfg.ENTRY_H1600 = od.find(0x1600);
  cfg.CNT_TPDO = this->tpdos_.size();
  cfg.ENTRY_H1800 = od.find(0x1800);
  cfg.ENTRY_H1A00 = od.find(0x1A00);
#endif
  // the default SDO server (CiA 301: mandatory)
  cfg.CNT_SDO_SRV = 1;
  cfg.ENTRY_H1200 = od.find(0x1200);
#ifdef USE_CANOPEN_SDO_CLIENT
  cfg.CNT_SDO_CLI = 1;
  cfg.ENTRY_H1280 = od.find(0x1280);
#endif

  uint32_t heap = 0;
  this->co_ = CO_new(&cfg, &heap);
  if (this->co_ == nullptr) {
    this->mark_failed(LOG_STR("CO_new failed"));
    return;
  }
  CO_ReturnError_t err = CO_CANinit(this->co_, this, 0);
  if (err != CO_ERROR_NO) {
    ESP_LOGE(TAG, "CO_CANinit failed: %d", err);
    this->mark_failed();
    return;
  }
  uint32_t err_info = 0;
  uint16_t nmt_control = this->start_operational_ ? CO_NMT_STARTUP_TO_OPERATIONAL : 0;
  err = CO_CANopenInit(this->co_, nullptr, nullptr, od.od(), nullptr, nmt_control, 500 /* first HB, ms */, 1000, 0,
                       false, this->node_id_, &err_info);
  if (err != CO_ERROR_NO) {
    ESP_LOGE(TAG, "CO_CANopenInit failed: %d (OD 0x%04" PRIx32 ")", err, err_info);
    this->mark_failed();
    return;
  }

#ifdef USE_CANOPEN_PDO
  if (!this->rpdos_.empty() || !this->tpdos_.empty()) {
    err = CO_CANopenInitPDO(this->co_, this->co_->em, od.od(), this->node_id_, &err_info);
    if (err != CO_ERROR_NO) {
      ESP_LOGE(TAG, "CO_CANopenInitPDO failed: %d (OD 0x%04" PRIx32 ")", err, err_info);
      this->mark_failed();
      return;
    }
  }
#endif
  for (uint8_t i = 0; i < n_cons; i++) {
    CO_HBconsumer_t *hb = this->co_->HBcons;
    CO_HBconsumer_initCallbackHeartbeatStarted(hb, i, this, on_hb_started);
    CO_HBconsumer_initCallbackNmtChanged(hb, i, this, on_hb_nmt_changed);
    CO_HBconsumer_initCallbackTimeout(hb, i, this, on_hb_timeout);
    CO_HBconsumer_initCallbackRemoteReset(hb, i, this, on_hb_remote_reset);
  }

  for (const auto &l : this->frame_listeners_)
    this->check_frame_listener_(l.can_id, l.mask);

  CO_CANsetNormalMode(this->co_->CANmodule);
  this->last_us_ = micros();
  ESP_LOGI(TAG, "CANopenNode initialized, node 0x%02x, heap %" PRIu32 " B", this->node_id_, heap);
}

void CANopenComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CANopen (CANopenNode):");
  ESP_LOGCONFIG(TAG, "  Node ID: 0x%02x", this->node_id_);
  ESP_LOGCONFIG(TAG, "  Heartbeat interval: %u ms", this->hb_producer_ms_);
#ifdef CANOPEN_SDO_BLOCK_SIZE
  ESP_LOGCONFIG(TAG, "  SDO server: block transfer, download block size %u", CANOPEN_SDO_BLOCK_SIZE);
#endif
  ESP_LOGCONFIG(TAG, "  NMT startup: %s",
                this->start_operational_ ? LOG_STR_LITERAL("operational") : LOG_STR_LITERAL("pre-operational"));
  ESP_LOGCONFIG(
      TAG, "  Identity: vendor 0x%08" PRIx32 ", product 0x%08" PRIx32 ", revision 0x%08" PRIx32 ", serial 0x%08" PRIx32,
      this->identity_[0], this->identity_[1], this->identity_[2], this->identity_[3]);
  ESP_LOGCONFIG(TAG, "  Device name: %s, software version: %s", this->device_name_, this->software_version_);
  if (this->hardware_version_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Hardware version: %s", this->hardware_version_);
  }
#ifdef USE_CANOPEN_SDO_CLIENT
  ESP_LOGCONFIG(TAG, "  SDO client: response timeout %u ms", this->response_timeout_ms_);
#endif
  for (uint32_t c : this->hb_cons_) {
    ESP_LOGCONFIG(TAG, "  Heartbeat client: node 0x%02x, timeout %u ms", (unsigned) (c >> 16), (unsigned) (c & 0xFFFF));
  }
}

void CANopenComponent::add_frame_listener(uint32_t can_id, uint32_t mask, FrameCallback callback, void *context) {
  this->frame_listeners_.push_back(FrameListener{can_id & 0x7FF, mask & 0x7FF, callback, context});
  if (this->co_ != nullptr)
    this->check_frame_listener_(can_id & 0x7FF, mask & 0x7FF);
}

// A listener on an ID CANopenNode receives itself (NMT, heartbeats it consumes, ...) is almost always a mistake.
void CANopenComponent::check_frame_listener_(uint32_t can_id, uint32_t mask) {
  CO_CANmodule_t *mod = this->co_->CANmodule;
  for (uint16_t i = 0; i < mod->rxSize; i++) {
    const CO_CANrx_t *rx = &mod->rxArray[i];
    if (rx->CANrx_callback == nullptr || (rx->ident & 0x800))
      continue;
    if (((can_id ^ rx->ident) & mask & rx->mask & 0x7FF) == 0) {
      ESP_LOGW(TAG, "frame listener 0x%03" PRIx32 "/0x%03" PRIx32 " overlaps CAN ID 0x%03x used by CANopenNode", can_id,
               mask, rx->ident & 0x7FF);
    }
  }
}

bool CANopenComponent::send_frame(uint32_t can_id, const uint8_t *data, uint8_t len) {
  if (this->canbus_ == nullptr || len > 8)
    return false;
  return this->canbus_->send_data(can_id & 0x7FF, false, false, data, len) == canbus::ERROR_OK;
}

void CANopenComponent::on_frame_(uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {
  if (extended_id || this->co_ == nullptr || this->is_failed())
    return;
  CO_CANmodule_t *mod = this->co_->CANmodule;
  CO_CANrxMsg_t msg{};
  msg.ident = (can_id & 0x7FF) | (rtr ? 0x800 : 0);
  msg.DLC = data.size() > 8 ? 8 : data.size();
  memcpy(msg.data, data.data(), msg.DLC);
  for (uint16_t i = 0; i < mod->rxSize; i++) {
    CO_CANrx_t *rx = &mod->rxArray[i];
    if (rx->CANrx_callback != nullptr && ((msg.ident ^ rx->ident) & rx->mask) == 0)
      rx->CANrx_callback(rx->object, &msg);
  }
  if (!rtr) {
    for (const auto &l : this->frame_listeners_) {
      if (((msg.ident ^ l.can_id) & l.mask) == 0)
        l.callback(l.context, msg.ident, msg.data, msg.DLC);
    }
    uint8_t node;
    NMTState state;
    Emergency emcy;
    if (decode_heartbeat(msg.ident, msg.data, msg.DLC, node, state)) {
      this->heartbeat_callback_.call(node, state);
    } else if (decode_emcy(msg.ident, msg.data, msg.DLC, node, emcy)) {
      ESP_LOGW(TAG, "node 0x%02x EMCY: error code 0x%04x, register 0x%02x", node, emcy.error_code, emcy.error_register);
      this->emcy_callback_.call(node, emcy);
    }
  }
  this->process_();
}

void CANopenComponent::loop() { this->process_(); }

void CANopenComponent::process_() {
  if (this->co_ == nullptr || this->is_failed())
    return;
  uint32_t now = micros();
  uint32_t dt = now - this->last_us_;
  this->last_us_ = now;
  CO_NMT_reset_cmd_t reset = CO_process(this->co_, false, dt, nullptr);
  if (reset == CO_RESET_APP) {
    ESP_LOGW(TAG, "NMT reset node");
    App.safe_reboot();
  } else if (reset == CO_RESET_COMM) {
    ESP_LOGW(TAG, "NMT reset communication (not implemented)");
  }
#ifdef USE_CANOPEN_PDO
  // no SYNC: event-driven PDOs only
  CO_process_RPDO(this->co_, false, dt, nullptr);
  CO_process_TPDO(this->co_, false, dt, nullptr);
#endif
  // CANopenNode's state change callback has no context pointer, so the state is compared here
  NMTState state = to_nmt_state(CO_NMT_getInternalState(this->co_->NMT));
  if (state != this->nmt_state_) {
    this->nmt_state_ = state;
    ESP_LOGI(TAG, "NMT state: %s", LOG_STR_ARG(nmt_state_to_string(state)));
    this->nmt_state_callback_.call(state);
  }
#ifdef USE_CANOPEN_SDO_CLIENT
  this->process_sdo_(dt);
#endif
}

#ifdef USE_CANOPEN_PDO
bool CANopenComponent::configure_tpdo(uint8_t n, uint32_t cob_id, std::initializer_list<PdoMapping> mappings,
                                      uint16_t event_timer_ms, uint16_t inhibit_time_100us, uint8_t transmission_type) {
  return this->configure_pdo_(this->tpdos_, LOG_STR("TPDO"), n, cob_id, mappings, event_timer_ms, inhibit_time_100us,
                              transmission_type);
}

bool CANopenComponent::configure_rpdo(uint8_t n, uint32_t cob_id, std::initializer_list<PdoMapping> mappings,
                                      uint16_t event_timer_ms, uint8_t transmission_type) {
  return this->configure_pdo_(this->rpdos_, LOG_STR("RPDO"), n, cob_id, mappings, event_timer_ms, 0, transmission_type);
}

bool CANopenComponent::configure_pdo_(std::vector<PdoParams> &pdos, const LogString *kind, uint8_t n, uint32_t cob_id,
                                      std::initializer_list<PdoMapping> mappings, uint16_t event_timer_ms,
                                      uint16_t inhibit_time_100us, uint8_t transmission_type) {
  unsigned bits = 0;
  bool aligned = true;
  for (const auto &m : mappings) {
    bits += m.bits;
    // no CO_CONFIG_PDO_BITWISE_MAPPING: whole bytes
    aligned = aligned && m.bits > 0 && (m.bits % 8) == 0;
  }
  if (this->od_.finalized() || n >= MAX_PDO || mappings.size() > 8 || bits > 64 || !aligned || (cob_id & 0x7FF) == 0) {
    ESP_LOGE(TAG, "%s %u: invalid configuration (or after setup), ignored", LOG_STR_ARG(kind), n);
    return false;
  }
  if (pdos.size() <= n)
    pdos.resize(n + 1);
  PdoParams &p = pdos[n];
  // without mappings the PDO would be invalid (CANopenNode sends an EMCY): disabled, for the host to map later
  p.cob_id = (cob_id & 0x7FF) | (mappings.size() == 0 ? 0x80000000 : 0);
  p.transmission_type = transmission_type;
  p.inhibit_time = inhibit_time_100us;
  p.event_timer = event_timer_ms;
  p.map_count = mappings.size();
  size_t i = 0;
  for (const auto &m : mappings)
    p.map[i++] = (uint32_t(m.index) << 16) | (uint32_t(m.sub) << 8) | m.bits;
  return true;
}

void CANopenComponent::add_pdo_map_(uint16_t index, PdoParams &p) {
  this->od_.add_record(index, {{0, ODA_SDO_RW, &p.map_count, 1},
                               {1, ODA_SDO_RW, &p.map[0], 4},
                               {2, ODA_SDO_RW, &p.map[1], 4},
                               {3, ODA_SDO_RW, &p.map[2], 4},
                               {4, ODA_SDO_RW, &p.map[3], 4},
                               {5, ODA_SDO_RW, &p.map[4], 4},
                               {6, ODA_SDO_RW, &p.map[5], 4},
                               {7, ODA_SDO_RW, &p.map[6], 4},
                               {8, ODA_SDO_RW, &p.map[7], 4}});
}

void CANopenComponent::request_tpdo(uint8_t n) {
  if (this->co_ != nullptr && n < this->tpdos_.size())
    CO_TPDOsendRequest(&this->co_->TPDO[n]);
}

bool CANopenComponent::request_tpdo(uint16_t index, uint8_t sub) {
  OD_entry_t *entry = this->od_.finalized() ? this->od_.find(index) : nullptr;
  // the request flags live in the entry's extension (its handler)
  if (entry == nullptr || entry->extension == nullptr || sub >= OD_FLAGS_PDO_SIZE * 8)
    return false;
  OD_requestTPDO(entry, sub);
  return true;
}
#endif

bool CANopenComponent::set_nmt_state(NMTState state) {
  if (this->co_ == nullptr || this->is_failed())
    return false;
  CO_NMT_command_t command;
  switch (state) {
    case NMTState::NMT_STATE_OPERATIONAL:
      command = CO_NMT_ENTER_OPERATIONAL;
      break;
    case NMTState::NMT_STATE_PRE_OPERATIONAL:
      command = CO_NMT_ENTER_PRE_OPERATIONAL;
      break;
    case NMTState::NMT_STATE_STOPPED:
      command = CO_NMT_ENTER_STOPPED;
      break;
    default:
      return false;
  }
  // applied by the next CO_process()
  CO_NMT_sendInternalCommand(this->co_->NMT, command);
  return true;
}

#ifdef USE_CANOPEN_NMT_MASTER
bool CANopenComponent::send_nmt(uint8_t node_id, NMTCommand command) {
  if (this->co_ == nullptr || this->is_failed() || node_id > 127)
    return false;
  ESP_LOGD(TAG, "NMT command %u to node 0x%02x", (unsigned) command, node_id);
  return CO_NMT_sendCommand(this->co_->NMT, static_cast<CO_NMT_command_t>(command), node_id) == CO_ERROR_NO;
}
#endif

void CANopenComponent::node_event_(uint8_t node_id, NodeEvent event, NMTState state) {
  if (event == NodeEvent::NODE_EVENT_OFFLINE) {
    ESP_LOGW(TAG, "node 0x%02x offline (heartbeat timeout)", node_id);
  } else {
    ESP_LOGI(TAG, "node 0x%02x %s, NMT state: %s", node_id, LOG_STR_ARG(node_event_to_string(event)),
             LOG_STR_ARG(nmt_state_to_string(state)));
  }
  this->node_event_callback_.call(node_id, event, state);
}

// CANopenNode heartbeat consumer callbacks (called from CO_process())
void CANopenComponent::on_hb_started(uint8_t node_id, uint8_t idx, void *object) {
  auto *self = static_cast<CANopenComponent *>(object);
  // the heartbeat's state is already stored, but CO_HBconsumer_getNmtState() only returns it once HBstate is ACTIVE,
  // which CANopenNode sets after this callback
  CO_NMT_internalState_t state = self->co_->HBcons->monitoredNodes[idx].NMTstate;
  self->node_event_(node_id, NodeEvent::NODE_EVENT_ONLINE, to_nmt_state(state));
}

void CANopenComponent::on_hb_nmt_changed(uint8_t node_id, uint8_t idx, CO_NMT_internalState_t state, void *object) {
  static_cast<CANopenComponent *>(object)->node_event_(node_id, NodeEvent::NODE_EVENT_NMT_CHANGED, to_nmt_state(state));
}

void CANopenComponent::on_hb_timeout(uint8_t node_id, uint8_t idx, void *object) {
  static_cast<CANopenComponent *>(object)->node_event_(node_id, NodeEvent::NODE_EVENT_OFFLINE,
                                                       NMTState::NMT_STATE_UNKNOWN);
}

void CANopenComponent::on_hb_remote_reset(uint8_t node_id, uint8_t idx, void *object) {
  static_cast<CANopenComponent *>(object)->node_event_(node_id, NodeEvent::NODE_EVENT_RESET,
                                                       NMTState::NMT_STATE_BOOTUP);
}

}  // namespace esphome::canopen
