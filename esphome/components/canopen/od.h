#pragma once

#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <vector>

#define OD_DEFINITION
#include "CANopen.h"

namespace esphome::canopen {

// Attributes of OD entries (CANopenNode's ODA_* values)
constexpr uint8_t OD_READ = ODA_SDO_R;
constexpr uint8_t OD_WRITE = ODA_SDO_W;
constexpr uint8_t OD_READ_WRITE = ODA_SDO_RW;
// mappable to a TPDO / RPDO
constexpr uint8_t OD_TPDO = ODA_TPDO;
constexpr uint8_t OD_RPDO = ODA_RPDO;

// Result of an OD access; anything but OK / PARTIAL becomes the matching SDO abort code. Same values as CANopenNode's
// ODR_t, so the conversion is free.
enum class OdResult : int8_t {
  OD_RESULT_PARTIAL = ODR_PARTIAL,  // more read / write calls follow
  OD_RESULT_OK = ODR_OK,
  OD_RESULT_OUT_OF_MEMORY = ODR_OUT_OF_MEM,
  OD_RESULT_UNSUPPORTED_ACCESS = ODR_UNSUPP_ACCESS,
  OD_RESULT_WRITE_ONLY = ODR_WRITEONLY,
  OD_RESULT_READ_ONLY = ODR_READONLY,
  OD_RESULT_NOT_EXIST = ODR_IDX_NOT_EXIST,
  OD_RESULT_DEVICE_INCOMPATIBLE = ODR_DEV_INCOMPAT,
  OD_RESULT_HARDWARE = ODR_HW,
  OD_RESULT_TYPE_MISMATCH = ODR_TYPE_MISMATCH,
  OD_RESULT_DATA_LONG = ODR_DATA_LONG,
  OD_RESULT_DATA_SHORT = ODR_DATA_SHORT,
  OD_RESULT_SUB_NOT_EXIST = ODR_SUB_NOT_EXIST,
  OD_RESULT_INVALID_VALUE = ODR_INVALID_VALUE,
  OD_RESULT_VALUE_HIGH = ODR_VALUE_HIGH,
  OD_RESULT_VALUE_LOW = ODR_VALUE_LOW,
  OD_RESULT_GENERAL = ODR_GENERAL,
  OD_RESULT_DATA_TRANSFER = ODR_DATA_TRANSF,
  OD_RESULT_DEVICE_STATE = ODR_DATA_DEV_STATE,
  OD_RESULT_NO_DATA = ODR_NO_DATA,
};

// One read or write of an OD sub-entry, in chunks for large data (a view of CANopenNode's OD_stream_t).
class OdStream {
 public:
  explicit OdStream(OD_stream_t *stream) : stream_(stream) {}

  uint16_t index() const { return this->stream_->index; }
  uint8_t sub() const { return this->stream_->subIndex; }
  // bytes already transferred; a handler returning PARTIAL advances it
  size_t offset() const { return this->stream_->dataOffset; }
  void set_offset(size_t offset) { this->stream_->dataOffset = offset; }
  // total size: the entry's length. For an entry without a fixed size (a domain) it is 0 during a download until the
  // last chunk, then the total (CANopenNode's end of transfer indication: the size the client indicated isn't passed
  // on). A read handler sets it on the first call (offset 0) to indicate the upload size.
  size_t size() const { return this->stream_->dataLength; }
  void set_size(size_t size) { this->stream_->dataLength = size; }
  // the entry's storage (nullptr for entries without one)
  void *data() const { return this->stream_->dataOrig; }

  // the default access: copy from / to the entry's storage
  OdResult read_original(void *buf, size_t count, size_t *done) {
    OD_size_t n = 0;
    auto ret = static_cast<OdResult>(OD_readOriginal(this->stream_, buf, count, &n));
    *done = n;
    return ret;
  }
  OdResult write_original(const void *buf, size_t count, size_t *done) {
    OD_size_t n = 0;
    auto ret = static_cast<OdResult>(OD_writeOriginal(this->stream_, buf, count, &n));
    *done = n;
    return ret;
  }

 protected:
  OD_stream_t *stream_;
};

// Base class of OD handlers (ObjectDictionary::set_object()). Handlers define od_read() / od_write() with these
// signatures; they are called directly (no virtual functions), and the defaults access the entry's storage. Called for
// SDO transfers (also local ones), write_od() and, with PDOs, for mapped entries. The SDO server checks the entry's
// attributes before calling them.
struct OdObject {
  OdResult od_read(OdStream stream, void *buf, size_t count, size_t *done) {
    return stream.read_original(buf, count, done);
  }
  OdResult od_write(OdStream stream, const void *buf, size_t count, size_t *done) {
    return stream.write_original(buf, count, done);
  }
};

// Object dictionary built at runtime (CO_MULTIPLE_OD). Entries are collected with add_*() and turned into the sorted
// OD_entry_t list by finalize(). The value storage is owned by the caller and must outlive the OD. All allocation
// happens before finalize() (in setup): the containers only grow until then, and entries refer to their objects by
// position, so the pointers CANopenNode gets are taken once they no longer move.
class ObjectDictionary {
 public:
  struct RecordSub {
    uint8_t sub;
    OD_attr_t attr;
    void *data;
    OD_size_t len;
  };

  void add_var(uint16_t index, OD_attr_t attr, void *data, OD_size_t len);
  // ARRAY: sub0 = *count (read only), subs 1..n = elements of elem_len bytes at data
  void add_array(uint16_t index, OD_attr_t attr, uint8_t *count, void *data, OD_size_t elem_len, uint8_t n);
  // RECORD: subs are looked up by subindex, so sub0 (highest subindex, uint8_t) is passed like the others
  void add_record(uint16_t index, std::initializer_list<RecordSub> subs) {
    this->add_record(index, subs.begin(), subs.size());
  }
  void add_record(uint16_t index, const RecordSub *subs, size_t count);
  // read-only VISIBLE_STRING VAR. Without ODA_STR the SDO server indicates the exact size (short strings go
  // expedited). An empty string is one zero byte: CANopenNode aborts zero-length uploads.
  void add_string(uint16_t index, const char *str);

  // Calls handler->od_read() / od_write() (an OdObject subclass) for every access to an entry added before.
  template<typename T> void set_object(uint16_t index, T *handler) {
    static_assert(std::is_base_of_v<OdObject, T>, "OD handlers derive from canopen::OdObject");
    this->set_extension_(index, OD_extension_t{handler, &read_trampoline<T>, &write_trampoline<T>, {}});
  }

  // Builds the entry list. Entries can't be added after this. False if an index was added twice.
  bool finalize();
  bool finalized() const { return this->finalized_; }

  OD_entry_t *find(uint16_t index) { return OD_find(&this->od_, index); }
  OD_t *od() { return &this->od_; }

  // Writes a value through the entry's handler, as an SDO download does. Needs write access. After finalize().
  OdResult write(uint16_t index, uint8_t sub, const void *data, size_t len);

 protected:
  // an entry until finalize(): its object and extension by position in vars_ / arrays_ / records_ / extensions_
  struct PendingEntry {
    uint16_t index;
    uint8_t sub_count;
    uint8_t type;
    uint16_t object;
    uint16_t extension;  // position + 1, 0 = none
  };

  bool can_add_(uint16_t index) const;
  void set_extension_(uint16_t index, const OD_extension_t &extension);

  template<typename T>
  static ODR_t read_trampoline(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *count_read) {
    size_t done = 0;
    auto ret = static_cast<T *>(stream->object)->od_read(OdStream(stream), buf, count, &done);
    *count_read = done;
    return static_cast<ODR_t>(ret);
  }
  template<typename T>
  static ODR_t write_trampoline(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *count_written) {
    size_t done = 0;
    auto ret = static_cast<T *>(stream->object)->od_write(OdStream(stream), buf, count, &done);
    *count_written = done;
    return static_cast<ODR_t>(ret);
  }

  std::vector<PendingEntry> pending_;  // released by finalize()
  std::vector<OD_obj_var_t> vars_;
  std::vector<OD_obj_array_t> arrays_;
  std::vector<OD_obj_record_t> records_;  // the subs of all records, each record's in a row
  std::vector<OD_extension_t> extensions_;
  std::vector<OD_entry_t> entries_;
  OD_t od_{0, nullptr};
  bool finalized_{false};
};

}  // namespace esphome::canopen
