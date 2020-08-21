// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test_double_base.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

// Prevent "undefined symbol: __zircon_driver_rec__" error.
BT_DECLARE_FAKE_DRIVER();

namespace bt::testing {

// ACL Buffer Info
constexpr size_t kMaxDataPacketLength = 64;
// Ensure outbound ACL packets aren't queued.
constexpr size_t kMaxPacketCount = 1000;

// If the packet size is too large, we consume too much of the fuzzer data per packet without much
// benefit.
constexpr uint16_t kMaxAclPacketSize = 100;

constexpr hci::ConnectionHandle kHandle = 0x0001;

// Don't toggle connection too often or else l2cap won't get very far.
constexpr float kToggleConnectionChance = 0.04;

class FuzzerController : public ControllerTestDoubleBase {
 public:
  FuzzerController() {}
  ~FuzzerController() override = default;

 private:
  void OnCommandPacketReceived(const PacketView<hci::CommandHeader>& command_packet) override {}
  void OnACLDataPacketReceived(const ByteBuffer& acl_data_packet) override {}
};

// Reuse ControllerTest test fixture code even though we're not using gtest.
using TestingBase = ControllerTest<FuzzerController>;
class DataFuzzTest : public TestingBase {
 public:
  DataFuzzTest(const uint8_t* data, size_t size) : data_(data, size), connection_(false) {
    TestingBase::SetUp();
    const auto bredr_buffer_info = hci::DataBufferInfo(kMaxDataPacketLength, kMaxPacketCount);
    InitializeACLDataChannel(bredr_buffer_info);

    domain_ = data::Domain::Create(transport()->WeakPtr());

    StartTestDevice();
  };

  ~DataFuzzTest() override {
    domain_ = nullptr;
    TestingBase::TearDown();
  }

  void TestBody() override {
    RegisterService();

    while (data_.remaining_bytes() > 0) {
      bool run_loop = data_.ConsumeBool();
      if (run_loop) {
        RunLoopUntilIdle();
      }

      if (!SendAclPacket()) {
        break;
      }

      if (data_.ConsumeProbability<float>() < kToggleConnectionChance) {
        ToggleConnection();
      }
    }

    RunLoopUntilIdle();
  }

  bool SendAclPacket() {
    if (data_.remaining_bytes() < sizeof(uint64_t)) {
      return false;
    }
    // Consumes 8 bytes.
    auto packet_size = data_.ConsumeIntegralInRange<uint16_t>(
        sizeof(hci::ACLDataHeader),
        std::min(static_cast<size_t>(kMaxAclPacketSize), data_.remaining_bytes()));

    auto packet_data = data_.ConsumeBytes<uint8_t>(packet_size);
    if (packet_data.size() < packet_size) {
      // Check if we ran out of fuzzer data.
      return false;
    }

    MutableBufferView packet_view(packet_data.data(), packet_data.size());

    // Use correct length so packets aren't rejected for invalid length.
    packet_view.AsMutable<hci::ACLDataHeader>().data_total_length =
        htole16(packet_view.size() - sizeof(hci::ACLDataHeader));

    // Use correct connection handle so packets aren't rejected/queued for invalid handle.
    uint16_t handle_and_flags = packet_view.As<hci::ACLDataHeader>().handle_and_flags;
    handle_and_flags &= 0xF000;  // Keep flags, clear handle.
    handle_and_flags |= kHandle;
    packet_view.AsMutable<hci::ACLDataHeader>().handle_and_flags = handle_and_flags;

    auto status = test_device()->SendACLDataChannelPacket(packet_view);
    ZX_ASSERT(status == ZX_OK);
    return true;
  }

  void RegisterService() {
    domain_->RegisterService(l2cap::kAVDTP, l2cap::ChannelParameters(),
                             [this](fbl::RefPtr<l2cap::Channel> chan) {
                               if (!chan) {
                                 return;
                               }
                               chan->Activate(/*rx_callback=*/[](auto) {}, /*closed_callback=*/
                                              [this, id = chan->id()] { channels_.erase(id); });
                               channels_.emplace(chan->id(), std::move(chan));
                             });
  }

  void ToggleConnection() {
    if (connection_) {
      acl_data_channel()->UnregisterLink(kHandle);
      domain_->RemoveConnection(kHandle);
      connection_ = false;
      return;
    }

    acl_data_channel()->RegisterLink(kHandle, hci::Connection::LinkType::kACL);
    domain_->AddACLConnection(
        kHandle, hci::Connection::Role::kMaster, /*link_error_callback=*/[] {},
        /*security_upgrade_callback=*/[](auto, auto, auto) {});
    connection_ = true;
  }

 private:
  FuzzedDataProvider data_;
  fbl::RefPtr<data::Domain> domain_;
  bool connection_;
  std::unordered_map<l2cap::ChannelId, fbl::RefPtr<l2cap::Channel>> channels_;
};

}  // namespace bt::testing

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  bt::UsePrintf(bt::LogSeverity::ERROR);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bt::testing::DataFuzzTest fuzz(data, size);
  fuzz.TestBody();
  return 0;
}
