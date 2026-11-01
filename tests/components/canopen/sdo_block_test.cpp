#include <gtest/gtest.h>

#include <vector>

#include "301/crc16-ccitt.h"
#include "esphome/components/canopen/canopen.h"
#include "fake_canbus.h"

namespace esphome::canopen::testing {

// SDO block download (CiA 301) to the hub's server, played by hand as a client on the fake bus. The unit test
// build enables block transfer with a download block size of 16 (tests/components/canopen/__init__.py).
static constexpr uint32_t SDO_RX = 0x610;  // client -> server (0x600 + node 0x10)
static constexpr uint32_t SDO_TX = 0x590;  // server -> client

// a domain without storage, collected in chunks by its handler (like a firmware image)
struct Domain : OdObject {
  std::vector<uint8_t> data;
  std::vector<size_t> sizes;  // stream.size() at each call
  OdResult od_write(OdStream stream, const void *buf, size_t count, size_t *done) {
    this->sizes.push_back(stream.size());
    const auto *bytes = static_cast<const uint8_t *>(buf);
    this->data.insert(this->data.end(), bytes, bytes + count);
    stream.set_offset(stream.offset() + count);
    *done = count;
    // the size is 0 until the last chunk (end of transfer), then the total
    return stream.size() == 0 ? OdResult::OD_RESULT_PARTIAL : OdResult::OD_RESULT_OK;
  }
};

class BlockClient {
 public:
  BlockClient(TestHub &hub, FakeCanbus &bus) : hub_(hub), bus_(bus) {}

  // the last frame the server sent
  std::vector<uint8_t> reply() {
    this->hub_.loop();
    const FakeCanbus::Frame *last = nullptr;
    for (const auto &f : this->bus_.sent) {
      if (f.can_id == SDO_TX)
        last = &f;
    }
    return last != nullptr ? last->data : std::vector<uint8_t>{};
  }
  void send(const std::vector<uint8_t> &data) {
    this->bus_.sent.clear();
    this->hub_.frame(SDO_RX, data);
  }

  // whole download; returns the block sizes the server announced (initiate, then each block ack)
  std::vector<uint8_t> download(uint16_t index, uint8_t sub, const std::vector<uint8_t> &data) {
    std::vector<uint8_t> blksizes;
    uint32_t size = data.size();
    // initiate: ccs 6, client CRC, size indicated
    this->send({0xC6, uint8_t(index), uint8_t(index >> 8), sub, uint8_t(size), uint8_t(size >> 8), uint8_t(size >> 16),
                uint8_t(size >> 24)});
    auto r = this->reply();
    EXPECT_EQ(r.size(), 8u);
    if (r.size() != 8 || r[0] != 0xA4)  // scs 5, server CRC
      return blksizes;
    uint8_t blksize = r[4];
    blksizes.push_back(blksize);
    size_t pos = 0;
    uint8_t last_len = 0;
    while (pos < data.size()) {
      uint8_t seq = 0;
      while (seq < blksize && pos < data.size()) {
        seq++;
        size_t n = std::min<size_t>(7, data.size() - pos);
        bool last = pos + n == data.size();
        std::vector<uint8_t> frame(8, 0);
        frame[0] = (last ? 0x80 : 0) | seq;
        std::copy(data.begin() + pos, data.begin() + pos + n, frame.begin() + 1);
        this->send(frame);
        pos += n;
        last_len = n;
      }
      r = this->reply();
      EXPECT_EQ(r.size(), 8u);
      if (r.size() != 8 || r[0] != 0xA2 || r[1] != seq)  // block ack, all segments received
        return blksizes;
      blksize = r[2];
      blksizes.push_back(blksize);
    }
    // end: bytes of the last segment without data, CRC
    uint16_t crc = crc16_ccitt(data.data(), data.size(), 0);
    this->send({uint8_t(0xC1 | ((7 - last_len) << 2)), uint8_t(crc), uint8_t(crc >> 8), 0, 0, 0, 0, 0});
    r = this->reply();
    EXPECT_EQ(r.size(), 8u);
    if (r.size() == 8)
      EXPECT_EQ(r[0], 0xA1);  // end response
    return blksizes;
  }

  TestHub &hub() { return this->hub_; }
  FakeCanbus &bus() { return this->bus_; }

 protected:
  TestHub &hub_;
  FakeCanbus &bus_;
};

TEST(CanopenSdoBlock, Download) {
  FakeCanbus bus;
  TestHub hub;
  hub.set_canbus(&bus);
  Domain image;
  hub.get_od().add_var(0x3000, OD_WRITE, nullptr, 0);
  hub.get_od().set_object(0x3000, &image);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();

  // 2000 bytes: 286 segments, in blocks of at most 16 (the announced size, CANOPEN_SDO_BLOCK_SIZE); more than the
  // server buffer (900), so the handler gets it in several chunks
  std::vector<uint8_t> data(2000);
  for (size_t i = 0; i < data.size(); i++)
    data[i] = i * 7 + 3;
  BlockClient client(hub, bus);
  auto blksizes = client.download(0x3000, 0, data);
  ASSERT_GE(blksizes.size(), 3u);
  for (uint8_t b : blksizes)
    EXPECT_EQ(b, CANOPEN_SDO_BLOCK_SIZE);
  EXPECT_EQ(image.data, data);
  // a domain's size isn't known before the last chunk, which has the total (the client's indicated size isn't passed)
  ASSERT_GE(image.sizes.size(), 3u);
  for (size_t i = 0; i + 1 < image.sizes.size(); i++)
    EXPECT_EQ(image.sizes[i], 0u);
  EXPECT_EQ(image.sizes.back(), data.size());
}

TEST(CanopenSdoBlock, WrongCrcAborts) {
  FakeCanbus bus;
  TestHub hub;
  hub.set_canbus(&bus);
  Domain image;
  hub.get_od().add_var(0x3000, OD_WRITE, nullptr, 0);
  hub.get_od().set_object(0x3000, &image);
  hub.setup();
  hub.loop();

  BlockClient client(hub, bus);
  // one segment of 5 bytes, then an end with a wrong CRC
  client.send({0xC6, 0x00, 0x30, 0, 5, 0, 0, 0});
  ASSERT_EQ(client.reply()[0], 0xA4);
  client.send({0x81, 1, 2, 3, 4, 5, 0, 0});
  ASSERT_EQ(client.reply()[0], 0xA2);
  client.send({0xC1 | (2 << 2), 0x12, 0x34, 0, 0, 0, 0, 0});
  auto r = client.reply();
  ASSERT_EQ(r.size(), 8u);
  EXPECT_EQ(r[0], 0x80);  // abort
  uint32_t code = r[4] | (r[5] << 8) | (r[6] << 16) | (uint32_t(r[7]) << 24);
  EXPECT_EQ(code, 0x05040004u);  // CRC error
}

}  // namespace esphome::canopen::testing

namespace esphome::canopen::testing {

// a domain read in chunks with the size indicated at the first read, consumed as it's read (like remote logs)
struct Source : OdObject {
  std::vector<uint8_t> data;
  size_t remaining{0};
  size_t pos{0};
  OdResult od_read(OdStream stream, void *buf, size_t count, size_t *done) {
    if (stream.offset() == 0) {
      this->remaining = this->data.size() - this->pos;
      stream.set_size(this->remaining);
    }
    size_t n = std::min(count, this->remaining);
    std::copy(this->data.begin() + this->pos, this->data.begin() + this->pos + n, static_cast<uint8_t *>(buf));
    this->pos += n;
    this->remaining -= n;
    stream.set_offset(stream.offset() + n);
    *done = n;
    return this->remaining != 0 ? OdResult::OD_RESULT_PARTIAL : OdResult::OD_RESULT_OK;
  }
};

// block upload as python-canopen's BlockUploadStream does it (no CRC, pst 0), blksize 16; returns the data, or
// stops at the first unexpected frame (abort code in *abort)
static std::vector<uint8_t> block_upload(BlockClient &client, uint16_t index, uint8_t sub, uint32_t *abort) {
  std::vector<uint8_t> out;
  *abort = 0;
  auto check_abort = [&](const std::vector<uint8_t> &r) {
    if (r.size() == 8 && r[0] == 0x80) {
      *abort = r[4] | (r[5] << 8) | (r[6] << 16) | (uint32_t(r[7]) << 24);
      return true;
    }
    return false;
  };
  client.send({0xA0, uint8_t(index), uint8_t(index >> 8), sub, 16, 0, 0, 0});
  auto r = client.reply();
  if (r.size() != 8 || check_abort(r) || (r[0] & 0xF9) != 0xC0)
    return out;
  client.send({0xA3, 0, 0, 0, 0, 0, 0, 0});
  for (int blocks = 0; blocks < 100; blocks++) {
    // the server sends a block, one segment per processing call
    uint8_t seq = 0;
    bool last = false;
    for (int i = 0; i < 200 && !last; i++) {
      client.hub().loop();
      for (const auto &f : client.bus().sent) {
        if (f.can_id != SDO_TX)
          continue;
        if (check_abort(f.data))
          return out;
        seq = f.data[0] & 0x7F;
        last = (f.data[0] & 0x80) != 0;
        out.insert(out.end(), f.data.begin() + 1, f.data.end());
      }
      client.bus().sent.clear();
      if (seq == 16)
        break;
    }
    if (last) {
      client.send({0xA2, seq, 16, 0, 0, 0, 0, 0});
      r = client.reply();
      if (r.size() != 8 || check_abort(r) || (r[0] & 0xE3) != 0xC1)
        return out;
      out.resize(out.size() - ((r[0] >> 2) & 7));  // bytes without data in the last segment
      client.send({0xA1, 0, 0, 0, 0, 0, 0, 0});
      return out;
    }
    client.send({0xA2, seq, 16, 0, 0, 0, 0, 0});
  }
  return out;
}

TEST(CanopenSdoBlock, UploadConsumedDomain) {
  for (size_t size : {100, 500, 1500, 2048}) {
    FakeCanbus bus;
    TestHub hub;
    hub.set_canbus(&bus);
    Source src;
    for (size_t i = 0; i < size; i++)
      src.data.push_back(uint8_t(i * 7 + 3));
    hub.get_od().add_var(0x3100, OD_READ, nullptr, 0);
    hub.get_od().set_object(0x3100, &src);
    hub.setup();
    hub.loop();
    BlockClient client(hub, bus);
    uint32_t abort = 0;
    auto out = block_upload(client, 0x3100, 0, &abort);
    EXPECT_EQ(abort, 0u) << "size " << size;
    EXPECT_EQ(out, src.data) << "size " << size;
  }
}

}  // namespace esphome::canopen::testing
