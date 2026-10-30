#include "od.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen.od");

static OD_attr_t multi_byte(OD_attr_t attr, OD_size_t len) { return len >= 2 ? attr | ODA_MB : attr; }

void ObjectDictionary::add_entry_(uint16_t index, uint8_t sub_count, uint8_t type, void *obj) {
  if (this->finalized_) {
    ESP_LOGE(TAG, "OD 0x%04x added after init, ignored", index);
    return;
  }
  this->entries_.push_back(OD_entry_t{index, sub_count, type, obj, nullptr});
}

void ObjectDictionary::add_var(uint16_t index, OD_attr_t attr, void *data, OD_size_t len) {
  this->vars_.push_back(OD_obj_var_t{data, multi_byte(attr, len), len});
  this->add_entry_(index, 1, ODT_VAR, &this->vars_.back());
}

void ObjectDictionary::add_array(uint16_t index, OD_attr_t attr, uint8_t *count, void *data, OD_size_t elem_len,
                                 uint8_t n) {
  this->arrays_.push_back(OD_obj_array_t{count, data, ODA_SDO_R, multi_byte(attr, elem_len), elem_len, elem_len});
  this->add_entry_(index, n + 1, ODT_ARR, &this->arrays_.back());
}

void ObjectDictionary::add_record(uint16_t index, std::initializer_list<RecordSub> subs) {
  auto &record = this->records_.emplace_back();
  record.reserve(subs.size());
  for (const auto &s : subs)
    record.push_back(OD_obj_record_t{s.data, s.sub, multi_byte(s.attr, s.len), s.len});
  this->add_entry_(index, record.size(), ODT_REC, record.data());
}

void ObjectDictionary::add_string(uint16_t index, const char *str) {
  OD_size_t len = std::strlen(str);
  // no ODA_MB: a string has no byte order
  this->vars_.push_back(OD_obj_var_t{const_cast<char *>(str), ODA_SDO_R, len > 0 ? len : 1});
  this->add_entry_(index, 1, ODT_VAR, &this->vars_.back());
}

void ObjectDictionary::set_extension_(uint16_t index, OD_extension_t *extension) {
  for (auto &entry : this->entries_) {
    if (entry.index == index) {
      entry.extension = extension;
      return;
    }
  }
  ESP_LOGE(TAG, "OD 0x%04x: handler for a missing entry, ignored", index);
}

OdResult ObjectDictionary::write(uint16_t index, uint8_t sub, const void *data, size_t len) {
  if (!this->finalized_)
    return OdResult::OD_RESULT_DEVICE_STATE;
  OD_IO_t io;
  auto ret = static_cast<OdResult>(OD_getSub(OD_find(&this->od_, index), sub, &io, false));
  if (ret != OdResult::OD_RESULT_OK)
    return ret;
  if ((io.stream.attribute & ODA_SDO_W) == 0)
    return OdResult::OD_RESULT_READ_ONLY;
  if (io.stream.dataLength == 0) {
    // no fixed size (domain): the whole write, as an SDO client indicates it
    io.stream.dataLength = len;
  } else if (len > io.stream.dataLength) {
    return OdResult::OD_RESULT_DATA_LONG;
  } else if (len < io.stream.dataLength) {
    return OdResult::OD_RESULT_DATA_SHORT;
  }
  OD_size_t written = 0;
  ret = static_cast<OdResult>(io.write(&io.stream, data, len, &written));
  // all data was given: a handler asking for more means the value was too short
  return ret == OdResult::OD_RESULT_PARTIAL ? OdResult::OD_RESULT_DATA_SHORT : ret;
}

bool ObjectDictionary::finalize() {
  std::sort(this->entries_.begin(), this->entries_.end(),
            [](const OD_entry_t &a, const OD_entry_t &b) { return a.index < b.index; });
  bool ok = true;
  for (size_t i = 1; i < this->entries_.size(); i++) {
    if (this->entries_[i].index == this->entries_[i - 1].index) {
      ESP_LOGE(TAG, "OD 0x%04x added twice", this->entries_[i].index);
      ok = false;
    }
  }
  this->od_.size = this->entries_.size();
  // OD_find() expects a blank last element
  this->entries_.push_back(OD_entry_t{0, 0, 0, nullptr, nullptr});
  this->od_.list = this->entries_.data();
  this->finalized_ = true;
  return ok;
}

}  // namespace esphome::canopen
