#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/canopen/ota/program_download.h"
#include "../fake_canbus.h"

namespace esphome::canopen::testing {

struct FakeBackend {
  std::vector<uint8_t> written;
  int begins{0};
  int aborts{0};
  int ends{0};
  size_t begin_size{1};
  ota::OTAResponseTypes begin_ret{ota::OTA_RESPONSE_OK};
  ota::OTAResponseTypes write_ret{ota::OTA_RESPONSE_OK};
  ota::OTAResponseTypes end_ret{ota::OTA_RESPONSE_OK};

  ota::OTAResponseTypes begin(size_t size, ota::OTAType = ota::OTA_TYPE_UPDATE_APP) {
    this->begins++;
    this->begin_size = size;
    return this->begin_ret;
  }
  void set_update_md5(const char *) { ADD_FAILURE() << "the hub OTA has no MD5"; }
  ota::OTAResponseTypes write(uint8_t *data, size_t len) {
    if (this->write_ret == ota::OTA_RESPONSE_OK)
      this->written.insert(this->written.end(), data, data + len);
    return this->write_ret;
  }
  ota::OTAResponseTypes end() {
    this->ends++;
    return this->end_ret;
  }
  void abort() { this->aborts++; }
  static constexpr bool supports_compression() { return false; }
};
static_assert(ota::OTABackendContract<FakeBackend>);

class TestOta : public ProgramDownload {
 public:
  TestOta() {
    this->delay_ms_ = 0;
    this->reboot_ = false;
  }
  FakeBackend &backend() { return this->fake_; }

 protected:
  ota::OTAResponseTypes backend_begin_() override { return this->fake_.begin(0); }
  ota::OTAResponseTypes backend_write_(const uint8_t *data, size_t len) override {
    return this->fake_.write(const_cast<uint8_t *>(data), len);
  }
  ota::OTAResponseTypes backend_end_() override { return this->fake_.end(); }
  void backend_abort_() override { this->fake_.abort(); }

  FakeBackend fake_;
};

using Ota = ProgramDownload;

class ProgramDownloadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub.set_canbus(&this->bus);
    this->ota.set_canopen(&this->hub);
    this->hub.setup();
    this->ota.setup();
  }
  OdResult control(uint8_t value) { return this->hub.get_od().write(Ota::OD_PROGRAM_CONTROL, 1, &value, 1); }
  // the image in chunks, as an SDO transfer delivers it: the size is 0 until the last chunk
  OdResult chunks(const std::vector<uint8_t> &image, size_t chunk, bool finish = true) {
    OD_IO_t io;
    EXPECT_EQ(OD_getSub(this->hub.get_od().find(Ota::OD_PROGRAM_DATA), 1, &io, false), ODR_OK);
    io.stream.dataLength = 0;
    size_t pos = 0;
    OdResult ret = OdResult::OD_RESULT_PARTIAL;
    while (pos < image.size()) {
      size_t n = std::min(chunk, image.size() - pos);
      if (finish && pos + n == image.size())
        io.stream.dataLength = image.size();
      OD_size_t written = 0;
      ret = static_cast<OdResult>(io.write(&io.stream, image.data() + pos, n, &written));
      if (ret != OdResult::OD_RESULT_PARTIAL && ret != OdResult::OD_RESULT_OK)
        return ret;
      pos += n;
    }
    return ret;
  }
  template<typename T> T read(uint16_t index) {
    OD_IO_t io;
    EXPECT_EQ(OD_getSub(this->hub.get_od().find(index), 1, &io, false), ODR_OK);
    T value{};
    OD_size_t n = 0;
    EXPECT_EQ(io.read(&io.stream, &value, sizeof(value), &n), ODR_OK);
    return value;
  }
  uint8_t status() { return this->read<uint8_t>(Ota::OD_PROGRAM_CONTROL); }
  uint32_t crc() { return this->read<uint32_t>(Ota::OD_PROGRAM_ID); }
  uint32_t flash() { return this->read<uint32_t>(Ota::OD_FLASH_STATUS); }
  void prepare() {
    ASSERT_EQ(this->control(Ota::CONTROL_STOP), OdResult::OD_RESULT_OK);
    ASSERT_EQ(this->control(Ota::CONTROL_CLEAR), OdResult::OD_RESULT_OK);
  }

  FakeCanbus bus;
  TestHub hub;
  TestOta ota;
  const std::vector<uint8_t> image{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
};

TEST(ProgramDownloadCrc, MatchesZlib) {
  const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  // the check value of CRC-32, zlib.crc32(b"123456789")
  EXPECT_EQ(crc32_update(0, data, sizeof(data)), 0xCBF43926u);
  EXPECT_EQ(crc32_update(crc32_update(0, data, 4), data + 4, 5), 0xCBF43926u);
  EXPECT_EQ(crc32_update(0, data, 0), 0u);
}

TEST_F(ProgramDownloadTest, InitialState) {
  EXPECT_EQ(this->status(), Ota::CONTROL_START);
  EXPECT_EQ(this->crc(), 0u);
  EXPECT_EQ(this->flash(), 0u);
}

TEST_F(ProgramDownloadTest, WholeSequence) {
  this->prepare();
  EXPECT_EQ(this->status(), Ota::CONTROL_CLEAR);
  EXPECT_EQ(this->ota.backend().begins, 1);
  EXPECT_EQ(this->ota.backend().begin_size, 0u);
  EXPECT_EQ(this->chunks(this->image, 4), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->ota.backend().written, this->image);
  EXPECT_EQ(this->status(), Ota::CONTROL_STOP);
  EXPECT_EQ(this->crc(), 0xCBF43926u);
  EXPECT_EQ(this->flash(), 0u);
  EXPECT_EQ(this->ota.backend().ends, 0);

  EXPECT_EQ(this->control(Ota::CONTROL_START), OdResult::OD_RESULT_OK);
  // deferred: the reply to the write goes first
  EXPECT_EQ(this->ota.backend().ends, 0);
  EXPECT_EQ(this->status(), Ota::CONTROL_START);
  this->ota.loop();
  EXPECT_EQ(this->ota.backend().ends, 1);
  EXPECT_EQ(this->ota.backend().aborts, 0);
  this->ota.loop();
}

TEST_F(ProgramDownloadTest, InProgressDuringDownload) {
  this->prepare();
  EXPECT_EQ(this->chunks(this->image, 4, false), OdResult::OD_RESULT_PARTIAL);
  EXPECT_EQ(this->flash(), Ota::FLASH_IN_PROGRESS);
  EXPECT_EQ(this->crc(), 0u);
  EXPECT_EQ(this->status(), Ota::CONTROL_CLEAR);
}

TEST_F(ProgramDownloadTest, ClearNeedsStop) {
  EXPECT_EQ(this->control(Ota::CONTROL_CLEAR), OdResult::OD_RESULT_NO_DATA);
  EXPECT_EQ(this->ota.backend().begins, 0);
}

TEST_F(ProgramDownloadTest, DataNeedsClear) {
  EXPECT_EQ(this->chunks(this->image, 4), OdResult::OD_RESULT_HARDWARE);
  EXPECT_EQ(this->ota.backend().written.size(), 0u);
  EXPECT_EQ((this->flash() >> 1) & 0x7F, Ota::FLASH_NOT_CLEARED);
}

TEST_F(ProgramDownloadTest, StartNeedsWholeImage) {
  this->prepare();
  EXPECT_EQ(this->control(Ota::CONTROL_START), OdResult::OD_RESULT_NO_DATA);
  EXPECT_EQ(this->chunks(this->image, 4, false), OdResult::OD_RESULT_PARTIAL);
  EXPECT_EQ(this->control(Ota::CONTROL_START), OdResult::OD_RESULT_NO_DATA);
  EXPECT_EQ(this->ota.backend().ends, 0);
}

TEST_F(ProgramDownloadTest, StartWithoutImageOnlyRuns) {
  EXPECT_EQ(this->control(Ota::CONTROL_STOP), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->status(), Ota::CONTROL_STOP);
  EXPECT_EQ(this->control(Ota::CONTROL_START), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->status(), Ota::CONTROL_START);
  this->ota.loop();
  EXPECT_EQ(this->ota.backend().ends, 0);
}

TEST_F(ProgramDownloadTest, StopAfterCrcMismatchDropsTheImage) {
  this->prepare();
  EXPECT_EQ(this->chunks(this->image, 4), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->control(Ota::CONTROL_STOP), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->ota.backend().aborts, 1);
  EXPECT_EQ(this->crc(), 0u);
  // nothing left to start
  EXPECT_EQ(this->control(Ota::CONTROL_START), OdResult::OD_RESULT_OK);
  this->ota.loop();
  EXPECT_EQ(this->ota.backend().ends, 0);
}

TEST_F(ProgramDownloadTest, StopDuringDownload) {
  this->prepare();
  EXPECT_EQ(this->chunks(this->image, 4, false), OdResult::OD_RESULT_PARTIAL);
  EXPECT_EQ(this->control(Ota::CONTROL_STOP), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->ota.backend().aborts, 1);
  EXPECT_EQ(this->flash(), 0u);
  EXPECT_EQ(this->chunks(this->image, 4), OdResult::OD_RESULT_HARDWARE);
}

TEST_F(ProgramDownloadTest, NewTransferAfterPartOfAnImageIsRefused) {
  this->prepare();
  EXPECT_EQ(this->chunks(this->image, 4, false), OdResult::OD_RESULT_PARTIAL);
  // a new SDO transfer: offset 0 again
  EXPECT_EQ(this->chunks(this->image, 4), OdResult::OD_RESULT_HARDWARE);
  EXPECT_EQ(this->ota.backend().written, this->image);
}

TEST_F(ProgramDownloadTest, ClearAgainRestarts) {
  this->prepare();
  EXPECT_EQ(this->chunks(this->image, 4), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->control(Ota::CONTROL_CLEAR), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->ota.backend().aborts, 1);
  EXPECT_EQ(this->ota.backend().begins, 2);
  EXPECT_EQ(this->crc(), 0u);
  EXPECT_EQ(this->chunks(this->image, 4), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->crc(), 0xCBF43926u);
}

TEST_F(ProgramDownloadTest, BeginFailure) {
  this->ota.backend().begin_ret = ota::OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION;
  EXPECT_EQ(this->control(Ota::CONTROL_STOP), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->control(Ota::CONTROL_CLEAR), OdResult::OD_RESULT_HARDWARE);
  EXPECT_EQ(this->status(), Ota::CONTROL_STOP);
  EXPECT_EQ((this->flash() >> 1) & 0x7F, Ota::FLASH_UNSPECIFIED);
  EXPECT_EQ(this->flash() >> 16, uint32_t(ota::OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION));
}

TEST_F(ProgramDownloadTest, WriteFailure) {
  this->prepare();
  this->ota.backend().write_ret = ota::OTA_RESPONSE_ERROR_WRITING_FLASH;
  EXPECT_EQ(this->chunks(this->image, 4), OdResult::OD_RESULT_HARDWARE);
  EXPECT_EQ(this->ota.backend().aborts, 1);
  EXPECT_EQ(this->status(), Ota::CONTROL_STOP);
  EXPECT_EQ(this->crc(), 0u);
  EXPECT_EQ((this->flash() >> 1) & 0x7F, Ota::FLASH_WRITE_ERROR);
  // a stop keeps the error readable, a clear resets it
  EXPECT_EQ(this->control(Ota::CONTROL_STOP), OdResult::OD_RESULT_OK);
  EXPECT_EQ((this->flash() >> 1) & 0x7F, Ota::FLASH_WRITE_ERROR);
  this->ota.backend().write_ret = ota::OTA_RESPONSE_OK;
  EXPECT_EQ(this->control(Ota::CONTROL_CLEAR), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->flash(), 0u);
}

TEST_F(ProgramDownloadTest, EndFailureIsInFlashStatus) {
  this->prepare();
  EXPECT_EQ(this->chunks(this->image, 4), OdResult::OD_RESULT_OK);
  this->ota.backend().end_ret = ota::OTA_RESPONSE_ERROR_UPDATE_END;
  EXPECT_EQ(this->control(Ota::CONTROL_START), OdResult::OD_RESULT_OK);
  this->ota.loop();
  EXPECT_EQ(this->ota.backend().aborts, 1);
  EXPECT_EQ(this->status(), Ota::CONTROL_STOP);
  EXPECT_EQ(this->crc(), 0u);
  EXPECT_EQ(this->flash() >> 16, uint32_t(ota::OTA_RESPONSE_ERROR_UPDATE_END));
}

TEST_F(ProgramDownloadTest, InvalidControlValues) {
  EXPECT_EQ(this->control(2), OdResult::OD_RESULT_INVALID_VALUE);
  EXPECT_EQ(this->control(0x80), OdResult::OD_RESULT_INVALID_VALUE);
  EXPECT_EQ(this->status(), Ota::CONTROL_START);
}

TEST_F(ProgramDownloadTest, ControlRefusedWhileFinishing) {
  this->prepare();
  EXPECT_EQ(this->chunks(this->image, 4), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->control(Ota::CONTROL_START), OdResult::OD_RESULT_OK);
  EXPECT_EQ(this->control(Ota::CONTROL_STOP), OdResult::OD_RESULT_NO_DATA);
  EXPECT_EQ(this->control(Ota::CONTROL_CLEAR), OdResult::OD_RESULT_NO_DATA);
  EXPECT_EQ(this->ota.backend().aborts, 0);
}

}  // namespace esphome::canopen::testing
