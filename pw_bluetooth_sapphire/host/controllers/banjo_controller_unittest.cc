// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "banjo_controller.h"

#include <ddktl/device.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/slab_allocators.h"
#include "src/devices/testing/mock-ddk/mock-device.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::controllers {

using FeaturesBits = pw::bluetooth::Controller::FeaturesBits;
using ScoCodingFormat = pw::bluetooth::Controller::ScoCodingFormat;
using ScoSampleRate = pw::bluetooth::Controller::ScoSampleRate;
using ScoEncoding = pw::bluetooth::Controller::ScoEncoding;
using pw::bluetooth::AclPriority;

constexpr hci_spec::ConnectionHandle kConnectionHandle = 0x0001;

const StaticByteBuffer kSetAclPriorityNormalCommand(0x01);
const StaticByteBuffer kSetAclPrioritySourceCommand(0x02);
const StaticByteBuffer kSetAclPrioritySinkCommand(0x03);

class FakeDevice : public ddk::BtHciProtocol<FakeDevice>, public ddk::BtVendorProtocol<FakeDevice> {
 public:
  using ConfigureScoCallback =
      fit::function<void(sco_coding_format_t, sco_encoding_t, sco_sample_rate_t,
                         bt_hci_configure_sco_callback, void*)>;
  using ResetScoCallback = fit::function<void(bt_hci_reset_sco_callback, void*)>;

  FakeDevice(async_dispatcher_t* dispatcher) : dispatcher_(dispatcher) {}

  zx_status_t SendEvent(BufferView event) {
    return command_channel_.write(/*flags=*/0, event.data(), static_cast<uint32_t>(event.size()),
                                  /*handles=*/nullptr, /*num_handles=*/0);
  }
  zx_status_t SendAcl(BufferView buffer) {
    return acl_channel_.write(/*flags=*/0, buffer.data(), static_cast<uint32_t>(buffer.size()),
                              /*handles=*/nullptr, /*num_handles=*/0);
  }

  zx_status_t SendSco(BufferView buffer) {
    return sco_channel_.write(/*flags=*/0, buffer.data(), static_cast<uint32_t>(buffer.size()),
                              /*handles=*/nullptr, /*num_handles=*/0);
  }

  void ResetCommandChannel() { command_channel_.reset(); }

  const std::vector<bt::DynamicByteBuffer>& commands_received() const { return commands_received_; }
  const std::vector<bt::DynamicByteBuffer>& acl_packets_received() const {
    return acl_packets_received_;
  }
  const std::vector<bt::DynamicByteBuffer>& sco_packets_received() const {
    return sco_packets_received_;
  }

  bt_hci_protocol_t hci_proto() const {
    bt_hci_protocol_t proto;
    proto.ctx = const_cast<FakeDevice*>(this);
    proto.ops = const_cast<bt_hci_protocol_ops_t*>(&bt_hci_protocol_ops_);
    return proto;
  }

  bt_vendor_protocol_t vendor_proto() const {
    bt_vendor_protocol_t proto;
    proto.ctx = const_cast<FakeDevice*>(this);
    proto.ops = const_cast<bt_vendor_protocol_ops_t*>(&bt_vendor_protocol_ops_);
    return proto;
  }

  void set_sco_supported(bool supported) { sco_supported_ = supported; }

  void set_command_channel_supported(bool supported) { command_channel_supported_ = supported; }

  void set_acl_channel_supported(bool supported) { acl_channel_supported_ = supported; }

  void set_features(bt_vendor_features_t features) { features_ = features; }

  void set_encode_command_status(zx_status_t status) { encode_command_status_ = status; }

  void set_configure_sco_callback(ConfigureScoCallback callback) {
    configure_sco_cb_ = std::move(callback);
  }

  void set_reset_sco_callback(ResetScoCallback callback) { reset_sco_cb_ = std::move(callback); }

  bool command_channel_is_valid() const { return command_channel_.is_valid(); }
  bool acl_channel_is_valid() const { return acl_channel_.is_valid(); }
  bool sco_channel_is_valid() const { return sco_channel_.is_valid(); }

  // ddk::BtHciProtocol mixins:

  zx_status_t BtHciOpenCommandChannel(zx::channel in) {
    if (!command_channel_supported_) {
      return ZX_ERR_NOT_SUPPORTED;
    }
    command_channel_ = std::move(in);
    InitializeWait(command_wait_, &command_channel_);
    return ZX_OK;
  }

  zx_status_t BtHciOpenAclDataChannel(zx::channel in) {
    if (!acl_channel_supported_) {
      return ZX_ERR_NOT_SUPPORTED;
    }
    acl_channel_ = std::move(in);
    InitializeWait(acl_wait_, &acl_channel_);
    return ZX_OK;
  }

  zx_status_t BtHciOpenScoChannel(zx::channel in) {
    if (!sco_supported_) {
      return ZX_ERR_NOT_SUPPORTED;
    }
    sco_channel_ = std::move(in);
    InitializeWait(sco_wait_, &sco_channel_);
    return ZX_OK;
  }

  void BtHciConfigureSco(sco_coding_format_t coding_format, sco_encoding_t encoding,
                         sco_sample_rate_t sample_rate, bt_hci_configure_sco_callback callback,
                         void* cookie) {
    if (configure_sco_cb_) {
      configure_sco_cb_(coding_format, encoding, sample_rate, callback, cookie);
    }
  }

  void BtHciResetSco(bt_hci_reset_sco_callback callback, void* cookie) {
    if (reset_sco_cb_) {
      reset_sco_cb_(callback, cookie);
    }
  }

  zx_status_t BtHciOpenSnoopChannel(zx::channel in) { return ZX_ERR_NOT_SUPPORTED; }

  // ddk::BtVendorProtocol mixins:

  bt_vendor_features_t BtVendorGetFeatures() { return features_; }

  zx_status_t BtVendorEncodeCommand(bt_vendor_command_t command, const bt_vendor_params_t* params,
                                    uint8_t* out_encoded_buffer, size_t encoded_size,
                                    size_t* out_encoded_actual) {
    if (command != BT_VENDOR_COMMAND_SET_ACL_PRIORITY) {
      return ZX_ERR_NOT_SUPPORTED;
    }
    bt_vendor_set_acl_priority_params_t acl_params = params->set_acl_priority;
    EXPECT_EQ(acl_params.connection_handle, kConnectionHandle);
    MutableBufferView out_buffer_view(out_encoded_buffer, encoded_size);
    *out_encoded_actual = kSetAclPriorityNormalCommand.size();
    if (acl_params.priority == BT_VENDOR_ACL_PRIORITY_NORMAL) {
      kSetAclPriorityNormalCommand.Copy(&out_buffer_view);
    } else if (acl_params.direction == BT_VENDOR_ACL_DIRECTION_SOURCE) {
      kSetAclPrioritySourceCommand.Copy(&out_buffer_view);
    } else {
      kSetAclPrioritySinkCommand.Copy(&out_buffer_view);
    }
    return encode_command_status_;
  }

 private:
  void InitializeWait(async::WaitBase& wait, zx::channel* channel) {
    BT_ASSERT(channel->is_valid());
    wait.Cancel();
    wait.set_object(channel->get());
    wait.set_trigger(ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED);
    BT_ASSERT(wait.Begin(dispatcher_) == ZX_OK);
  }

  void OnChannelSignal(zx::channel* channel, async::WaitBase* wait,
                       const zx_packet_signal_t* signal,
                       std::vector<DynamicByteBuffer>* out_vector) {
    if (signal->observed & ZX_CHANNEL_PEER_CLOSED) {
      channel->reset();
      return;
    }
    ASSERT_TRUE(signal->observed & ZX_CHANNEL_READABLE);

    // ACL packets are larger than all other packets.
    bt::StaticByteBuffer<hci::allocators::kLargeACLDataPacketSize> buffer;
    uint32_t read_size = 0;
    zx_status_t read_status = channel->read(0u, buffer.mutable_data(), /*handles=*/nullptr,
                                            static_cast<uint32_t>(buffer.size()), 0, &read_size,
                                            /*actual_handles=*/nullptr);
    ASSERT_TRUE(read_status == ZX_OK);
    out_vector->emplace_back(bt::BufferView(buffer, read_size));
    wait->Begin(dispatcher_);
  }

  void OnAclSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                   const zx_packet_signal_t* signal) {
    ASSERT_TRUE(status == ZX_OK);
    OnChannelSignal(&acl_channel_, wait, signal, &acl_packets_received_);
  }

  void OnCommandSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                       const zx_packet_signal_t* signal) {
    ASSERT_TRUE(status == ZX_OK);
    OnChannelSignal(&command_channel_, wait, signal, &commands_received_);
  }

  void OnScoSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                   const zx_packet_signal_t* signal) {
    ASSERT_TRUE(status == ZX_OK);
    OnChannelSignal(&sco_channel_, wait, signal, &sco_packets_received_);
  }

  bt_vendor_features_t features_{0};
  bool sco_supported_ = true;
  bool command_channel_supported_ = true;
  bool acl_channel_supported_ = true;
  zx_status_t encode_command_status_ = ZX_OK;
  ConfigureScoCallback configure_sco_cb_;
  ResetScoCallback reset_sco_cb_;

  zx::channel command_channel_;
  async::WaitMethod<FakeDevice, &FakeDevice::OnCommandSignal> command_wait_{this};
  std::vector<bt::DynamicByteBuffer> commands_received_;

  zx::channel acl_channel_;
  async::WaitMethod<FakeDevice, &FakeDevice::OnAclSignal> acl_wait_{this};
  std::vector<bt::DynamicByteBuffer> acl_packets_received_;

  zx::channel sco_channel_;
  async::WaitMethod<FakeDevice, &FakeDevice::OnScoSignal> sco_wait_{this};
  std::vector<bt::DynamicByteBuffer> sco_packets_received_;

  async_dispatcher_t* dispatcher_;
};

class BanjoControllerTest : public ::gtest::TestLoopFixture {
 public:
  void SetUp() { fake_device_.emplace(dispatcher()); }

  void InitializeController(bool vendor_supported = true) {
    auto hci_proto = fake_device_->hci_proto();
    auto vendor_proto = fake_device_->vendor_proto();
    std::optional<ddk::BtVendorProtocolClient> vendor_client =
        vendor_supported ? std::optional(ddk::BtVendorProtocolClient(&vendor_proto)) : std::nullopt;
    controller_.emplace(ddk::BtHciProtocolClient(&hci_proto), std::move(vendor_client),
                        dispatcher());

    std::optional<pw::Status> complete_status;
    controller_->Initialize(
        [&](pw::Status cb_complete_status) { complete_status = cb_complete_status; },
        [&](pw::Status cb_error) { controller_error_ = cb_error; });
    ASSERT_TRUE(complete_status.has_value());
    ASSERT_TRUE(complete_status.value().ok());
    ASSERT_FALSE(controller_error_.has_value());
  }

  void DestroyController() { controller_.reset(); }

  BanjoController* controller() { return &controller_.value(); }

  FakeDevice* fake_device() { return &fake_device_.value(); }

  std::optional<pw::Status> controller_error() const { return controller_error_; }

 private:
  std::optional<pw::Status> controller_error_;
  std::optional<FakeDevice> fake_device_;
  std::optional<BanjoController> controller_;
};

TEST_F(BanjoControllerTest, InitializeFailsDueToCommandChannelError) {
  FakeDevice fake_device(dispatcher());
  fake_device.set_command_channel_supported(false);
  auto hci_proto = fake_device.hci_proto();
  BanjoController controller(ddk::BtHciProtocolClient(&hci_proto), std::nullopt, dispatcher());

  std::optional<pw::Status> complete_status;
  std::optional<pw::Status> error;
  controller.Initialize(
      [&](pw::Status cb_complete_status) { complete_status = cb_complete_status; },
      [&](pw::Status cb_error) { error = cb_error; });
  ASSERT_TRUE(complete_status.has_value());
  ASSERT_TRUE(complete_status->IsInternal());
  ASSERT_FALSE(error.has_value());
}

TEST_F(BanjoControllerTest, InitializeFailsDueToAclChannelError) {
  FakeDevice fake_device(dispatcher());
  fake_device.set_acl_channel_supported(false);
  auto hci_proto = fake_device.hci_proto();
  BanjoController controller(ddk::BtHciProtocolClient(&hci_proto), std::nullopt, dispatcher());

  std::optional<pw::Status> complete_status;
  std::optional<pw::Status> error;
  controller.Initialize(
      [&](pw::Status cb_complete_status) { complete_status = cb_complete_status; },
      [&](pw::Status cb_error) { error = cb_error; });
  ASSERT_TRUE(complete_status.has_value());
  ASSERT_TRUE(complete_status->IsInternal());
  ASSERT_FALSE(error.has_value());
}

TEST_F(BanjoControllerTest, SendAndReceiveAcl) {
  RETURN_IF_FATAL(InitializeController());

  const StaticByteBuffer acl_packet_0(0x00, 0x01, 0x02, 0x03);
  controller()->SendAclData(acl_packet_0.subspan());
  RunLoopUntilIdle();
  ASSERT_EQ(fake_device()->acl_packets_received().size(), 1u);
  EXPECT_THAT(fake_device()->acl_packets_received()[0], BufferEq(acl_packet_0));

  const StaticByteBuffer acl_packet_1(0x04, 0x05, 0x06, 0x07);
  controller()->SendAclData(acl_packet_1.subspan());
  RunLoopUntilIdle();
  ASSERT_EQ(fake_device()->acl_packets_received().size(), 2u);
  EXPECT_THAT(fake_device()->acl_packets_received()[1], BufferEq(acl_packet_1));

  std::vector<DynamicByteBuffer> received_acl;
  controller()->SetReceiveAclFunction([&](pw::span<const std::byte> buffer) {
    received_acl.emplace_back(BufferView(buffer.data(), buffer.size()));
  });

  fake_device()->SendAcl(acl_packet_0.view());
  RunLoopUntilIdle();
  ASSERT_EQ(received_acl.size(), 1u);
  EXPECT_THAT(received_acl[0], BufferEq(acl_packet_0));

  fake_device()->SendAcl(acl_packet_1.view());
  RunLoopUntilIdle();
  ASSERT_EQ(received_acl.size(), 2u);
  EXPECT_THAT(received_acl[1], BufferEq(acl_packet_1));

  std::optional<pw::Status> close_status;
  controller()->Close([&](pw::Status status) { close_status = status; });
  ASSERT_TRUE(close_status.has_value());
  EXPECT_EQ(close_status.value(), PW_STATUS_OK);
}

TEST_F(BanjoControllerTest, SendCommandsAndReceiveEvents) {
  RETURN_IF_FATAL(InitializeController());

  const StaticByteBuffer packet_0(0x00, 0x01, 0x02, 0x03);
  controller()->SendCommand(packet_0.subspan());
  RunLoopUntilIdle();
  ASSERT_EQ(fake_device()->commands_received().size(), 1u);
  EXPECT_THAT(fake_device()->commands_received()[0], BufferEq(packet_0));

  const StaticByteBuffer packet_1(0x04, 0x05, 0x06, 0x07);
  controller()->SendCommand(packet_1.subspan());
  RunLoopUntilIdle();
  ASSERT_EQ(fake_device()->commands_received().size(), 2u);
  EXPECT_THAT(fake_device()->commands_received()[1], BufferEq(packet_1));

  std::vector<DynamicByteBuffer> events;
  controller()->SetEventFunction([&](pw::span<const std::byte> buffer) {
    events.emplace_back(BufferView(buffer.data(), buffer.size()));
  });

  fake_device()->SendEvent(packet_1.view());
  RunLoopUntilIdle();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_THAT(events[0], BufferEq(packet_1));

  fake_device()->SendEvent(packet_1.view());
  RunLoopUntilIdle();
  ASSERT_EQ(events.size(), 2u);
  EXPECT_THAT(events[1], BufferEq(packet_1));

  std::optional<pw::Status> close_status;
  controller()->Close([&](pw::Status status) { close_status = status; });
  ASSERT_TRUE(close_status.has_value());
  EXPECT_EQ(close_status.value(), PW_STATUS_OK);
}

TEST_F(BanjoControllerTest, SendAndReceiveSco) {
  RETURN_IF_FATAL(InitializeController());

  const StaticByteBuffer sco_packet_0(0x00, 0x01, 0x02, 0x03);
  controller()->SendScoData(sco_packet_0.subspan());
  RunLoopUntilIdle();
  ASSERT_EQ(fake_device()->sco_packets_received().size(), 1u);
  EXPECT_THAT(fake_device()->sco_packets_received()[0], BufferEq(sco_packet_0));

  const StaticByteBuffer sco_packet_1(0x04, 0x05, 0x06, 0x07);
  controller()->SendScoData(sco_packet_1.subspan());
  RunLoopUntilIdle();
  ASSERT_EQ(fake_device()->sco_packets_received().size(), 2u);
  EXPECT_THAT(fake_device()->sco_packets_received()[1], BufferEq(sco_packet_1));

  std::vector<DynamicByteBuffer> received_sco;
  controller()->SetReceiveScoFunction([&](pw::span<const std::byte> buffer) {
    received_sco.emplace_back(BufferView(buffer.data(), buffer.size()));
  });

  fake_device()->SendSco(sco_packet_0.view());
  RunLoopUntilIdle();
  ASSERT_EQ(received_sco.size(), 1u);
  EXPECT_THAT(received_sco[0], BufferEq(sco_packet_0));

  fake_device()->SendSco(sco_packet_1.view());
  RunLoopUntilIdle();
  ASSERT_EQ(received_sco.size(), 2u);
  EXPECT_THAT(received_sco[1], BufferEq(sco_packet_1));

  std::optional<pw::Status> close_status;
  controller()->Close([&](pw::Status status) { close_status = status; });
  ASSERT_TRUE(close_status.has_value());
  EXPECT_EQ(close_status.value(), PW_STATUS_OK);
}

TEST_F(BanjoControllerTest, GetFeatures) {
  fake_device()->set_sco_supported(true);
  fake_device()->set_features(BT_VENDOR_FEATURES_SET_ACL_PRIORITY_COMMAND |
                              BT_VENDOR_FEATURES_ANDROID_VENDOR_EXTENSIONS);
  InitializeController(/*vendor_supported=*/true);

  std::optional<FeaturesBits> features;
  controller()->GetFeatures([&](FeaturesBits f) { features = f; });
  ASSERT_TRUE(features.has_value());
  EXPECT_EQ(features.value(), FeaturesBits::kSetAclPriorityCommand |
                                  FeaturesBits::kAndroidVendorExtensions | FeaturesBits::kHciSco);
}

TEST_F(BanjoControllerTest, ScoNotSupported) {
  fake_device()->set_sco_supported(false);
  RETURN_IF_FATAL(InitializeController());
  std::optional<FeaturesBits> features;
  controller()->GetFeatures([&](FeaturesBits f) { features = f; });
  ASSERT_TRUE(features.has_value());
  EXPECT_EQ(features.value(), FeaturesBits{0});
}

TEST_F(BanjoControllerTest, EncodeSetAclPriorityCommandNormal) {
  RETURN_IF_FATAL(InitializeController());
  pw::bluetooth::VendorCommandParameters params = pw::bluetooth::SetAclPriorityCommandParameters{
      .connection_handle = kConnectionHandle, .priority = AclPriority::kNormal};
  std::optional<DynamicByteBuffer> buffer;
  controller()->EncodeVendorCommand(params, [&](pw::Result<pw::span<const std::byte>> result) {
    ASSERT_TRUE(result.ok());
    buffer.emplace(BufferView(result.value().data(), result.value().size()));
  });
  ASSERT_TRUE(buffer);
  EXPECT_THAT(*buffer, BufferEq(kSetAclPriorityNormalCommand));
}

TEST_F(BanjoControllerTest, EncodeSetAclPriorityCommandSink) {
  RETURN_IF_FATAL(InitializeController());
  pw::bluetooth::VendorCommandParameters params = pw::bluetooth::SetAclPriorityCommandParameters{
      .connection_handle = kConnectionHandle, .priority = AclPriority::kSink};
  std::optional<DynamicByteBuffer> buffer;
  controller()->EncodeVendorCommand(params, [&](pw::Result<pw::span<const std::byte>> result) {
    ASSERT_TRUE(result.ok());
    buffer.emplace(BufferView(result.value().data(), result.value().size()));
  });
  ASSERT_TRUE(buffer);
  EXPECT_THAT(*buffer, BufferEq(kSetAclPrioritySinkCommand));
}

TEST_F(BanjoControllerTest, EncodeSetAclPriorityCommandSource) {
  RETURN_IF_FATAL(InitializeController());
  pw::bluetooth::VendorCommandParameters params = pw::bluetooth::SetAclPriorityCommandParameters{
      .connection_handle = kConnectionHandle, .priority = AclPriority::kSource};
  std::optional<DynamicByteBuffer> buffer;
  controller()->EncodeVendorCommand(params, [&](pw::Result<pw::span<const std::byte>> result) {
    ASSERT_TRUE(result.ok());
    buffer.emplace(BufferView(result.value().data(), result.value().size()));
  });
  ASSERT_TRUE(buffer);
  EXPECT_THAT(*buffer, BufferEq(kSetAclPrioritySourceCommand));
}

TEST_F(BanjoControllerTest, EncodeSetAclPriorityCommandNotSupported) {
  fake_device()->set_encode_command_status(ZX_ERR_NOT_SUPPORTED);
  RETURN_IF_FATAL(InitializeController());

  pw::bluetooth::VendorCommandParameters params = pw::bluetooth::SetAclPriorityCommandParameters{
      .connection_handle = kConnectionHandle, .priority = AclPriority::kSource};
  std::optional<pw::Result<pw::span<const std::byte>>> result;
  controller()->EncodeVendorCommand(
      params, [&](pw::Result<pw::span<const std::byte>> cb_result) { result = cb_result; });
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->status(), pw::Status::Unimplemented());
}

TEST_F(BanjoControllerTest, ConfigureScoWithFormatCvsdEncoding8BitsSampleRate8Khz) {
  RETURN_IF_FATAL(InitializeController());
  int device_cb_count = 0;
  fake_device()->set_configure_sco_callback(
      [&](sco_coding_format_t format, sco_encoding_t encoding, sco_sample_rate_t rate,
          bt_hci_configure_sco_callback callback, void* cookie) {
        device_cb_count++;
        EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
        EXPECT_EQ(encoding, SCO_ENCODING_BITS_8);
        EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_8);
        callback(cookie, ZX_OK);
      });

  int controller_cb_count = 0;
  controller()->ConfigureSco(ScoCodingFormat::kCvsd, ScoEncoding::k8Bits, ScoSampleRate::k8Khz,
                             [&](pw::Status status) {
                               controller_cb_count++;
                               EXPECT_EQ(status, PW_STATUS_OK);
                             });
  EXPECT_EQ(device_cb_count, 1);
  // ConfigureSco() callback should be posted.
  EXPECT_EQ(controller_cb_count, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(controller_cb_count, 1);
  EXPECT_EQ(device_cb_count, 1);
}

TEST_F(BanjoControllerTest, ConfigureScoWithFormatCvsdEncoding16BitsSampleRate8Khz) {
  RETURN_IF_FATAL(InitializeController());
  fake_device()->set_configure_sco_callback(
      [](sco_coding_format_t format, sco_encoding_t encoding, sco_sample_rate_t rate,
         bt_hci_configure_sco_callback callback, void* cookie) {
        EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
        EXPECT_EQ(encoding, SCO_ENCODING_BITS_16);
        EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_8);
        callback(cookie, ZX_OK);
      });

  int config_cb_count = 0;
  controller()->ConfigureSco(ScoCodingFormat::kCvsd, ScoEncoding::k16Bits, ScoSampleRate::k8Khz,
                             [&](pw::Status status) {
                               config_cb_count++;
                               EXPECT_EQ(status, PW_STATUS_OK);
                             });
  RunLoopUntilIdle();
  EXPECT_EQ(config_cb_count, 1);
}

TEST_F(BanjoControllerTest, ConfigureScoWithFormatCvsdEncoding16BitsSampleRate16Khz) {
  RETURN_IF_FATAL(InitializeController());
  fake_device()->set_configure_sco_callback(
      [](sco_coding_format_t format, sco_encoding_t encoding, sco_sample_rate_t rate,
         bt_hci_configure_sco_callback callback, void* cookie) {
        EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
        EXPECT_EQ(encoding, SCO_ENCODING_BITS_16);
        EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_16);
        callback(cookie, ZX_OK);
      });

  int config_cb_count = 0;
  controller()->ConfigureSco(ScoCodingFormat::kCvsd, ScoEncoding::k16Bits, ScoSampleRate::k16Khz,
                             [&](pw::Status status) {
                               config_cb_count++;
                               EXPECT_EQ(status, PW_STATUS_OK);
                             });
  RunLoopUntilIdle();
  EXPECT_EQ(config_cb_count, 1);
}

TEST_F(BanjoControllerTest, ConfigureScoWithFormatMsbcEncoding16BitsSampleRate16Khz) {
  RETURN_IF_FATAL(InitializeController());
  fake_device()->set_configure_sco_callback(
      [](sco_coding_format_t format, sco_encoding_t encoding, sco_sample_rate_t rate,
         bt_hci_configure_sco_callback callback, void* cookie) {
        EXPECT_EQ(format, SCO_CODING_FORMAT_MSBC);
        EXPECT_EQ(encoding, SCO_ENCODING_BITS_16);
        EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_16);
        callback(cookie, ZX_OK);
      });

  int config_cb_count = 0;
  controller()->ConfigureSco(ScoCodingFormat::kMsbc, ScoEncoding::k16Bits, ScoSampleRate::k16Khz,
                             [&](pw::Status status) {
                               config_cb_count++;
                               EXPECT_EQ(status, PW_STATUS_OK);
                             });
  RunLoopUntilIdle();
  EXPECT_EQ(config_cb_count, 1);
}

TEST_F(BanjoControllerTest, ResetSco) {
  RETURN_IF_FATAL(InitializeController());
  int device_cb_count = 0;
  fake_device()->set_reset_sco_callback([&](bt_hci_reset_sco_callback callback, void* ctx) {
    device_cb_count++;
    callback(ctx, ZX_OK);
  });
  int controller_cb_count = 0;
  controller()->ResetSco([&](pw::Status status) {
    controller_cb_count++;
    EXPECT_EQ(status, PW_STATUS_OK);
  });
  EXPECT_EQ(device_cb_count, 1);
  // The ResetSco() callback should be posted.
  EXPECT_EQ(controller_cb_count, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(device_cb_count, 1);
  EXPECT_EQ(controller_cb_count, 1);
}

TEST_F(BanjoControllerTest, ConfigureScoCallbackCalledAfterHciWrapperDestroyed) {
  RETURN_IF_FATAL(InitializeController());
  int device_cb_count = 0;
  bt_hci_configure_sco_callback config_callback = nullptr;
  void* config_callback_ctx = nullptr;
  fake_device()->set_configure_sco_callback(
      [&](sco_coding_format_t format, sco_encoding_t encoding, sco_sample_rate_t rate,
          bt_hci_configure_sco_callback callback, void* cookie) {
        device_cb_count++;
        config_callback = callback;
        config_callback_ctx = cookie;
      });

  int controller_cb_count = 0;
  controller()->ConfigureSco(ScoCodingFormat::kCvsd, ScoEncoding::k8Bits, ScoSampleRate::k8Khz,
                             [&](pw::Status /*status*/) { controller_cb_count++; });
  ASSERT_EQ(device_cb_count, 1);
  EXPECT_EQ(controller_cb_count, 0);

  DestroyController();
  config_callback(config_callback_ctx, ZX_OK);

  // The ConfigureSco() callback should never be called.
  EXPECT_EQ(controller_cb_count, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(controller_cb_count, 0);
}

TEST_F(BanjoControllerTest, ResetScoCallbackCalledAfterHciWrapperDestroyed) {
  RETURN_IF_FATAL(InitializeController());
  int device_cb_count = 0;
  bt_hci_reset_sco_callback reset_callback = nullptr;
  void* reset_callback_ctx = nullptr;
  fake_device()->set_reset_sco_callback([&](bt_hci_reset_sco_callback callback, void* ctx) {
    device_cb_count++;
    reset_callback = callback;
    reset_callback_ctx = ctx;
  });

  int controller_cb_count = 0;
  controller()->ResetSco([&](pw::Status /*status*/) { controller_cb_count++; });
  ASSERT_EQ(device_cb_count, 1);
  EXPECT_EQ(controller_cb_count, 0);

  DestroyController();
  reset_callback(reset_callback_ctx, ZX_OK);

  // The ResetSco() callback should never be called.
  EXPECT_EQ(controller_cb_count, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(controller_cb_count, 0);
}

TEST_F(BanjoControllerTest, CloseClosesChannels) {
  RETURN_IF_FATAL(InitializeController());
  EXPECT_TRUE(fake_device()->acl_channel_is_valid());
  EXPECT_TRUE(fake_device()->sco_channel_is_valid());
  EXPECT_TRUE(fake_device()->command_channel_is_valid());

  std::optional<pw::Status> close_status;
  controller()->Close([&](pw::Status status) { close_status = status; });
  ASSERT_TRUE(close_status.has_value());
  EXPECT_EQ(close_status.value(), PW_STATUS_OK);
  RunLoopUntilIdle();
  EXPECT_FALSE(fake_device()->acl_channel_is_valid());
  EXPECT_FALSE(fake_device()->sco_channel_is_valid());
  EXPECT_FALSE(fake_device()->command_channel_is_valid());
}

TEST_F(BanjoControllerTest, DeviceClosesCommandChannel) {
  RETURN_IF_FATAL(InitializeController());
  fake_device()->ResetCommandChannel();
  RunLoopUntilIdle();
  EXPECT_THAT(controller_error(), ::testing::Optional(pw::Status::Unavailable()));
}

TEST_F(BanjoControllerTest, GetFeaturesWithoutVendorProto) {
  RETURN_IF_FATAL(InitializeController(/*vendor_supported=*/false));
  std::optional<FeaturesBits> features;
  controller()->GetFeatures([&](FeaturesBits cb_features) { features = cb_features; });
  EXPECT_THAT(features, ::testing::Optional(FeaturesBits{0}));
}

}  // namespace bt::controllers
