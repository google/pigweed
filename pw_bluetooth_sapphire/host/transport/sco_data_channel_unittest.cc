// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_channel.h"

#include <lib/gtest/test_loop_fixture.h>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bt::hci {
namespace {

constexpr hci_spec::ConnectionHandle kConnectionHandle0 = 0x0000;
constexpr hci_spec::ConnectionHandle kConnectionHandle1 = 0x0001;
constexpr size_t kBufferMaxNumPackets = 2;

constexpr hci_spec::SynchronousConnectionParameters kMsbcConnectionParameters{
    .transmit_bandwidth = 0,
    .receive_bandwidth = 0,
    .transmit_coding_format =
        hci_spec::VendorCodingFormat{
            .coding_format = hci_spec::CodingFormat::kMSbc,
            .company_id = 0,
            .vendor_codec_id = 0,
        },
    .receive_coding_format =
        hci_spec::VendorCodingFormat{
            .coding_format = hci_spec::CodingFormat::kMSbc,
            .company_id = 0,
            .vendor_codec_id = 0,
        },
    .transmit_codec_frame_size_bytes = 0,
    .receive_codec_frame_size_bytes = 0,
    .input_bandwidth = 32000,
    .output_bandwidth = 32000,
    .input_coding_format =
        hci_spec::VendorCodingFormat{
            .coding_format = hci_spec::CodingFormat::kMSbc,
            .company_id = 0,
            .vendor_codec_id = 0,
        },
    .output_coding_format =
        hci_spec::VendorCodingFormat{
            .coding_format = hci_spec::CodingFormat::kMSbc,
            .company_id = 0,
            .vendor_codec_id = 0,
        },
    .input_coded_data_size_bits = 16,
    .output_coded_data_size_bits = 16,
    .input_pcm_data_format = hci_spec::PcmDataFormat::kUnsigned,
    .output_pcm_data_format = hci_spec::PcmDataFormat::kUnsigned,
    .input_pcm_sample_payload_msb_position = 0,
    .output_pcm_sample_payload_msb_position = 0,
    .input_data_path = hci_spec::ScoDataPath::kHci,
    .output_data_path = hci_spec::ScoDataPath::kHci,
    .input_transport_unit_size_bits = 0,
    .output_transport_unit_size_bits = 0,
    .max_latency_ms = 0,
    .packet_types = 0,
    .retransmission_effort = hci_spec::ScoRetransmissionEffort::kNone,
};

constexpr hci_spec::SynchronousConnectionParameters kCvsdConnectionParameters{
    .transmit_bandwidth = 0,
    .receive_bandwidth = 0,
    .transmit_coding_format =
        hci_spec::VendorCodingFormat{
            .coding_format = hci_spec::CodingFormat::kCvsd,
            .company_id = 0,
            .vendor_codec_id = 0,
        },
    .receive_coding_format =
        hci_spec::VendorCodingFormat{
            .coding_format = hci_spec::CodingFormat::kCvsd,
            .company_id = 0,
            .vendor_codec_id = 0,
        },
    .transmit_codec_frame_size_bytes = 0,
    .receive_codec_frame_size_bytes = 0,
    .input_bandwidth = 8000,
    .output_bandwidth = 8000,
    .input_coding_format =
        hci_spec::VendorCodingFormat{
            .coding_format = hci_spec::CodingFormat::kCvsd,
            .company_id = 0,
            .vendor_codec_id = 0,
        },
    .output_coding_format =
        hci_spec::VendorCodingFormat{
            .coding_format = hci_spec::CodingFormat::kCvsd,
            .company_id = 0,
            .vendor_codec_id = 0,
        },
    .input_coded_data_size_bits = 8,
    .output_coded_data_size_bits = 8,
    .input_pcm_data_format = hci_spec::PcmDataFormat::kUnsigned,
    .output_pcm_data_format = hci_spec::PcmDataFormat::kUnsigned,
    .input_pcm_sample_payload_msb_position = 0,
    .output_pcm_sample_payload_msb_position = 0,
    .input_data_path = hci_spec::ScoDataPath::kHci,
    .output_data_path = hci_spec::ScoDataPath::kHci,
    .input_transport_unit_size_bits = 0,
    .output_transport_unit_size_bits = 0,
    .max_latency_ms = 0,
    .packet_types = 0,
    .retransmission_effort = hci_spec::ScoRetransmissionEffort::kNone,
};

class FakeScoConnection : public ScoDataChannel::ConnectionInterface {
 public:
  explicit FakeScoConnection(
      ScoDataChannel* data_channel, hci_spec::ConnectionHandle handle = kConnectionHandle0,
      hci_spec::SynchronousConnectionParameters params = kMsbcConnectionParameters)
      : handle_(handle), params_(params), data_channel_(data_channel), weak_ptr_factory_(this) {}

  ~FakeScoConnection() override = default;

  void QueuePacket(std::unique_ptr<ScoDataPacket> packet) {
    queued_packets_.push(std::move(packet));
    data_channel_->OnOutboundPacketReadable();
  }

  const std::vector<std::unique_ptr<ScoDataPacket>>& received_packets() const {
    return received_packets_;
  }
  const std::queue<std::unique_ptr<ScoDataPacket>>& queued_packets() const {
    return queued_packets_;
  }

  uint16_t hci_error_count() const { return hci_error_count_; }

  fxl::WeakPtr<FakeScoConnection> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  // ScoDataChannel::ConnectionInterface overrides:

  hci_spec::ConnectionHandle handle() const override { return handle_; }

  hci_spec::SynchronousConnectionParameters parameters() override { return params_; }

  std::unique_ptr<ScoDataPacket> GetNextOutboundPacket() override {
    if (queued_packets_.empty()) {
      return nullptr;
    }
    std::unique_ptr<ScoDataPacket> packet = std::move(queued_packets_.front());
    queued_packets_.pop();
    return packet;
  }

  void ReceiveInboundPacket(std::unique_ptr<ScoDataPacket> packet) override {
    received_packets_.push_back(std::move(packet));
  }

  void OnHciError() override { hci_error_count_++; }

 private:
  hci_spec::ConnectionHandle handle_;
  hci_spec::SynchronousConnectionParameters params_;
  std::queue<std::unique_ptr<ScoDataPacket>> queued_packets_;
  std::vector<std::unique_ptr<ScoDataPacket>> received_packets_;
  ScoDataChannel* data_channel_;
  uint16_t hci_error_count_ = 0;
  fxl::WeakPtrFactory<FakeScoConnection> weak_ptr_factory_;
};

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;
class ScoDataChannelTest : public TestingBase {
 public:
  void SetUp() override {
    TestingBase::SetUp();
    StartTestDevice();

    DataBufferInfo buffer_info(/*max_data_length=*/10, kBufferMaxNumPackets);
    InitializeScoDataChannel(buffer_info);
  }
};

class ScoDataChannelSingleConnectionTest : public ScoDataChannelTest {
 public:
  void SetUp() override {
    ScoDataChannelTest::SetUp();

    set_configure_sco_cb([this](sco_coding_format_t format, sco_encoding_t encoding,
                                sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                                void* cookie) {
      config_count_++;
      EXPECT_EQ(format, SCO_CODING_FORMAT_MSBC);
      EXPECT_EQ(encoding, SCO_ENCODING_BITS_16);
      EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_16);
      callback(cookie, ZX_OK);
    });

    set_reset_sco_cb([this](bt_hci_reset_sco_callback callback, void* cookie) {
      reset_count_++;
      callback(cookie, ZX_OK);
    });

    connection_.emplace(sco_data_channel());

    sco_data_channel()->RegisterConnection(connection_->GetWeakPtr());
    EXPECT_EQ(config_count_, 1);
    EXPECT_EQ(reset_count_, 0);
  }

  void TearDown() override {
    sco_data_channel()->UnregisterConnection(connection_->handle());
    EXPECT_EQ(config_count_, 1);
    EXPECT_EQ(reset_count_, 1);
    set_configure_sco_cb(nullptr);
    set_reset_sco_cb(nullptr);
    ScoDataChannelTest::TearDown();
  }

  FakeScoConnection* connection() { return &connection_.value(); }

 private:
  std::optional<FakeScoConnection> connection_;
  int config_count_ = 0;
  int reset_count_ = 0;
};

TEST_F(ScoDataChannelSingleConnectionTest, SendManyMsbcPackets) {
  // Queue 1 more than than the max number of packets (1 packet will remain queued).
  for (size_t i = 0; i <= kBufferMaxNumPackets; i++) {
    std::unique_ptr<ScoDataPacket> packet =
        ScoDataPacket::New(kConnectionHandle0, /*payload_size=*/1);
    packet->mutable_view()->mutable_payload_data()[0] = static_cast<uint8_t>(i);

    // The last packet should remain queued.
    if (i < kBufferMaxNumPackets) {
      EXPECT_SCO_PACKET_OUT(test_device(), StaticByteBuffer(LowerBits(kConnectionHandle0),
                                                            UpperBits(kConnectionHandle0),
                                                            0x01,  // payload length
                                                            static_cast<uint8_t>(i)));
    }
    connection()->QueuePacket(std::move(packet));
    RunLoopUntilIdle();
  }

  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());

  EXPECT_SCO_PACKET_OUT(
      test_device(), StaticByteBuffer(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0),
                                      0x01,  // payload length
                                      static_cast<uint8_t>(kBufferMaxNumPackets)));
  test_device()->SendCommandChannelPacket(
      testing::NumberOfCompletedPacketsPacket(kConnectionHandle0, 1));
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());
}

TEST_F(ScoDataChannelSingleConnectionTest, ReceiveManyPackets) {
  for (uint8_t i = 0; i < 20; i++) {
    SCOPED_TRACE(i);
    StaticByteBuffer packet(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0),
                            0x01,  // payload length
                            i      // payload
    );
    test_device()->SendScoDataChannelPacket(packet);
    RunLoopUntilIdle();
    ASSERT_EQ(connection()->received_packets().size(), static_cast<size_t>(i) + 1);
    EXPECT_TRUE(ContainersEqual(connection()->received_packets()[i]->view().data(), packet));
  }
}

TEST_F(ScoDataChannelTest, RegisterTwoConnectionsAndUnregisterFirstConnection) {
  int config_count = 0;
  set_configure_sco_cb([&](sco_coding_format_t format, sco_encoding_t encoding,
                           sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                           void* cookie) {
    config_count++;
    callback(cookie, ZX_OK);
  });

  int reset_count = 0;
  set_reset_sco_cb([&](bt_hci_reset_sco_callback callback, void* cookie) {
    reset_count++;
    callback(cookie, ZX_OK);
  });

  FakeScoConnection connection_0(sco_data_channel());
  sco_data_channel()->RegisterConnection(connection_0.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);

  FakeScoConnection connection_1(sco_data_channel(), kConnectionHandle1);
  sco_data_channel()->RegisterConnection(connection_1.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);

  StaticByteBuffer packet_0(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0),
                            0x01,  // payload length
                            0x00   // payload
  );
  test_device()->SendScoDataChannelPacket(packet_0);
  RunLoopUntilIdle();
  ASSERT_EQ(connection_0.received_packets().size(), 1u);
  ASSERT_EQ(connection_1.received_packets().size(), 0u);

  StaticByteBuffer packet_1(LowerBits(kConnectionHandle1), UpperBits(kConnectionHandle1),
                            0x01,  // payload length
                            0x01   // payload
  );
  test_device()->SendScoDataChannelPacket(packet_1);
  RunLoopUntilIdle();
  ASSERT_EQ(connection_0.received_packets().size(), 1u);
  // The packet should be received even though connection_1 isn't the active connection.
  ASSERT_EQ(connection_1.received_packets().size(), 1u);

  EXPECT_SCO_PACKET_OUT(test_device(), packet_0);
  std::unique_ptr<ScoDataPacket> out_packet_0 = ScoDataPacket::New(/*payload_size=*/1);
  out_packet_0->mutable_view()->mutable_data().Write(packet_0);
  out_packet_0->InitializeFromBuffer();
  connection_0.QueuePacket(std::move(out_packet_0));
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());
  test_device()->SendCommandChannelPacket(
      testing::NumberOfCompletedPacketsPacket(kConnectionHandle0, 1));

  std::unique_ptr<ScoDataPacket> out_packet_1 = ScoDataPacket::New(/*payload_size=*/1);
  out_packet_1->mutable_view()->mutable_data().Write(packet_1);
  out_packet_1->InitializeFromBuffer();
  // The packet should be sent even though connection_1 isn't the active connection.
  EXPECT_SCO_PACKET_OUT(test_device(), packet_1);
  connection_1.QueuePacket(std::move(out_packet_1));
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());
  // This is necessary because kBufferMaxNumPackets is 2, so we won't be able to send
  // any more packets until at least 1 is ACKed by the controller.
  test_device()->SendCommandChannelPacket(
      testing::NumberOfCompletedPacketsPacket(kConnectionHandle1, 1));

  // connection_1 should become the active connection (+1 to config_count).
  sco_data_channel()->UnregisterConnection(connection_0.handle());
  EXPECT_EQ(config_count, 2);
  EXPECT_EQ(reset_count, 0);
  RunLoopUntilIdle();

  out_packet_1 = ScoDataPacket::New(/*payload_size=*/1);
  out_packet_1->mutable_view()->mutable_data().Write(packet_1);
  out_packet_1->InitializeFromBuffer();
  // Now that connection_1 is the active connection, packets should still be sent.
  EXPECT_SCO_PACKET_OUT(test_device(), packet_1);
  connection_1.QueuePacket(std::move(out_packet_1));
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());

  // There are no active connections now (+1 to reset_count).
  sco_data_channel()->UnregisterConnection(connection_1.handle());
  EXPECT_EQ(config_count, 2);
  EXPECT_EQ(reset_count, 1);
}

TEST_F(ScoDataChannelTest, RegisterTwoConnectionsAndClearControllerPacketCountOfFirstConnection) {
  set_configure_sco_cb([](sco_coding_format_t format, sco_encoding_t encoding,
                          sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                          void* cookie) { callback(cookie, ZX_OK); });

  set_reset_sco_cb(
      [](bt_hci_reset_sco_callback callback, void* cookie) { callback(cookie, ZX_OK); });

  FakeScoConnection connection_0(sco_data_channel());
  sco_data_channel()->RegisterConnection(connection_0.GetWeakPtr());

  FakeScoConnection connection_1(sco_data_channel(), kConnectionHandle1);
  sco_data_channel()->RegisterConnection(connection_1.GetWeakPtr());

  auto packet_0 = StaticByteBuffer(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0),
                                   0x01,  // payload length
                                   0x00   // payload
  );
  auto packet_1 = StaticByteBuffer(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0),
                                   0x01,  // payload length
                                   0x01   // payload
  );
  auto packet_2 = StaticByteBuffer(LowerBits(kConnectionHandle1), UpperBits(kConnectionHandle1),
                                   0x01,  // payload length
                                   0x02   // payload
  );

  EXPECT_SCO_PACKET_OUT(test_device(), packet_0);
  std::unique_ptr<ScoDataPacket> out_packet_0 = ScoDataPacket::New(/*payload_size=*/1);
  out_packet_0->mutable_view()->mutable_data().Write(packet_0);
  out_packet_0->InitializeFromBuffer();
  connection_0.QueuePacket(std::move(out_packet_0));
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());

  // The second packet should fill up the controller buffer (kBufferMaxNumPackets).
  ASSERT_EQ(kBufferMaxNumPackets, 2u);
  EXPECT_SCO_PACKET_OUT(test_device(), packet_1);
  std::unique_ptr<ScoDataPacket> out_packet_1 = ScoDataPacket::New(/*payload_size=*/1);
  out_packet_1->mutable_view()->mutable_data().Write(packet_1);
  out_packet_1->InitializeFromBuffer();
  connection_0.QueuePacket(std::move(out_packet_1));
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());

  std::unique_ptr<ScoDataPacket> out_packet_2 = ScoDataPacket::New(/*payload_size=*/1);
  out_packet_2->mutable_view()->mutable_data().Write(packet_2);
  out_packet_2->InitializeFromBuffer();
  // The packet should NOT be sent because the controller buffer is full.
  connection_1.QueuePacket(std::move(out_packet_2));
  RunLoopUntilIdle();

  // connection_1 should become the active connection, but out_packet_2 can't be sent yet.
  sco_data_channel()->UnregisterConnection(connection_0.handle());
  RunLoopUntilIdle();
  EXPECT_EQ(connection_1.queued_packets().size(), 1u);

  // Clearing the pending packet count for connection_0 should result in packet_2 being sent.
  EXPECT_SCO_PACKET_OUT(test_device(), packet_2);
  sco_data_channel()->ClearControllerPacketCount(connection_0.handle());
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());

  // There are no active connections now.
  sco_data_channel()->UnregisterConnection(connection_1.handle());
  sco_data_channel()->ClearControllerPacketCount(connection_1.handle());
  RunLoopUntilIdle();
}

TEST_F(ScoDataChannelSingleConnectionTest, IgnoreInboundPacketForUnknownConnectionHandle) {
  // kConnectionHandle1 is not registered.
  auto packet = StaticByteBuffer(LowerBits(kConnectionHandle1), UpperBits(kConnectionHandle1),
                                 0x01,  // payload length
                                 0x07   // payload
  );
  test_device()->SendScoDataChannelPacket(packet);
  RunLoopUntilIdle();
  EXPECT_EQ(connection()->received_packets().size(), 0u);
}

TEST_F(ScoDataChannelSingleConnectionTest,
       IgnoreNumberOfCompletedPacketsEventForUnknownConnectionHandle) {
  // Queue 1 more than than the max number of packets (1 packet will remain queued).
  for (size_t i = 0; i <= kBufferMaxNumPackets; i++) {
    std::unique_ptr<ScoDataPacket> packet =
        ScoDataPacket::New(kConnectionHandle0, /*payload_size=*/1);
    packet->mutable_view()->mutable_payload_data()[0] = static_cast<uint8_t>(i);

    // The last packet should remain queued.
    if (i < kBufferMaxNumPackets) {
      EXPECT_SCO_PACKET_OUT(test_device(), StaticByteBuffer(LowerBits(kConnectionHandle0),
                                                            UpperBits(kConnectionHandle0),
                                                            0x01,  // payload length
                                                            static_cast<uint8_t>(i)));
    }
    connection()->QueuePacket(std::move(packet));
    RunLoopUntilIdle();
  }
  EXPECT_EQ(connection()->queued_packets().size(), 1u);
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());

  // kConnectionHandle1 is not registered, so this event should be ignored (no packets should be
  // sent).
  test_device()->SendCommandChannelPacket(
      testing::NumberOfCompletedPacketsPacket(kConnectionHandle1, 1));
  RunLoopUntilIdle();
  EXPECT_EQ(connection()->queued_packets().size(), 1u);
}

TEST_F(ScoDataChannelSingleConnectionTest, ReceiveTooSmallPacket) {
  StaticByteBuffer invalid_packet(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0));
  test_device()->SendScoDataChannelPacket(invalid_packet);
  RunLoopUntilIdle();
  // Packet should be ignored.
  EXPECT_EQ(connection()->received_packets().size(), 0u);

  // The next valid packet should not be ignored.
  auto valid_packet = StaticByteBuffer(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0),
                                       0x01,  // correct payload length
                                       0x01   // payload
  );
  test_device()->SendScoDataChannelPacket(valid_packet);
  RunLoopUntilIdle();
  EXPECT_EQ(connection()->received_packets().size(), 1u);
}

TEST_F(ScoDataChannelSingleConnectionTest, ReceivePacketWithIncorrectHeaderLengthField) {
  auto packet = StaticByteBuffer(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0),
                                 0x03,  // incorrect payload length
                                 0x00   // payload
  );
  test_device()->SendScoDataChannelPacket(packet);
  RunLoopUntilIdle();
  // Packet should be ignored.
  EXPECT_EQ(connection()->received_packets().size(), 0u);

  // The next valid packet should not be ignored.
  packet = StaticByteBuffer(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0),
                            0x01,  // correct payload length
                            0x01   // payload
  );
  test_device()->SendScoDataChannelPacket(packet);
  RunLoopUntilIdle();
  EXPECT_EQ(connection()->received_packets().size(), 1u);
}

TEST_F(ScoDataChannelTest, CvsdConnectionEncodingBits8SampleRate8Khz) {
  int config_count = 0;
  set_configure_sco_cb([&](sco_coding_format_t format, sco_encoding_t encoding,
                           sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                           void* cookie) {
    config_count++;
    EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
    EXPECT_EQ(encoding, SCO_ENCODING_BITS_8);
    EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_8);
    callback(cookie, ZX_OK);
  });

  int reset_count = 0;
  set_reset_sco_cb([&](bt_hci_reset_sco_callback callback, void* cookie) {
    reset_count++;
    callback(cookie, ZX_OK);
  });

  FakeScoConnection connection_0(sco_data_channel(), kConnectionHandle0, kCvsdConnectionParameters);
  sco_data_channel()->RegisterConnection(connection_0.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
}

TEST_F(ScoDataChannelTest, CvsdConnectionEncodingBits16SampleRate8Khz) {
  int config_count = 0;
  set_configure_sco_cb([&](sco_coding_format_t format, sco_encoding_t encoding,
                           sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                           void* cookie) {
    config_count++;
    EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
    EXPECT_EQ(encoding, SCO_ENCODING_BITS_16);
    EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_8);
    callback(cookie, ZX_OK);
  });

  int reset_count = 0;
  set_reset_sco_cb([&](bt_hci_reset_sco_callback callback, void* cookie) {
    reset_count++;
    callback(cookie, ZX_OK);
  });

  hci_spec::SynchronousConnectionParameters params = kCvsdConnectionParameters;
  params.input_coded_data_size_bits = 16;
  params.output_coded_data_size_bits = 16;
  // Bandwidth = sample size (2 bytes/sample) * sample rate (8000 samples/sec) = 16000 bytes/sec
  params.output_bandwidth = 16000;
  params.input_bandwidth = 16000;
  FakeScoConnection connection(sco_data_channel(), kConnectionHandle0, params);
  sco_data_channel()->RegisterConnection(connection.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
}

TEST_F(ScoDataChannelTest, CvsdConnectionEncodingBits16SampleRate16Khz) {
  int config_count = 0;
  set_configure_sco_cb([&](sco_coding_format_t format, sco_encoding_t encoding,
                           sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                           void* cookie) {
    config_count++;
    EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
    EXPECT_EQ(encoding, SCO_ENCODING_BITS_16);
    EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_16);
    callback(cookie, ZX_OK);
  });

  int reset_count = 0;
  set_reset_sco_cb([&](bt_hci_reset_sco_callback callback, void* cookie) {
    reset_count++;
    callback(cookie, ZX_OK);
  });

  hci_spec::SynchronousConnectionParameters params = kCvsdConnectionParameters;
  params.input_coded_data_size_bits = 16;
  params.output_coded_data_size_bits = 16;
  // Bandwidth = sample size (2 bytes/sample) * sample rate (16,000 samples/sec) = 32,000 bytes/sec
  params.output_bandwidth = 32000;
  params.input_bandwidth = 32000;
  FakeScoConnection connection(sco_data_channel(), kConnectionHandle0, params);
  sco_data_channel()->RegisterConnection(connection.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
}

TEST_F(ScoDataChannelTest, CvsdConnectionInvalidSampleSizeAndRate) {
  int config_count = 0;
  set_configure_sco_cb([&](sco_coding_format_t format, sco_encoding_t encoding,
                           sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                           void* cookie) {
    config_count++;
    EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
    EXPECT_EQ(encoding, SCO_ENCODING_BITS_16);
    EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_16);
    callback(cookie, ZX_OK);
  });

  int reset_count = 0;
  set_reset_sco_cb([&](bt_hci_reset_sco_callback callback, void* cookie) {
    reset_count++;
    callback(cookie, ZX_OK);
  });

  hci_spec::SynchronousConnectionParameters params = kCvsdConnectionParameters;
  // Invalid sample size will be replaced with sample size of 16 bits.
  params.input_coded_data_size_bits = 0u;
  params.output_coded_data_size_bits = 0u;
  // Invalid rate will be replaced with 16kHz
  params.output_bandwidth = 1;
  params.input_bandwidth = 1;
  FakeScoConnection connection(sco_data_channel(), kConnectionHandle0, params);
  sco_data_channel()->RegisterConnection(connection.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
}

TEST_F(ScoDataChannelTest, ConfigureCallbackCalledAfterTransportDestroyedDoesNotUseAfterFree) {
  bt_hci_configure_sco_callback config_cb = nullptr;
  void* config_cb_cookie = nullptr;
  set_configure_sco_cb([&](sco_coding_format_t format, sco_encoding_t encoding,
                           sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                           void* cookie) {
    config_cb = callback;
    config_cb_cookie = cookie;
  });

  int reset_count = 0;
  set_reset_sco_cb([&](bt_hci_reset_sco_callback callback, void* cookie) {
    reset_count++;
    callback(cookie, ZX_OK);
  });

  FakeScoConnection connection(sco_data_channel());
  sco_data_channel()->RegisterConnection(connection.GetWeakPtr());
  EXPECT_TRUE(config_cb);
  EXPECT_EQ(reset_count, 0);

  DeleteTransport();
  RunLoopUntilIdle();

  // Callback should not use-after-free.
  config_cb(config_cb_cookie, ZX_OK);
  RunLoopUntilIdle();
}

TEST_F(ScoDataChannelTest,
       RegisterAndUnregisterFirstConnectionAndRegisterSecondConnectionBeforeFirstConfigCompletes) {
  std::vector<std::pair<bt_hci_configure_sco_callback, void*>> config_callbacks;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  set_configure_sco_cb([&](sco_coding_format_t format, sco_encoding_t encoding,
                           sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                           void* cookie) { config_callbacks.emplace_back(callback, cookie); });

  int reset_count = 0;
  set_reset_sco_cb([&](bt_hci_reset_sco_callback callback, void* cookie) {
    reset_count++;
    callback(cookie, ZX_OK);
  });

  FakeScoConnection connection_0(sco_data_channel());
  sco_data_channel()->RegisterConnection(connection_0.GetWeakPtr());
  EXPECT_EQ(config_callbacks.size(), 1u);
  sco_data_channel()->UnregisterConnection(connection_0.handle());
  EXPECT_EQ(reset_count, 1);

  FakeScoConnection connection_1(sco_data_channel(), kConnectionHandle1);
  auto packet = StaticByteBuffer(LowerBits(kConnectionHandle1), UpperBits(kConnectionHandle1),
                                 0x01,  // payload length
                                 0x00   // payload
  );
  std::unique_ptr<ScoDataPacket> sco_packet = ScoDataPacket::New(/*payload_size=*/1);
  sco_packet->mutable_view()->mutable_data().Write(packet);
  sco_packet->InitializeFromBuffer();
  connection_1.QueuePacket(std::move(sco_packet));

  sco_data_channel()->RegisterConnection(connection_1.GetWeakPtr());
  EXPECT_EQ(config_callbacks.size(), 2u);
  // sco_packet should not be sent yet.
  RunLoopUntilIdle();
  // The first callback completing should not complete the second connection configuration.
  config_callbacks[0].first(config_callbacks[0].second, ZX_OK);
  // sco_packet should not be sent yet.
  RunLoopUntilIdle();
  EXPECT_EQ(connection_1.queued_packets().size(), 1u);
  // Queued packet should be sent after second callback called.
  config_callbacks[1].first(config_callbacks[1].second, ZX_OK);
  EXPECT_SCO_PACKET_OUT(test_device(), packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());
}

TEST_F(ScoDataChannelSingleConnectionTest,
       ReceiveNumberOfCompletedPacketsEventWithInconsistentNumberOfHandles) {
  // Queue 1 more than than the max number of packets (1 packet will remain queued).
  for (size_t i = 0; i <= kBufferMaxNumPackets; i++) {
    std::unique_ptr<ScoDataPacket> packet =
        ScoDataPacket::New(kConnectionHandle0, /*payload_size=*/1);
    packet->mutable_view()->mutable_payload_data()[0] = static_cast<uint8_t>(i);

    // The last packet should remain queued.
    if (i < kBufferMaxNumPackets) {
      EXPECT_SCO_PACKET_OUT(test_device(), StaticByteBuffer(LowerBits(kConnectionHandle0),
                                                            UpperBits(kConnectionHandle0),
                                                            0x01,  // payload length
                                                            static_cast<uint8_t>(i)));
    }
    connection()->QueuePacket(std::move(packet));
    RunLoopUntilIdle();
  }
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());

  // The handle in the event should still be processed even though the number of handles is wrong.
  EXPECT_SCO_PACKET_OUT(
      test_device(), StaticByteBuffer(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0),
                                      0x01,  // payload length
                                      static_cast<uint8_t>(kBufferMaxNumPackets)));

  constexpr uint16_t num_packets = 1;
  StaticByteBuffer event{0x13,
                         0x05,  // Number Of Completed Packet HCI event header, parameters length
                         0x09,  // Incorrect number of handles
                         LowerBits(kConnectionHandle0),
                         UpperBits(kConnectionHandle0),
                         LowerBits(num_packets),
                         UpperBits(num_packets)};
  test_device()->SendCommandChannelPacket(event);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());
}

TEST_F(ScoDataChannelTest, RegisterTwoConnectionsAndFirstConfigurationFails) {
  int config_count = 0;
  set_configure_sco_cb([&](sco_coding_format_t format, sco_encoding_t encoding,
                           sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                           void* cookie) {
    config_count++;
    if (config_count == 1) {
      callback(cookie, ZX_ERR_INVALID_ARGS);
      return;
    }
    callback(cookie, ZX_OK);
  });

  int reset_count = 0;
  set_reset_sco_cb([&](bt_hci_reset_sco_callback callback, void* cookie) {
    reset_count++;
    callback(cookie, ZX_OK);
  });

  FakeScoConnection connection_0(sco_data_channel());
  sco_data_channel()->RegisterConnection(connection_0.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
  EXPECT_EQ(connection_0.hci_error_count(), 0);

  FakeScoConnection connection_1(sco_data_channel(), kConnectionHandle1);
  sco_data_channel()->RegisterConnection(connection_1.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);

  // The first configuration error should be processed & the configuration of connection_1 should
  // succeed.
  RunLoopUntilIdle();
  EXPECT_EQ(connection_0.hci_error_count(), 1);
  EXPECT_EQ(config_count, 2);
  EXPECT_EQ(reset_count, 0);

  auto packet_0 = StaticByteBuffer(LowerBits(kConnectionHandle0), UpperBits(kConnectionHandle0),
                                   0x01,  // payload length
                                   0x00   // payload
  );
  test_device()->SendScoDataChannelPacket(packet_0);
  RunLoopUntilIdle();
  // packet_0 should not be received since connection_0 failed configuration and was unregistered.
  ASSERT_EQ(connection_0.received_packets().size(), 0u);

  auto packet_1 = StaticByteBuffer(LowerBits(kConnectionHandle1), UpperBits(kConnectionHandle1),
                                   0x01,  // payload length
                                   0x01   // payload
  );
  test_device()->SendScoDataChannelPacket(packet_1);
  RunLoopUntilIdle();
  ASSERT_EQ(connection_1.received_packets().size(), 1u);

  // There are no active connections now (+1 to reset_count).
  sco_data_channel()->UnregisterConnection(connection_1.handle());
  EXPECT_EQ(config_count, 2);
  EXPECT_EQ(reset_count, 1);
}

TEST_F(ScoDataChannelTest, UnsupportedCodingFormatTreatedAsCvsd) {
  int config_count = 0;
  set_configure_sco_cb([&](sco_coding_format_t format, sco_encoding_t encoding,
                           sco_sample_rate_t rate, bt_hci_configure_sco_callback callback,
                           void* cookie) {
    config_count++;
    EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
    callback(cookie, ZX_OK);
  });

  int reset_count = 0;
  set_reset_sco_cb([&](bt_hci_reset_sco_callback callback, void* cookie) {
    reset_count++;
    callback(cookie, ZX_OK);
  });

  hci_spec::SynchronousConnectionParameters params = kCvsdConnectionParameters;
  params.output_coding_format.coding_format = hci_spec::CodingFormat::kMuLaw;
  params.input_coding_format.coding_format = hci_spec::CodingFormat::kMuLaw;

  FakeScoConnection connection_0(sco_data_channel(), kConnectionHandle0, params);
  sco_data_channel()->RegisterConnection(connection_0.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
}

}  // namespace
}  // namespace bt::hci
