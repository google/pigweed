// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hci_wrapper.h"

#include <fuchsia/hardware/bt/hci/c/banjo.h>
#include <fuchsia/hardware/bt/vendor/c/banjo.h>

#include <gmock/gmock.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt::hci {

namespace {

constexpr bt_vendor_features_t kVendorFeatures = BT_VENDOR_FEATURES_SET_ACL_PRIORITY_COMMAND;

using TestingBase = ::gtest::TestLoopFixture;
class HciWrapperTest : public ::gtest::TestLoopFixture {
 public:
  void SetUp() override {}

  void InitializeHci(bool sco_supported = true, bt_vendor_features_t features = kVendorFeatures) {
    zx::channel cmd;
    zx::channel acl;
    ZX_ASSERT(zx::channel::create(/*flags=*/0, &cmd_, &cmd) == ZX_OK);
    ZX_ASSERT(zx::channel::create(/*flags=*/0, &acl_, &acl) == ZX_OK);
    auto device = std::make_unique<DummyDeviceWrapper>(std::move(cmd), std::move(acl), features);
    device_ = device.get();

    if (sco_supported) {
      zx::channel sco;
      ZX_ASSERT(zx::channel::create(/*flags=*/0, &sco_, &sco) == ZX_OK);
      device->set_sco_channel(std::move(sco));
    }

    hci_ = HciWrapper::Create(std::move(device), dispatcher());
    ASSERT_TRUE(hci_->Initialize(fit::bind_member<&HciWrapperTest::OnError>(this)));
  }

  fitx::result<zx_status_t, DynamicByteBuffer> ReadNextPacket(zx::channel* channel) {
    StaticByteBuffer<20> buffer;
    uint32_t read_size;
    zx_status_t status = channel->read(0u, buffer.mutable_data(), /*handles=*/nullptr,
                                       buffer.size(), 0, &read_size, /*actual_handles=*/nullptr);
    if (status != ZX_OK) {
      return fitx::error(status);
    }
    return fitx::ok(DynamicByteBuffer(buffer.view(/*pos=*/0, read_size)));
  }

  HciWrapper* hci() { return hci_.get(); }

  DummyDeviceWrapper* device() { return device_; }

  zx::channel* cmd_channel() { return &cmd_; }
  zx::channel* acl_channel() { return &acl_; }
  zx::channel* sco_channel() { return &sco_; }

  void DestroyHci() { hci_.reset(); }

 private:
  void OnError(zx_status_t status) { error_ = status; }

  std::unique_ptr<HciWrapper> hci_;
  DummyDeviceWrapper* device_;
  zx::channel cmd_;
  zx::channel acl_;
  zx::channel sco_;
  std::optional<zx_status_t> error_;
};

TEST_F(HciWrapperTest, InitializeFailureCommandChannelInvalid) {
  zx::channel cmd;
  zx::channel acl0;
  zx::channel acl1;
  ZX_ASSERT(zx::channel::create(/*flags=*/0, &acl0, &acl1) == ZX_OK);
  auto device =
      std::make_unique<DummyDeviceWrapper>(zx::channel(), std::move(acl1), kVendorFeatures,
                                           [](auto, auto) { return fpromise::error(); });
  auto hci = HciWrapper::Create(std::move(device), dispatcher());
  EXPECT_FALSE(hci->Initialize([](auto) {}));
}

TEST_F(HciWrapperTest, InitializeFailureAclChannelInvalid) {
  zx::channel cmd0;
  zx::channel cmd1;
  zx::channel acl;
  ZX_ASSERT(zx::channel::create(/*flags=*/0, &cmd0, &cmd1) == ZX_OK);
  auto device =
      std::make_unique<DummyDeviceWrapper>(std::move(cmd1), std::move(acl), kVendorFeatures,
                                           [](auto, auto) { return fpromise::error(); });
  auto hci = HciWrapper::Create(std::move(device), dispatcher());
  EXPECT_FALSE(hci->Initialize([](auto) {}));
}

TEST_F(HciWrapperTest, InitializeWithSco) {
  InitializeHci(/*sco_supported=*/true);
  EXPECT_TRUE(hci()->IsScoSupported());
}

TEST_F(HciWrapperTest, InitializeWithoutSco) {
  InitializeHci(/*sco_supported=*/false);
  EXPECT_FALSE(hci()->IsScoSupported());
}

TEST_F(HciWrapperTest, SendCommand) {
  constexpr hci_spec::OpCode opcode = 1;
  constexpr uint8_t payload = 8;
  InitializeHci();
  auto packet = CommandPacket::New(opcode, sizeof(payload));
  packet->mutable_view()->mutable_data()[0] = payload;
  DynamicByteBuffer expected_packet(packet->view().data());
  EXPECT_EQ(hci()->SendCommand(std::move(packet)), ZX_OK);

  fitx::result<zx_status_t, DynamicByteBuffer> read_result = ReadNextPacket(cmd_channel());
  ASSERT_TRUE(read_result.is_ok());
  EXPECT_TRUE(ContainersEqual(read_result.value(), expected_packet));
}

TEST_F(HciWrapperTest, SendAcl) {
  constexpr uint8_t payload = 8;
  InitializeHci();
  auto packet = ACLDataPacket::New(sizeof(payload));
  packet->mutable_view()->mutable_data()[0] = payload;
  DynamicByteBuffer expected_packet(packet->view().data());
  EXPECT_EQ(hci()->SendAclPacket(std::move(packet)), ZX_OK);

  fitx::result<zx_status_t, DynamicByteBuffer> read_result = ReadNextPacket(acl_channel());
  ASSERT_TRUE(read_result.is_ok());
  EXPECT_TRUE(ContainersEqual(read_result.value(), expected_packet));
}

TEST_F(HciWrapperTest, SendSco) {
  constexpr uint8_t payload = 8;
  InitializeHci(/*sco_supported=*/true);
  auto packet = ScoDataPacket::New(sizeof(payload));
  packet->mutable_view()->mutable_data()[0] = payload;
  DynamicByteBuffer expected_packet(packet->view().data());
  EXPECT_EQ(hci()->SendScoPacket(std::move(packet)), ZX_OK);

  fitx::result<zx_status_t, DynamicByteBuffer> read_result = ReadNextPacket(sco_channel());
  ASSERT_TRUE(read_result.is_ok());
  EXPECT_TRUE(ContainersEqual(read_result.value(), expected_packet));
}

TEST_F(HciWrapperTest, ReceiveEvents) {
  InitializeHci();
  std::unique_ptr<EventPacket> event;
  hci()->SetEventCallback(
      [&](std::unique_ptr<EventPacket> cb_event) { event = std::move(cb_event); });

  StaticByteBuffer event_buffer_0(0x01,   // event_code
                                  0x01,   // parameter_total_size
                                  0x00);  // payload
  cmd_channel()->write(/*flags=*/0, event_buffer_0.data(), event_buffer_0.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(event);
  EXPECT_TRUE(ContainersEqual(event->view().data(), event_buffer_0));

  event.reset();
  StaticByteBuffer event_buffer_1(0x01,   // event_code
                                  0x01,   // parameter_total_size
                                  0x01);  // payload
  cmd_channel()->write(/*flags=*/0, event_buffer_1.data(), event_buffer_1.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(event);
  EXPECT_TRUE(ContainersEqual(event->view().data(), event_buffer_1));
}

TEST_F(HciWrapperTest, ReceiveEventSmallerThanHeaderFollowedByValidEvent) {
  InitializeHci();
  std::unique_ptr<EventPacket> event;
  hci()->SetEventCallback(
      [&](std::unique_ptr<EventPacket> cb_event) { event = std::move(cb_event); });

  StaticByteBuffer event_buffer_0(0x01  // event_code
  );
  cmd_channel()->write(/*flags=*/0, event_buffer_0.data(), event_buffer_0.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_FALSE(event);

  StaticByteBuffer event_buffer_1(0x01,   // event_code
                                  0x01,   // parameter_total_size
                                  0x01);  // payload
  cmd_channel()->write(/*flags=*/0, event_buffer_1.data(), event_buffer_1.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(event);
  EXPECT_TRUE(ContainersEqual(event->view().data(), event_buffer_1));
}

TEST_F(HciWrapperTest, ReceiveEventWithInvalidSizeFieldInHeaderFollowedByValidEvent) {
  InitializeHci();
  std::unique_ptr<EventPacket> event;
  hci()->SetEventCallback(
      [&](std::unique_ptr<EventPacket> cb_event) { event = std::move(cb_event); });

  StaticByteBuffer event_buffer_0(0x01,   // event_code
                                  0x09,   // parameter_total_size (INVALID!)
                                  0x00);  // payload
  cmd_channel()->write(/*flags=*/0, event_buffer_0.data(), event_buffer_0.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_FALSE(event);

  StaticByteBuffer event_buffer_1(0x01,   // event_code
                                  0x01,   // parameter_total_size
                                  0x01);  // payload
  cmd_channel()->write(/*flags=*/0, event_buffer_1.data(), event_buffer_1.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(event);
  EXPECT_TRUE(ContainersEqual(event->view().data(), event_buffer_1));
}

TEST_F(HciWrapperTest, ReceiveAcl) {
  InitializeHci();
  std::unique_ptr<ACLDataPacket> packet;
  hci()->SetAclCallback(
      [&](std::unique_ptr<ACLDataPacket> cb_packet) { packet = std::move(cb_packet); });

  StaticByteBuffer packet_buffer_0(0x00, 0x00,  // handle_and_flags
                                   0x01, 0x00,  // data_total_length
                                   0x00         // payload
  );
  acl_channel()->write(/*flags=*/0, packet_buffer_0.data(), packet_buffer_0.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet);
  EXPECT_TRUE(ContainersEqual(packet->view().data(), packet_buffer_0));

  packet.reset();
  StaticByteBuffer packet_buffer_1(0x00, 0x00,  // handle_and_flags
                                   0x01, 0x00,  // data_total_length
                                   0x01         // payload
  );
  acl_channel()->write(/*flags=*/0, packet_buffer_1.data(), packet_buffer_1.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet);
  EXPECT_TRUE(ContainersEqual(packet->view().data(), packet_buffer_1));
}

TEST_F(HciWrapperTest, ReceiveAclWithLargePayload) {
  InitializeHci();
  std::unique_ptr<ACLDataPacket> packet;
  hci()->SetAclCallback(
      [&](std::unique_ptr<ACLDataPacket> cb_packet) { packet = std::move(cb_packet); });

  // Ensure the size of the packet is larger than the max value of 1 byte to detect narrowing
  // conversion bugs.
  constexpr uint16_t payload_size = static_cast<uint16_t>(std::numeric_limits<uint8_t>::max()) + 1;
  constexpr size_t packet_size = payload_size + sizeof(hci_spec::ACLDataHeader);
  const StaticByteBuffer packet_buffer_header(0x00, 0x00,  // handle_and_flags
                                              LowerBits(payload_size),
                                              UpperBits(payload_size)  // data_total_length
  );
  StaticByteBuffer<packet_size> packet_buffer;
  packet_buffer.Fill(0x03);
  packet_buffer_header.Copy(&packet_buffer);
  acl_channel()->write(/*flags=*/0, packet_buffer.data(), packet_buffer.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet);
  EXPECT_TRUE(ContainersEqual(packet->view().data(), packet_buffer));
}

TEST_F(HciWrapperTest, ReceiveAclSmallerThanHeaderFollowedByValidPacket) {
  InitializeHci();
  std::unique_ptr<ACLDataPacket> packet;
  hci()->SetAclCallback(
      [&](std::unique_ptr<ACLDataPacket> cb_packet) { packet = std::move(cb_packet); });

  StaticByteBuffer packet_buffer_0(0x00, 0x00);  // handle_and_flags
  acl_channel()->write(/*flags=*/0, packet_buffer_0.data(), packet_buffer_0.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_FALSE(packet);

  StaticByteBuffer packet_buffer_1(0x00, 0x00,  // handle_and_flags
                                   0x01, 0x00,  // data_total_length
                                   0x01         // payload
  );
  acl_channel()->write(/*flags=*/0, packet_buffer_1.data(), packet_buffer_1.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet);
  EXPECT_TRUE(ContainersEqual(packet->view().data(), packet_buffer_1));
}

TEST_F(HciWrapperTest, ReceiveAclWithInvalidSizeFieldInHeaderFollowedByValidPacket) {
  InitializeHci();
  std::unique_ptr<ACLDataPacket> packet;
  hci()->SetAclCallback(
      [&](std::unique_ptr<ACLDataPacket> cb_packet) { packet = std::move(cb_packet); });

  StaticByteBuffer packet_buffer_0(0x00, 0x00,  // handle_and_flags
                                   0x09, 0x00,  // data_total_length
                                   0x00);       // payload
  acl_channel()->write(/*flags=*/0, packet_buffer_0.data(), packet_buffer_0.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_FALSE(packet);

  StaticByteBuffer packet_buffer_1(0x00, 0x00,  // handle_and_flags
                                   0x01, 0x00,  // data_total_length
                                   0x01         // payload
  );
  acl_channel()->write(/*flags=*/0, packet_buffer_1.data(), packet_buffer_1.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet);
  EXPECT_TRUE(ContainersEqual(packet->view().data(), packet_buffer_1));
}

TEST_F(HciWrapperTest, ReceiveAclPacketAndClearAclCallbackCancelsSignalWait) {
  InitializeHci();
  std::unique_ptr<ACLDataPacket> packet;
  hci()->SetAclCallback(
      [&](std::unique_ptr<ACLDataPacket> cb_packet) { packet = std::move(cb_packet); });

  StaticByteBuffer packet_buffer_0(0x00, 0x00,  // handle_and_flags
                                   0x01, 0x00,  // data_total_length
                                   0x00         // payload
  );
  acl_channel()->write(/*flags=*/0, packet_buffer_0.data(), packet_buffer_0.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet);
  EXPECT_TRUE(ContainersEqual(packet->view().data(), packet_buffer_0));

  packet.reset();
  hci()->SetAclCallback(nullptr);
  StaticByteBuffer packet_buffer_1(0x00, 0x00,  // handle_and_flags
                                   0x01, 0x00,  // data_total_length
                                   0x01         // payload
  );
  acl_channel()->write(/*flags=*/0, packet_buffer_1.data(), packet_buffer_1.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_FALSE(packet);
}

TEST_F(HciWrapperTest, ReceiveSco) {
  InitializeHci(/*sco_supported=*/true);
  std::unique_ptr<ScoDataPacket> packet;
  hci()->SetScoCallback(
      [&](std::unique_ptr<ScoDataPacket> cb_packet) { packet = std::move(cb_packet); });

  StaticByteBuffer packet_buffer_0(0x00, 0x00,  // handle_and_flags
                                   0x01,        // data_total_length
                                   0x00);       // payload
  sco_channel()->write(/*flags=*/0, packet_buffer_0.data(), packet_buffer_0.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet);
  EXPECT_TRUE(ContainersEqual(packet->view().data(), packet_buffer_0));

  packet.reset();
  StaticByteBuffer packet_buffer_1(0x00, 0x00,  // handle_and_flags
                                   0x01,        // data_total_length
                                   0x00         // payload
  );
  sco_channel()->write(/*flags=*/0, packet_buffer_1.data(), packet_buffer_1.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet);
  EXPECT_TRUE(ContainersEqual(packet->view().data(), packet_buffer_1));
}

TEST_F(HciWrapperTest, ReceiveScoPacketSmallerThanHeaderFollowedByValidPacket) {
  InitializeHci(/*sco_supported=*/true);
  std::unique_ptr<ScoDataPacket> packet;
  hci()->SetScoCallback(
      [&](std::unique_ptr<ScoDataPacket> cb_packet) { packet = std::move(cb_packet); });

  StaticByteBuffer packet_buffer_0(0x00, 0x00);  // handle_and_flags
  sco_channel()->write(/*flags=*/0, packet_buffer_0.data(), packet_buffer_0.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_FALSE(packet);

  StaticByteBuffer packet_buffer_1(0x00, 0x00,  // handle_and_flags
                                   0x01,        // data_total_length
                                   0x00         // payload
  );
  sco_channel()->write(/*flags=*/0, packet_buffer_1.data(), packet_buffer_1.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet);
  EXPECT_TRUE(ContainersEqual(packet->view().data(), packet_buffer_1));
}

TEST_F(HciWrapperTest, ReceiveScoPacketWithInvalidSizeFieldInHeaderFollowedByValidPacket) {
  InitializeHci(/*sco_supported=*/true);
  std::unique_ptr<ScoDataPacket> packet;
  hci()->SetScoCallback(
      [&](std::unique_ptr<ScoDataPacket> cb_packet) { packet = std::move(cb_packet); });

  StaticByteBuffer packet_buffer_0(0x00, 0x00,  // handle_and_flags
                                   0x09,        // data_total_length (INVALID!)
                                   0x00);       // payload
  sco_channel()->write(/*flags=*/0, packet_buffer_0.data(), packet_buffer_0.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_FALSE(packet);

  StaticByteBuffer packet_buffer_1(0x00, 0x00,  // handle_and_flags
                                   0x01,        // data_total_length
                                   0x00         // payload
  );
  sco_channel()->write(/*flags=*/0, packet_buffer_1.data(), packet_buffer_1.size(),
                       /*handles=*/nullptr,
                       /*num_handles=*/0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet);
  EXPECT_TRUE(ContainersEqual(packet->view().data(), packet_buffer_1));
}

TEST_F(HciWrapperTest, EncodeSetAclPriorityCommandFailure) {
  InitializeHci(/*sco_supported=*/true);
  device()->set_vendor_encode_callback([](auto, auto) { return fpromise::error(); });
  fitx::result<zx_status_t, DynamicByteBuffer> result =
      hci()->EncodeSetAclPriorityCommand(/*connection=*/1, AclPriority::kSink);
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error_value(), ZX_ERR_INTERNAL);
}

TEST_F(HciWrapperTest, EncodeSetAclPriorityCommandSuccessNormal) {
  InitializeHci(/*sco_supported=*/true);
  constexpr hci_spec::ConnectionHandle handle = 9;
  StaticByteBuffer packet(0x09);
  std::optional<uint32_t> command;
  std::optional<bt_vendor_params_t> params;
  device()->set_vendor_encode_callback([&](uint32_t cb_command, bt_vendor_params_t cb_params) {
    command = cb_command;
    params = cb_params;
    return fpromise::ok(DynamicByteBuffer(packet));
  });
  fitx::result<zx_status_t, DynamicByteBuffer> result =
      hci()->EncodeSetAclPriorityCommand(handle, AclPriority::kNormal);
  ASSERT_TRUE(result.is_ok());
  EXPECT_TRUE(ContainersEqual(packet, result.value()));
  EXPECT_THAT(command, ::testing::Optional(BT_VENDOR_COMMAND_SET_ACL_PRIORITY));
  ASSERT_TRUE(params);
  EXPECT_EQ(params->set_acl_priority.connection_handle, handle);
  EXPECT_EQ(params->set_acl_priority.priority, BT_VENDOR_ACL_PRIORITY_NORMAL);
}

TEST_F(HciWrapperTest, EncodeSetAclPriorityCommandSuccessSource) {
  InitializeHci(/*sco_supported=*/true);
  constexpr hci_spec::ConnectionHandle handle = 9;
  StaticByteBuffer packet(0x09);
  std::optional<uint32_t> command;
  std::optional<bt_vendor_params_t> params;
  device()->set_vendor_encode_callback([&](uint32_t cb_command, bt_vendor_params_t cb_params) {
    command = cb_command;
    params = cb_params;
    return fpromise::ok(DynamicByteBuffer(packet));
  });
  fitx::result<zx_status_t, DynamicByteBuffer> result =
      hci()->EncodeSetAclPriorityCommand(handle, AclPriority::kSource);
  ASSERT_TRUE(result.is_ok());
  EXPECT_TRUE(ContainersEqual(packet, result.value()));
  EXPECT_THAT(command, ::testing::Optional(BT_VENDOR_COMMAND_SET_ACL_PRIORITY));
  ASSERT_TRUE(params);
  EXPECT_EQ(params->set_acl_priority.connection_handle, handle);
  EXPECT_EQ(params->set_acl_priority.priority, BT_VENDOR_ACL_PRIORITY_HIGH);
  EXPECT_EQ(params->set_acl_priority.direction, BT_VENDOR_ACL_DIRECTION_SOURCE);
}

TEST_F(HciWrapperTest, EncodeSetAclPriorityCommandSuccessSink) {
  InitializeHci(/*sco_supported=*/true);
  constexpr hci_spec::ConnectionHandle handle = 9;
  StaticByteBuffer packet(0x09);
  std::optional<uint32_t> command;
  std::optional<bt_vendor_params_t> params;
  device()->set_vendor_encode_callback([&](uint32_t cb_command, bt_vendor_params_t cb_params) {
    command = cb_command;
    params = cb_params;
    return fpromise::ok(DynamicByteBuffer(packet));
  });
  fitx::result<zx_status_t, DynamicByteBuffer> result =
      hci()->EncodeSetAclPriorityCommand(handle, AclPriority::kSink);
  ASSERT_TRUE(result.is_ok());
  EXPECT_TRUE(ContainersEqual(packet, result.value()));
  EXPECT_THAT(command, ::testing::Optional(BT_VENDOR_COMMAND_SET_ACL_PRIORITY));
  ASSERT_TRUE(params);
  EXPECT_EQ(params->set_acl_priority.connection_handle, handle);
  EXPECT_EQ(params->set_acl_priority.priority, BT_VENDOR_ACL_PRIORITY_HIGH);
  EXPECT_EQ(params->set_acl_priority.direction, BT_VENDOR_ACL_DIRECTION_SINK);
}

TEST_F(HciWrapperTest, GetVendorFeaturesSetAclPriority) {
  InitializeHci(/*sco_supported=*/false, /*features=*/BT_VENDOR_FEATURES_SET_ACL_PRIORITY_COMMAND);
  EXPECT_EQ(hci()->GetVendorFeatures(), VendorFeaturesBits::kSetAclPriorityCommand);
}

TEST_F(HciWrapperTest, GetVendorFeaturesAndroidVendorExtensions) {
  InitializeHci(/*sco_supported=*/false, /*features=*/BT_VENDOR_FEATURES_ANDROID_VENDOR_EXTENSIONS);
  EXPECT_EQ(hci()->GetVendorFeatures(), VendorFeaturesBits::kAndroidVendorExtensions);
}

TEST_F(HciWrapperTest, GetVendorFeaturesAll) {
  // Send a full feature mask to simulate all known features supported plus some unknown features
  // supported.
  InitializeHci(/*sco_supported=*/false,
                /*features=*/std::numeric_limits<bt_vendor_features_t>::max());
  EXPECT_EQ(hci()->GetVendorFeatures(), VendorFeaturesBits::kAndroidVendorExtensions |
                                            VendorFeaturesBits::kSetAclPriorityCommand);
}

TEST_F(HciWrapperTest, ConfigureScoWithFormatCvsdEncoding8BitsSampleRate8Khz) {
  InitializeHci(/*sco_supported=*/true);
  int device_cb_count = 0;
  device()->set_configure_sco_callback([&](sco_coding_format_t format, sco_encoding_t encoding,
                                           sco_sample_rate_t rate,
                                           bt_hci_configure_sco_callback callback, void* cookie) {
    device_cb_count++;
    EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
    EXPECT_EQ(encoding, SCO_ENCODING_BITS_8);
    EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_8);
    callback(cookie, ZX_OK);
  });

  int hci_cb_count = 0;
  hci()->ConfigureSco(ScoCodingFormat::kCvsd, ScoEncoding::k8Bits, ScoSampleRate::k8Khz,
                      [&](zx_status_t status) {
                        hci_cb_count++;
                        EXPECT_EQ(status, ZX_OK);
                      });
  EXPECT_EQ(device_cb_count, 1);
  // ConfigureSco() callback should be posted.
  EXPECT_EQ(hci_cb_count, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(hci_cb_count, 1);
  EXPECT_EQ(device_cb_count, 1);
}

TEST_F(HciWrapperTest, ConfigureScoWithFormatCvsdEncoding16BitsSampleRate8Khz) {
  InitializeHci(/*sco_supported=*/true);
  device()->set_configure_sco_callback([](sco_coding_format_t format, sco_encoding_t encoding,
                                          sco_sample_rate_t rate,
                                          bt_hci_configure_sco_callback callback, void* cookie) {
    EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
    EXPECT_EQ(encoding, SCO_ENCODING_BITS_16);
    EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_8);
    callback(cookie, ZX_OK);
  });

  int hci_cb_count = 0;
  hci()->ConfigureSco(ScoCodingFormat::kCvsd, ScoEncoding::k16Bits, ScoSampleRate::k8Khz,
                      [&](zx_status_t status) {
                        hci_cb_count++;
                        EXPECT_EQ(status, ZX_OK);
                      });
  RunLoopUntilIdle();
  EXPECT_EQ(hci_cb_count, 1);
}

TEST_F(HciWrapperTest, ConfigureScoWithFormatCvsdEncoding16BitsSampleRate16Khz) {
  InitializeHci(/*sco_supported=*/true);
  device()->set_configure_sco_callback([](sco_coding_format_t format, sco_encoding_t encoding,
                                          sco_sample_rate_t rate,
                                          bt_hci_configure_sco_callback callback, void* cookie) {
    EXPECT_EQ(format, SCO_CODING_FORMAT_CVSD);
    EXPECT_EQ(encoding, SCO_ENCODING_BITS_16);
    EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_16);
    callback(cookie, ZX_OK);
  });

  int hci_cb_count = 0;
  hci()->ConfigureSco(ScoCodingFormat::kCvsd, ScoEncoding::k16Bits, ScoSampleRate::k16Khz,
                      [&](zx_status_t status) {
                        hci_cb_count++;
                        EXPECT_EQ(status, ZX_OK);
                      });
  RunLoopUntilIdle();
  EXPECT_EQ(hci_cb_count, 1);
}

TEST_F(HciWrapperTest, ConfigureScoWithFormatMsbcEncoding16BitsSampleRate16Khz) {
  InitializeHci(/*sco_supported=*/true);
  device()->set_configure_sco_callback([](sco_coding_format_t format, sco_encoding_t encoding,
                                          sco_sample_rate_t rate,
                                          bt_hci_configure_sco_callback callback, void* cookie) {
    EXPECT_EQ(format, SCO_CODING_FORMAT_MSBC);
    EXPECT_EQ(encoding, SCO_ENCODING_BITS_16);
    EXPECT_EQ(rate, SCO_SAMPLE_RATE_KHZ_16);
    callback(cookie, ZX_OK);
  });

  int hci_cb_count = 0;
  hci()->ConfigureSco(ScoCodingFormat::kMsbc, ScoEncoding::k16Bits, ScoSampleRate::k16Khz,
                      [&](zx_status_t status) {
                        hci_cb_count++;
                        EXPECT_EQ(status, ZX_OK);
                      });
  RunLoopUntilIdle();
  EXPECT_EQ(hci_cb_count, 1);
}

TEST_F(HciWrapperTest, ResetSco) {
  InitializeHci(/*sco_supported=*/true);
  int device_cb_count = 0;
  device()->set_reset_sco_callback([&](bt_hci_reset_sco_callback callback, void* ctx) {
    device_cb_count++;
    callback(ctx, ZX_OK);
  });
  int hci_cb_count = 0;
  hci()->ResetSco([&](zx_status_t status) {
    hci_cb_count++;
    EXPECT_EQ(status, ZX_OK);
  });
  EXPECT_EQ(device_cb_count, 1);
  // The ResetSco() callback should be posted.
  EXPECT_EQ(hci_cb_count, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(device_cb_count, 1);
  EXPECT_EQ(hci_cb_count, 1);
}

TEST_F(HciWrapperTest, ConfigureScoCallbackCalledAfterHciWrapperDestroyed) {
  InitializeHci(/*sco_supported=*/true);
  int device_cb_count = 0;
  bt_hci_configure_sco_callback config_callback = nullptr;
  void* config_callback_ctx = nullptr;
  device()->set_configure_sco_callback([&](sco_coding_format_t format, sco_encoding_t encoding,
                                           sco_sample_rate_t rate,
                                           bt_hci_configure_sco_callback callback, void* cookie) {
    device_cb_count++;
    config_callback = callback;
    config_callback_ctx = cookie;
  });

  int hci_cb_count = 0;
  hci()->ConfigureSco(ScoCodingFormat::kCvsd, ScoEncoding::k8Bits, ScoSampleRate::k8Khz,
                      [&](zx_status_t status) { hci_cb_count++; });
  ASSERT_EQ(device_cb_count, 1);
  EXPECT_EQ(hci_cb_count, 0);

  DestroyHci();
  config_callback(config_callback_ctx, ZX_OK);

  // The ConfigureSco() callback should never be called.
  EXPECT_EQ(hci_cb_count, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(hci_cb_count, 0);
}

TEST_F(HciWrapperTest, ResetScoCallbackCalledAfterHciWrapperDestroyed) {
  InitializeHci(/*sco_supported=*/true);
  int device_cb_count = 0;
  bt_hci_reset_sco_callback reset_callback = nullptr;
  void* reset_callback_ctx = nullptr;
  device()->set_reset_sco_callback([&](bt_hci_reset_sco_callback callback, void* ctx) {
    device_cb_count++;
    reset_callback = callback;
    reset_callback_ctx = ctx;
  });

  int hci_cb_count = 0;
  hci()->ResetSco([&](zx_status_t status) { hci_cb_count++; });
  ASSERT_EQ(device_cb_count, 1);
  EXPECT_EQ(hci_cb_count, 0);

  DestroyHci();
  reset_callback(reset_callback_ctx, ZX_OK);

  // The ResetSco() callback should never be called.
  EXPECT_EQ(hci_cb_count, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(hci_cb_count, 0);
}

}  // namespace

}  // namespace bt::hci
