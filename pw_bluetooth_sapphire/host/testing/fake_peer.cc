// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_peer.h"

#include <endian.h>

#include "fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"

namespace bt::testing {
namespace {

void WriteRandomRSSI(int8_t* out_mem) {
  constexpr int8_t kRSSIMin = -127;
  constexpr int8_t kRSSIMax = 20;

  int8_t rssi;
  random_generator()->GetInt(rssi);
  rssi = (rssi % (kRSSIMax - kRSSIMin)) + kRSSIMin;

  *out_mem = rssi;
}

}  // namespace

FakePeer::FakePeer(const DeviceAddress& address, bool connectable, bool scannable)
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
      gatt_server_(this) {
  signaling_server_.RegisterWithL2cap(&l2cap_);
  gatt_server_.RegisterWithL2cap(&l2cap_);
  sdp_server_.RegisterWithL2cap(&l2cap_);
}

void FakePeer::SetAdvertisingData(const ByteBuffer& data) {
  BT_DEBUG_ASSERT(data.size() <= hci_spec::kMaxLEAdvertisingDataLength);
  adv_data_ = DynamicByteBuffer(data);
}

void FakePeer::SetScanResponse(bool should_batch_reports, const ByteBuffer& data) {
  BT_DEBUG_ASSERT(scannable_);
  BT_DEBUG_ASSERT(data.size() <= hci_spec::kMaxLEAdvertisingDataLength);
  scan_rsp_ = DynamicByteBuffer(data);
  should_batch_reports_ = should_batch_reports;
}

DynamicByteBuffer FakePeer::CreateInquiryResponseEvent(
    pw::bluetooth::emboss::InquiryMode mode) const {
  BT_DEBUG_ASSERT(address_.type() == DeviceAddress::Type::kBREDR);

  if (mode == pw::bluetooth::emboss::InquiryMode::STANDARD) {
    size_t packet_size = pw::bluetooth::emboss::InquiryResultEvent::MinSizeInBytes() +
                         pw::bluetooth::emboss::InquiryResult::IntrinsicSizeInBytes();
    auto packet = hci::EmbossEventPacket::New<pw::bluetooth::emboss::InquiryResultEventWriter>(
        hci_spec::kInquiryResultEventCode, packet_size);
    auto view = packet.view_t();
    view.num_responses().Write(1);
    view.responses()[0].bd_addr().CopyFrom(address_.value().view());
    view.responses()[0].page_scan_repetition_mode().Write(
        pw::bluetooth::emboss::PageScanRepetitionMode::R0_);
    view.responses()[0].class_of_device().BackingStorage().WriteUInt(class_of_device_.to_int());
    return DynamicByteBuffer{packet.data()};
  }

  constexpr size_t packet_size =
      pw::bluetooth::emboss::InquiryResultWithRssiEvent::MinSizeInBytes() +
      pw::bluetooth::emboss::InquiryResultWithRssi::IntrinsicSizeInBytes();
  auto packet =
      hci::EmbossEventPacket::New<pw::bluetooth::emboss::InquiryResultWithRssiEventWriter>(
          hci_spec::kInquiryResultEventCode, packet_size);
  auto view = packet.view_t();

  // TODO(jamuraa): simulate clock offset and RSSI
  view.num_responses().Write(1);
  auto response = view.responses()[0];
  response.bd_addr().CopyFrom(address_.value().view());
  response.page_scan_repetition_mode().Write(pw::bluetooth::emboss::PageScanRepetitionMode::R0_);
  response.class_of_device().BackingStorage().WriteUInt(class_of_device_.to_int());
  response.clock_offset().BackingStorage().WriteUInt(0);
  response.rssi().Write(-30);
  return DynamicByteBuffer{packet.data()};
}

DynamicByteBuffer FakePeer::CreateAdvertisingReportEvent(bool include_scan_rsp) const {
  size_t param_size = sizeof(hci_spec::LEMetaEventParams) +
                      sizeof(hci_spec::LEAdvertisingReportSubeventParams) +
                      sizeof(hci_spec::LEAdvertisingReportData) + adv_data_.size() + sizeof(int8_t);
  if (include_scan_rsp) {
    BT_DEBUG_ASSERT(scannable_);
    param_size += sizeof(hci_spec::LEAdvertisingReportData) + scan_rsp_.size() + sizeof(int8_t);
  }

  DynamicByteBuffer buffer(sizeof(hci_spec::EventHeader) + param_size);
  MutablePacketView<hci_spec::EventHeader> event(&buffer, param_size);
  event.mutable_header()->event_code = hci_spec::kLEMetaEventCode;
  event.mutable_header()->parameter_total_size = param_size;

  auto payload = event.mutable_payload<hci_spec::LEMetaEventParams>();
  payload->subevent_code = hci_spec::kLEAdvertisingReportSubeventCode;

  auto subevent_payload =
      reinterpret_cast<hci_spec::LEAdvertisingReportSubeventParams*>(payload->subevent_parameters);
  subevent_payload->num_reports = include_scan_rsp ? 2 : 1;

  auto report = reinterpret_cast<hci_spec::LEAdvertisingReportData*>(subevent_payload->reports);
  if (directed_) {
    report->event_type = hci_spec::LEAdvertisingEventType::kAdvDirectInd;
  } else if (connectable_) {
    report->event_type = hci_spec::LEAdvertisingEventType::kAdvInd;
  } else if (scannable_) {
    report->event_type = hci_spec::LEAdvertisingEventType::kAdvScanInd;
  } else {
    report->event_type = hci_spec::LEAdvertisingEventType::kAdvNonConnInd;
  }
  if (address_.type() == DeviceAddress::Type::kLERandom) {
    report->address_type = address_resolved_ ? hci_spec::LEAddressType::kRandomIdentity
                                             : hci_spec::LEAddressType::kRandom;
  } else {
    report->address_type = address_resolved_ ? hci_spec::LEAddressType::kPublicIdentity
                                             : hci_spec::LEAddressType::kPublic;
  }
  report->address = address_.value();
  report->length_data = adv_data_.size();
  std::memcpy(report->data, adv_data_.data(), adv_data_.size());

  WriteRandomRSSI(reinterpret_cast<int8_t*>(report->data + report->length_data));

  if (include_scan_rsp) {
    WriteScanResponseReport(reinterpret_cast<hci_spec::LEAdvertisingReportData*>(
        report->data + report->length_data + sizeof(int8_t)));
  }

  return buffer;
}

DynamicByteBuffer FakePeer::CreateScanResponseReportEvent() const {
  BT_DEBUG_ASSERT(scannable_);
  size_t param_size = sizeof(hci_spec::LEMetaEventParams) +
                      sizeof(hci_spec::LEAdvertisingReportSubeventParams) +
                      sizeof(hci_spec::LEAdvertisingReportData) + scan_rsp_.size() + sizeof(int8_t);

  DynamicByteBuffer buffer(sizeof(hci_spec::EventHeader) + param_size);
  MutablePacketView<hci_spec::EventHeader> event(&buffer, param_size);
  event.mutable_header()->event_code = hci_spec::kLEMetaEventCode;
  event.mutable_header()->parameter_total_size = param_size;

  auto payload = event.mutable_payload<hci_spec::LEMetaEventParams>();
  payload->subevent_code = hci_spec::kLEAdvertisingReportSubeventCode;

  auto subevent_payload =
      reinterpret_cast<hci_spec::LEAdvertisingReportSubeventParams*>(payload->subevent_parameters);
  subevent_payload->num_reports = 1;

  auto report = reinterpret_cast<hci_spec::LEAdvertisingReportData*>(subevent_payload->reports);
  WriteScanResponseReport(report);

  return buffer;
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

void FakePeer::WriteScanResponseReport(hci_spec::LEAdvertisingReportData* report) const {
  BT_DEBUG_ASSERT(scannable_);
  report->event_type = hci_spec::LEAdvertisingEventType::kScanRsp;
  report->address_type = (address_.type() == DeviceAddress::Type::kLERandom)
                             ? hci_spec::LEAddressType::kRandom
                             : hci_spec::LEAddressType::kPublic;
  report->address = address_.value();
  report->length_data = scan_rsp_.size();
  std::memcpy(report->data, scan_rsp_.data(), scan_rsp_.size());

  WriteRandomRSSI(reinterpret_cast<int8_t*>(report->data + report->length_data));
}

void FakePeer::OnRxL2CAP(hci_spec::ConnectionHandle conn, const ByteBuffer& pdu) {
  if (pdu.size() < sizeof(l2cap::BasicHeader)) {
    bt_log(WARN, "fake-hci", "malformed L2CAP packet!");
    return;
  }
  l2cap_.HandlePdu(conn, pdu);
}

void FakePeer::SendPacket(hci_spec::ConnectionHandle conn, l2cap::ChannelId cid,
                          const ByteBuffer& packet) {
  ctrl()->SendL2CAPBFrame(conn, cid, packet);
}

}  // namespace bt::testing
