// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_channel.h"

#include <gtest/gtest.h>

#include "pw_bluetooth/controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::hci {
namespace {

using ScoCodingFormat = pw::bluetooth::Controller::ScoCodingFormat;
using ScoEncoding = pw::bluetooth::Controller::ScoEncoding;
using ScoSampleRate = pw::bluetooth::Controller::ScoSampleRate;

constexpr hci_spec::ConnectionHandle kConnectionHandle0 = 0x0000;
constexpr hci_spec::ConnectionHandle kConnectionHandle1 = 0x0001;
constexpr size_t kBufferMaxNumPackets = 2;

bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> MsbcConnectionParams() {
  bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> out;
  auto view = out.view();
  view.transmit_bandwidth().Write(0);
  view.receive_bandwidth().Write(0);
  view.transmit_coding_format().coding_format().Write(hci_spec::CodingFormat::MSBC);
  view.transmit_coding_format().company_id().Write(0);
  view.transmit_coding_format().vendor_codec_id().Write(0);
  view.receive_coding_format().coding_format().Write(hci_spec::CodingFormat::MSBC);
  view.receive_coding_format().company_id().Write(0);
  view.receive_coding_format().vendor_codec_id().Write(0);
  view.transmit_codec_frame_size_bytes().Write(0);
  view.receive_codec_frame_size_bytes().Write(0);
  view.input_bandwidth().Write(32000);
  view.output_bandwidth().Write(32000);
  view.input_coding_format().coding_format().Write(hci_spec::CodingFormat::MSBC);
  view.input_coding_format().company_id().Write(0);
  view.input_coding_format().vendor_codec_id().Write(0);
  view.output_coding_format().coding_format().Write(hci_spec::CodingFormat::MSBC);
  view.output_coding_format().company_id().Write(0);
  view.output_coding_format().vendor_codec_id().Write(0);
  view.input_coded_data_size_bits().Write(16);
  view.output_coded_data_size_bits().Write(16);
  view.input_pcm_data_format().Write(hci_spec::PcmDataFormat::UNSIGNED);
  view.output_pcm_data_format().Write(hci_spec::PcmDataFormat::UNSIGNED);
  view.input_pcm_sample_payload_msb_position().Write(0);
  view.output_pcm_sample_payload_msb_position().Write(0);
  view.input_data_path().Write(hci_spec::ScoDataPath::HCI);
  view.output_data_path().Write(hci_spec::ScoDataPath::HCI);
  view.input_transport_unit_size_bits().Write(0);
  view.output_transport_unit_size_bits().Write(0);
  view.max_latency_ms().Write(0);
  view.packet_types().BackingStorage().WriteUInt(0);
  view.retransmission_effort().Write(
      hci_spec::SynchronousConnectionParameters::ScoRetransmissionEffort::NONE);
  return out;
}

bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> cvsd_connection_params() {
  bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> out;
  auto view = out.view();
  view.transmit_bandwidth().Write(0);
  view.receive_bandwidth().Write(0);
  view.transmit_coding_format().coding_format().Write(hci_spec::CodingFormat::CVSD);
  view.transmit_coding_format().company_id().Write(0);
  view.transmit_coding_format().vendor_codec_id().Write(0);
  view.receive_coding_format().coding_format().Write(hci_spec::CodingFormat::CVSD);
  view.receive_coding_format().company_id().Write(0);
  view.receive_coding_format().vendor_codec_id().Write(0);
  view.transmit_codec_frame_size_bytes().Write(0);
  view.receive_codec_frame_size_bytes().Write(0);
  view.input_bandwidth().Write(8000);
  view.output_bandwidth().Write(8000);
  view.input_coding_format().coding_format().Write(hci_spec::CodingFormat::CVSD);
  view.input_coding_format().company_id().Write(0);
  view.input_coding_format().vendor_codec_id().Write(0);
  view.output_coding_format().coding_format().Write(hci_spec::CodingFormat::CVSD);
  view.output_coding_format().company_id().Write(0);
  view.output_coding_format().vendor_codec_id().Write(0);
  view.input_coded_data_size_bits().Write(8);
  view.output_coded_data_size_bits().Write(8);
  view.input_pcm_data_format().Write(hci_spec::PcmDataFormat::UNSIGNED);
  view.output_pcm_data_format().Write(hci_spec::PcmDataFormat::UNSIGNED);
  view.input_pcm_sample_payload_msb_position().Write(0);
  view.output_pcm_sample_payload_msb_position().Write(0);
  view.input_data_path().Write(hci_spec::ScoDataPath::HCI);
  view.output_data_path().Write(hci_spec::ScoDataPath::HCI);
  view.input_transport_unit_size_bits().Write(0);
  view.output_transport_unit_size_bits().Write(0);
  view.max_latency_ms().Write(0);
  view.packet_types().BackingStorage().WriteUInt(0);
  view.retransmission_effort().Write(
      hci_spec::SynchronousConnectionParameters::ScoRetransmissionEffort::NONE);
  return out;
}
class FakeScoConnection : public ScoDataChannel::ConnectionInterface {
 public:
  explicit FakeScoConnection(ScoDataChannel* data_channel,
                             hci_spec::ConnectionHandle handle = kConnectionHandle0,
                             bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter>
                                 params = MsbcConnectionParams())
      : handle_(handle),
        params_(std::move(params)),
        data_channel_(data_channel),
        weak_ptr_factory_(this) {}

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

  bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> parameters() override {
    return params_;
  }

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
  bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> params_;
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

    DataBufferInfo buffer_info(/*max_data_length=*/10, kBufferMaxNumPackets);
    InitializeScoDataChannel(buffer_info);
  }
};

class ScoDataChannelSingleConnectionTest : public ScoDataChannelTest {
 public:
  void SetUp() override {
    ScoDataChannelTest::SetUp();

    test_device()->set_configure_sco_cb([this](ScoCodingFormat format, ScoEncoding encoding,
                                               ScoSampleRate rate,
                                               fit::callback<void(pw::Status)> callback) {
      config_count_++;
      EXPECT_EQ(format, ScoCodingFormat::kMsbc);
      EXPECT_EQ(encoding, ScoEncoding::k16Bits);
      EXPECT_EQ(rate, ScoSampleRate::k16Khz);
      callback(PW_STATUS_OK);
    });

    test_device()->set_reset_sco_cb([this](fit::callback<void(pw::Status)> callback) {
      reset_count_++;
      callback(PW_STATUS_OK);
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
    test_device()->set_configure_sco_cb(nullptr);
    test_device()->set_reset_sco_cb(nullptr);
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
      bt::testing::NumberOfCompletedPacketsPacket(kConnectionHandle0, 1));
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
  test_device()->set_configure_sco_cb(
      [&](auto, auto, auto, fit::callback<void(pw::Status)> callback) {
        config_count++;
        callback(PW_STATUS_OK);
      });

  int reset_count = 0;
  test_device()->set_reset_sco_cb([&](fit::callback<void(pw::Status)> callback) {
    reset_count++;
    callback(PW_STATUS_OK);
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
      bt::testing::NumberOfCompletedPacketsPacket(kConnectionHandle0, 1));

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
      bt::testing::NumberOfCompletedPacketsPacket(kConnectionHandle1, 1));

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
  test_device()->set_configure_sco_cb(
      [](auto, auto, auto, fit::callback<void(pw::Status)> cb) { cb(PW_STATUS_OK); });
  test_device()->set_reset_sco_cb([](fit::callback<void(pw::Status)> cb) { cb(PW_STATUS_OK); });

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
      bt::testing::NumberOfCompletedPacketsPacket(kConnectionHandle1, 1));
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

  test_device()->set_configure_sco_cb([&](ScoCodingFormat format, ScoEncoding encoding,
                                          ScoSampleRate rate,
                                          fit::callback<void(pw::Status)> callback) {
    config_count++;
    EXPECT_EQ(format, ScoCodingFormat::kCvsd);
    EXPECT_EQ(encoding, ScoEncoding::k8Bits);
    EXPECT_EQ(rate, ScoSampleRate::k8Khz);
    callback(PW_STATUS_OK);
  });

  int reset_count = 0;
  test_device()->set_reset_sco_cb([&](fit::callback<void(pw::Status)> callback) {
    reset_count++;
    callback(PW_STATUS_OK);
  });

  FakeScoConnection connection_0(sco_data_channel(), kConnectionHandle0, cvsd_connection_params());
  sco_data_channel()->RegisterConnection(connection_0.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
}

TEST_F(ScoDataChannelTest, CvsdConnectionEncodingBits16SampleRate8Khz) {
  int config_count = 0;
  test_device()->set_configure_sco_cb([&](ScoCodingFormat format, ScoEncoding encoding,
                                          ScoSampleRate rate,
                                          fit::callback<void(pw::Status)> callback) {
    config_count++;
    EXPECT_EQ(format, ScoCodingFormat::kCvsd);
    EXPECT_EQ(encoding, ScoEncoding::k16Bits);
    EXPECT_EQ(rate, ScoSampleRate::k8Khz);
    callback(PW_STATUS_OK);
  });

  int reset_count = 0;
  test_device()->set_reset_sco_cb([&](fit::callback<void(pw::Status)> callback) {
    reset_count++;
    callback(PW_STATUS_OK);
  });

  bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> params =
      cvsd_connection_params();
  auto view = params.view();
  view.input_coded_data_size_bits().Write(16);
  view.output_coded_data_size_bits().Write(16);
  // Bandwidth = sample size (2 bytes/sample) * sample rate (8000 samples/sec) = 16000 bytes/sec
  view.output_bandwidth().Write(16000);
  view.input_bandwidth().Write(16000);
  FakeScoConnection connection(sco_data_channel(), kConnectionHandle0, params);
  sco_data_channel()->RegisterConnection(connection.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
}

TEST_F(ScoDataChannelTest, CvsdConnectionEncodingBits16SampleRate16Khz) {
  int config_count = 0;
  test_device()->set_configure_sco_cb([&](ScoCodingFormat format, ScoEncoding encoding,
                                          ScoSampleRate rate,
                                          fit::callback<void(pw::Status)> callback) {
    config_count++;
    EXPECT_EQ(format, ScoCodingFormat::kCvsd);
    EXPECT_EQ(encoding, ScoEncoding::k16Bits);
    EXPECT_EQ(rate, ScoSampleRate::k16Khz);
    callback(PW_STATUS_OK);
  });

  int reset_count = 0;
  test_device()->set_reset_sco_cb([&](fit::callback<void(pw::Status)> callback) {
    reset_count++;
    callback(PW_STATUS_OK);
  });

  bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> params =
      cvsd_connection_params();
  auto view = params.view();
  view.input_coded_data_size_bits().Write(16);
  view.output_coded_data_size_bits().Write(16);
  // Bandwidth = sample size (2 bytes/sample) * sample rate (16,000 samples/sec) = 32,000 bytes/sec
  view.output_bandwidth().Write(32000);
  view.input_bandwidth().Write(32000);
  FakeScoConnection connection(sco_data_channel(), kConnectionHandle0, params);
  sco_data_channel()->RegisterConnection(connection.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
}

TEST_F(ScoDataChannelTest, CvsdConnectionInvalidSampleSizeAndRate) {
  int config_count = 0;
  test_device()->set_configure_sco_cb([&](ScoCodingFormat format, ScoEncoding encoding,
                                          ScoSampleRate rate,
                                          fit::callback<void(pw::Status)> callback) {
    config_count++;
    EXPECT_EQ(format, ScoCodingFormat::kCvsd);
    EXPECT_EQ(encoding, ScoEncoding::k16Bits);
    EXPECT_EQ(rate, ScoSampleRate::k16Khz);
    callback(PW_STATUS_OK);
  });

  int reset_count = 0;
  test_device()->set_reset_sco_cb([&](fit::callback<void(pw::Status)> callback) {
    reset_count++;
    callback(PW_STATUS_OK);
  });

  bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> params =
      cvsd_connection_params();
  auto view = params.view();
  // Invalid sample size will be replaced with sample size of 16 bits.
  view.input_coded_data_size_bits().Write(0u);
  view.output_coded_data_size_bits().Write(0u);
  // Invalid rate will be replaced with 16kHz.
  view.output_bandwidth().Write(1);
  view.input_bandwidth().Write(1);
  FakeScoConnection connection(sco_data_channel(), kConnectionHandle0, params);
  sco_data_channel()->RegisterConnection(connection.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
}

TEST_F(ScoDataChannelTest, ConfigureCallbackCalledAfterTransportDestroyedDoesNotUseAfterFree) {
  fit::callback<void(pw::Status)> config_cb = nullptr;
  test_device()->set_configure_sco_cb(
      [&](ScoCodingFormat format, ScoEncoding encoding, ScoSampleRate rate,
          fit::callback<void(pw::Status)> callback) { config_cb = std::move(callback); });

  int reset_count = 0;
  test_device()->set_reset_sco_cb([&](fit::callback<void(pw::Status)> callback) {
    reset_count++;
    callback(PW_STATUS_OK);
  });

  FakeScoConnection connection(sco_data_channel());
  sco_data_channel()->RegisterConnection(connection.GetWeakPtr());
  EXPECT_TRUE(config_cb);
  EXPECT_EQ(reset_count, 0);

  DeleteTransport();
  RunLoopUntilIdle();

  // Callback should not use-after-free.
  config_cb(PW_STATUS_OK);
  RunLoopUntilIdle();
}

TEST_F(ScoDataChannelTest,
       RegisterAndUnregisterFirstConnectionAndRegisterSecondConnectionBeforeFirstConfigCompletes) {
  std::vector<fit::callback<void(pw::Status)>> config_callbacks;
  test_device()->set_configure_sco_cb(
      [&](auto, auto, auto, fit::callback<void(pw::Status)> callback) {
        config_callbacks.emplace_back(std::move(callback));
      });

  int reset_count = 0;
  test_device()->set_reset_sco_cb([&](fit::callback<void(pw::Status)> callback) {
    reset_count++;
    callback(PW_STATUS_OK);
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
  config_callbacks[0](PW_STATUS_OK);
  // sco_packet should not be sent yet.
  RunLoopUntilIdle();
  EXPECT_EQ(connection_1.queued_packets().size(), 1u);
  // Queued packet should be sent after second callback called.
  config_callbacks[1](PW_STATUS_OK);
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
  test_device()->set_configure_sco_cb([&](ScoCodingFormat format, ScoEncoding encoding,
                                          ScoSampleRate rate,
                                          fit::callback<void(pw::Status)> callback) {
    config_count++;
    if (config_count == 1) {
      callback(pw::Status::InvalidArgument());
      return;
    }
    callback(PW_STATUS_OK);
  });

  int reset_count = 0;
  test_device()->set_reset_sco_cb([&](fit::callback<void(pw::Status)> callback) {
    reset_count++;
    callback(PW_STATUS_OK);
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
  test_device()->set_configure_sco_cb([&](ScoCodingFormat format, ScoEncoding encoding,
                                          ScoSampleRate rate,
                                          fit::callback<void(pw::Status)> callback) {
    config_count++;
    EXPECT_EQ(format, ScoCodingFormat::kCvsd);
    callback(PW_STATUS_OK);
  });

  int reset_count = 0;
  test_device()->set_reset_sco_cb([&](fit::callback<void(pw::Status)> callback) {
    reset_count++;
    callback(PW_STATUS_OK);
  });

  bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> params =
      cvsd_connection_params();
  auto view = params.view();
  view.output_coding_format().coding_format().Write(hci_spec::CodingFormat::U_LAW);
  view.input_coding_format().coding_format().Write(hci_spec::CodingFormat::U_LAW);
  FakeScoConnection connection_0(sco_data_channel(), kConnectionHandle0, params);
  sco_data_channel()->RegisterConnection(connection_0.GetWeakPtr());
  EXPECT_EQ(config_count, 1);
  EXPECT_EQ(reset_count, 0);
}

TEST_F(ScoDataChannelSingleConnectionTest, NumberOfCompletedPacketsExceedsPendingPackets) {
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
      bt::testing::NumberOfCompletedPacketsPacket(kConnectionHandle0, kBufferMaxNumPackets + 1));
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());
}

}  // namespace
}  // namespace bt::hci
