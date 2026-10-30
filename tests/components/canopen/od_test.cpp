#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

#include "esphome/components/canopen/od.h"

namespace esphome::canopen::testing {

// upload a whole VAR through CANopenNode's OD interface, as the SDO server does
static std::string read_var(ObjectDictionary &od, uint16_t index, OD_attr_t *attr = nullptr) {
  OD_IO_t io;
  EXPECT_EQ(OD_getSub(od.find(index), 0, &io, false), ODR_OK);
  if (attr != nullptr)
    *attr = io.stream.attribute;
  char buf[64];
  OD_size_t n = 0;
  EXPECT_EQ(io.read(&io.stream, buf, sizeof(buf), &n), ODR_OK);
  return std::string(buf, n);
}

TEST(CanopenOD, Strings) {
  static const char name[] = "canopen test node";
  static const char empty[] = "";
  ObjectDictionary od;
  // added out of order: finalize() sorts
  od.add_string(0x100A, "2026.11.0");
  od.add_string(0x1008, name);
  od.add_string(0x1009, empty);
  od.finalize();

  OD_attr_t attr = 0;
  EXPECT_EQ(read_var(od, 0x1008, &attr), name);
  // read only, no ODA_STR (size known up front) and no ODA_MB (no byte order)
  EXPECT_EQ(attr, ODA_SDO_R);
  EXPECT_EQ(read_var(od, 0x100A), "2026.11.0");
  // an empty string is one zero byte: CANopenNode aborts zero-length uploads
  EXPECT_EQ(read_var(od, 0x1009), std::string(1, '\0'));
  EXPECT_EQ(od.find(0x1007), nullptr);
}

TEST(CanopenOD, Record) {
  uint8_t highest = 2;
  uint32_t a = 0x11223344, b = 0;
  ObjectDictionary od;
  od.add_record(0x1200, {{0, ODA_SDO_R, &highest, 1}, {1, ODA_SDO_R, &a, 4}, {2, ODA_SDO_RW, &b, 4}});
  od.finalize();

  OD_entry_t *entry = od.find(0x1200);
  ASSERT_NE(entry, nullptr);
  uint32_t value = 0;
  EXPECT_EQ(OD_get_u32(entry, 1, &value, true), ODR_OK);
  EXPECT_EQ(value, 0x11223344u);
  // CANopenNode writes the default COB-IDs into 0x1200 this way (original data, no attribute check)
  EXPECT_EQ(OD_set_u32(entry, 2, 0x5FF, true), ODR_OK);
  EXPECT_EQ(b, 0x5FFu);
  uint8_t count = 0;
  EXPECT_EQ(OD_get_u8(entry, 0, &count, true), ODR_OK);
  EXPECT_EQ(count, 2);
}

// overrides only od_write(): reads use the storage, writes are counted
struct CountingObject : OdObject {
  int writes{0};
  uint8_t last_sub{0};
  OdResult od_write(OdStream stream, const void *buf, size_t count, size_t *done) {
    this->writes++;
    this->last_sub = stream.sub();
    return stream.write_original(buf, count, done);
  }
};

// a read-only domain without storage, generated in chunks: 100 bytes 0, 1, 2, ...
struct GeneratedObject : OdObject {
  OdResult od_read(OdStream stream, void *buf, size_t count, size_t *done) {
    if (stream.offset() == 0)
      stream.set_size(100);  // size indication for the upload
    size_t n = std::min(count, stream.size() - stream.offset());
    for (size_t i = 0; i < n; i++)
      static_cast<uint8_t *>(buf)[i] = stream.offset() + i;
    stream.set_offset(stream.offset() + n);
    *done = n;
    return stream.offset() < stream.size() ? OdResult::OD_RESULT_PARTIAL : OdResult::OD_RESULT_OK;
  }
};

TEST(CanopenOD, HandlerWrite) {
  uint32_t value = 7;
  uint16_t ro = 1;
  CountingObject handler;
  ObjectDictionary od;
  od.add_var(0x2000, OD_READ_WRITE, &value, sizeof(value));
  od.add_var(0x2001, OD_READ, &ro, sizeof(ro));
  od.set_object(0x2000, &handler);
  ASSERT_TRUE(od.finalize());

  uint32_t v = 0x12345678;
  EXPECT_EQ(od.write(0x2000, 0, &v, sizeof(v)), OdResult::OD_RESULT_OK);
  EXPECT_EQ(value, 0x12345678u);
  EXPECT_EQ(handler.writes, 1);
  // reads go to the storage (default od_read)
  uint32_t read = 0;
  EXPECT_EQ(OD_get_u32(od.find(0x2000), 0, &read, false), ODR_OK);
  EXPECT_EQ(read, 0x12345678u);

  uint16_t short_value = 1;
  uint64_t long_value = 1;
  EXPECT_EQ(od.write(0x2000, 0, &short_value, sizeof(short_value)), OdResult::OD_RESULT_DATA_SHORT);
  EXPECT_EQ(od.write(0x2000, 0, &long_value, sizeof(long_value)), OdResult::OD_RESULT_DATA_LONG);
  EXPECT_EQ(od.write(0x2001, 0, &short_value, sizeof(short_value)), OdResult::OD_RESULT_READ_ONLY);
  EXPECT_EQ(od.write(0x2002, 0, &v, sizeof(v)), OdResult::OD_RESULT_NOT_EXIST);
  EXPECT_EQ(od.write(0x2000, 1, &v, sizeof(v)), OdResult::OD_RESULT_SUB_NOT_EXIST);
  EXPECT_EQ(handler.writes, 1);
}

TEST(CanopenOD, HandlerPartialRead) {
  GeneratedObject handler;
  ObjectDictionary od;
  od.add_var(0x3100, OD_READ, nullptr, 0);
  od.set_object(0x3100, &handler);
  ASSERT_TRUE(od.finalize());

  // as the SDO server reads it: chunks of the free buffer space until OK
  OD_IO_t io;
  ASSERT_EQ(OD_getSub(od.find(0x3100), 0, &io, false), ODR_OK);
  uint8_t data[128];
  size_t total = 0;
  ODR_t ret;
  do {
    OD_size_t n = 0;
    ret = io.read(&io.stream, data + total, 32, &n);
    total += n;
  } while (ret == ODR_PARTIAL && total < sizeof(data));
  EXPECT_EQ(ret, ODR_OK);
  EXPECT_EQ(total, 100u);
  EXPECT_EQ(io.stream.dataLength, 100u);
  EXPECT_EQ(data[0], 0);
  EXPECT_EQ(data[99], 99);
  // no od_write: the default writes to the storage, which a read-only entry doesn't allow
  uint8_t b = 0;
  EXPECT_EQ(od.write(0x3100, 0, &b, 1), OdResult::OD_RESULT_READ_ONLY);
}

TEST(CanopenOD, Errors) {
  uint8_t a = 0, b = 0;
  CountingObject handler;
  ObjectDictionary od;
  EXPECT_EQ(od.write(0x2000, 0, &a, 1), OdResult::OD_RESULT_DEVICE_STATE);  // before finalize()
  od.add_var(0x2000, OD_READ_WRITE, &a, 1);
  od.add_var(0x2000, OD_READ_WRITE, &b, 1);
  od.set_object(0x2100, &handler);  // no such entry: logged, ignored
  EXPECT_FALSE(od.finalize());
}

}  // namespace esphome::canopen::testing
