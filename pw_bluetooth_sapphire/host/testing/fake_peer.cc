// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/testing/fake_peer.h"

#include <endian.h>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/common/packet_view.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/testing/fake_controller.h"

namespace bt::testing {
using pw::bluetooth::emboss::LEExtendedAdvertisingReportDataWriter;

FakePeer::FakePeer(const DeviceAddress& address,
                   pw::async::Dispatcher& pw_dispatcher,
                   bool connectable,
                   bool scannable)
    : ctrl_(nullptr),
      address_(address),
      name_("FakePeer"),
      connected_(false),
      connectable_(connectable),
      scannable_(scannable),
      advertising_enabled_(true),
      directed_(false),
      address_resolved_(false),
      connect_status_(pw::bluetooth::emboss::StatusCode::SUCCESS),
      connect_response_(pw::bluetooth::emboss::StatusCode::SUCCESS),
      force_pending_connect_(false),
      supports_ll_conn_update_procedure_(true),
      le_features_(hci_spec::LESupportedFeatures{0}),
      should_batch_reports_(false),
      l2cap_(fit::bind_member<&FakePeer::SendPacket>(this)),
      gatt_server_(this),
      sdp_server_(pw_dispatcher) {
  signaling_server_.RegisterWithL2cap(&l2cap_);
  gatt_server_.RegisterWithL2cap(&l2cap_);
  sdp_server_.RegisterWithL2cap(&l2cap_);
}

void FakePeer::SetAdvertisingData(const ByteBuffer& data) {
  BT_DEBUG_ASSERT(data.size() <= hci_spec::kMaxLEAdvertisingDataLength);
  adv_data_ = DynamicByteBuffer(data);
}

void FakePeer::SetScanResponse(bool should_batch_reports,
                               const ByteBuffer& data) {
  BT_DEBUG_ASSERT(scannable_);
  scan_rsp_ = DynamicByteBuffer(data);
  should_batch_reports_ = should_batch_reports;
}

DynamicByteBuffer FakePeer::CreateInquiryResponseEvent(
    pw::bluetooth::emboss::InquiryMode mode) const {
  BT_DEBUG_ASSERT(address_.type() == DeviceAddress::Type::kBREDR);

  if (mode == pw::bluetooth::emboss::InquiryMode::STANDARD) {
    size_t packet_size =
        pw::bluetooth::emboss::InquiryResultEvent::MinSizeInBytes() +
        pw::bluetooth::emboss::InquiryResult::IntrinsicSizeInBytes();
    auto packet = hci::EmbossEventPacket::New<
        pw::bluetooth::emboss::InquiryResultEventWriter>(
        hci_spec::kInquiryResultEventCode, packet_size);
    auto view = packet.view_t();
    view.num_responses().Write(1);
    view.responses()[0].bd_addr().CopyFrom(address_.value().view());
    view.responses()[0].page_scan_repetition_mode().Write(
        pw::bluetooth::emboss::PageScanRepetitionMode::R0_);
    view.responses()[0].class_of_device().BackingStorage().WriteUInt(
        class_of_device_.to_int());
    return DynamicByteBuffer{packet.data()};
  }

  constexpr size_t packet_size =
      pw::bluetooth::emboss::InquiryResultWithRssiEvent::MinSizeInBytes() +
      pw::bluetooth::emboss::InquiryResultWithRssi::IntrinsicSizeInBytes();
  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::InquiryResultWithRssiEventWriter>(
      hci_spec::kInquiryResultEventCode, packet_size);
  auto view = packet.view_t();

  // TODO(jamuraa): simulate clock offset and RSSI
  view.num_responses().Write(1);
  auto response = view.responses()[0];
  response.bd_addr().CopyFrom(address_.value().view());
  response.page_scan_repetition_mode().Write(
      pw::bluetooth::emboss::PageScanRepetitionMode::R0_);
  response.class_of_device().BackingStorage().WriteUInt(
      class_of_device_.to_int());
  response.clock_offset().BackingStorage().WriteUInt(0);
  response.rssi().Write(-30);
  return DynamicByteBuffer{packet.data()};
}

void FakePeer::AddLink(hci_spec::ConnectionHandle handle) {
  BT_DEBUG_ASSERT(!HasLink(handle));
  logical_links_.insert(handle);

  if (logical_links_.size() == 1u) {
    set_connected(true);
  }
}

void FakePeer::RemoveLink(hci_spec::ConnectionHandle handle) {
  BT_DEBUG_ASSERT(HasLink(handle));
  logical_links_.erase(handle);
  if (logical_links_.empty())
    set_connected(false);
}

bool FakePeer::HasLink(hci_spec::ConnectionHandle handle) const {
  return logical_links_.count(handle) != 0u;
}

FakePeer::HandleSet FakePeer::Disconnect() {
  set_connected(false);
  return std::move(logical_links_);
}

void FakePeer::OnRxL2CAP(hci_spec::ConnectionHandle conn,
                         const ByteBuffer& pdu) {
  if (pdu.size() < sizeof(l2cap::BasicHeader)) {
    bt_log(WARN, "fake-hci", "malformed L2CAP packet!");
    return;
  }
  l2cap_.HandlePdu(conn, pdu);
}

void FakePeer::SendPacket(hci_spec::ConnectionHandle conn,
                          l2cap::ChannelId cid,
                          const ByteBuffer& packet) {
  ctrl()->SendL2CAPBFrame(conn, cid, packet);
}

}  // namespace bt::testing
