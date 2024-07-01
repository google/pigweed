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
                   bool scannable,
                   bool send_advertising_report)
    : controller_(nullptr),
      address_(address),
      name_("FakePeer"),
      connected_(false),
      connectable_(connectable),
      scannable_(scannable),
      advertising_enabled_(true),
      directed_(false),
      address_resolved_(false),
      send_advertising_report_(send_advertising_report),
      connect_status_(pw::bluetooth::emboss::StatusCode::SUCCESS),
      connect_response_(pw::bluetooth::emboss::StatusCode::SUCCESS),
      force_pending_connect_(false),
      supports_ll_conn_update_procedure_(true),
      le_features_(hci_spec::LESupportedFeatures{0}),
      l2cap_(fit::bind_member<&FakePeer::SendPacket>(this)),
      gatt_server_(this),
      sdp_server_(pw_dispatcher) {
  signaling_server_.RegisterWithL2cap(&l2cap_);
  gatt_server_.RegisterWithL2cap(&l2cap_);
  sdp_server_.RegisterWithL2cap(&l2cap_);
}

void FakePeer::set_scan_response(const ByteBuffer& data) {
  BT_DEBUG_ASSERT(scannable_);
  scan_response_ = DynamicByteBuffer(data);
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
                          const ByteBuffer& packet) const {
  controller()->SendL2CAPBFrame(conn, cid, packet);
}

void FakePeer::WriteScanResponseReport(
    hci_spec::LEAdvertisingReportData* report) const {
  BT_DEBUG_ASSERT(scannable_);
  report->event_type = hci_spec::LEAdvertisingEventType::kScanRsp;

  report->address = address_.value();
  report->address_type = hci_spec::LEAddressType::kPublic;
  if (address_.type() == DeviceAddress::Type::kLERandom) {
    report->address_type = hci_spec::LEAddressType::kRandom;
  }

  report->length_data = scan_response_.size();
  std::memcpy(report->data, scan_response_.data(), scan_response_.size());

  report->data[report->length_data] = rssi();
}

DynamicByteBuffer FakePeer::BuildLegacyAdvertisingReportEvent(
    bool include_scan_rsp) const {
  BT_DEBUG_ASSERT(advertising_data_.size() <=
                  hci_spec::kMaxLEAdvertisingDataLength);
  size_t param_size = sizeof(hci_spec::LEMetaEventParams) +
                      sizeof(hci_spec::LEAdvertisingReportSubeventParams) +
                      sizeof(hci_spec::LEAdvertisingReportData) +
                      advertising_data_.size() + sizeof(int8_t);

  if (include_scan_rsp) {
    BT_DEBUG_ASSERT(scannable_);
    BT_DEBUG_ASSERT(scan_response_.size() <=
                    hci_spec::kMaxLEAdvertisingDataLength);
    param_size += sizeof(hci_spec::LEAdvertisingReportData) +
                  scan_response_.size() + sizeof(int8_t);
  }

  DynamicByteBuffer buffer(sizeof(hci_spec::EventHeader) + param_size);
  MutablePacketView<hci_spec::EventHeader> event(&buffer, param_size);
  event.mutable_header()->event_code = hci_spec::kLEMetaEventCode;
  event.mutable_header()->parameter_total_size = param_size;

  auto payload = event.mutable_payload<hci_spec::LEMetaEventParams>();
  payload->subevent_code = hci_spec::kLEAdvertisingReportSubeventCode;

  auto subevent_payload =
      reinterpret_cast<hci_spec::LEAdvertisingReportSubeventParams*>(
          payload->subevent_parameters);
  subevent_payload->num_reports = 1;
  if (include_scan_rsp) {
    subevent_payload->num_reports++;
  }
  auto report = reinterpret_cast<hci_spec::LEAdvertisingReportData*>(
      subevent_payload->reports);
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
    report->address_type = hci_spec::LEAddressType::kRandom;
    if (address_resolved_) {
      report->address_type = hci_spec::LEAddressType::kRandomIdentity;
    }
  } else {
    report->address_type = hci_spec::LEAddressType::kPublic;
    if (address_resolved_) {
      report->address_type = hci_spec::LEAddressType::kPublicIdentity;
    }
  }

  report->address = address_.value();
  report->length_data = advertising_data_.size();
  std::memcpy(report->data, advertising_data_.data(), advertising_data_.size());
  report->data[report->length_data] = rssi();

  if (include_scan_rsp) {
    auto* scan_response_report =
        reinterpret_cast<hci_spec::LEAdvertisingReportData*>(
            report->data + report->length_data + sizeof(int8_t));
    WriteScanResponseReport(scan_response_report);
  }

  return buffer;
}

DynamicByteBuffer FakePeer::BuildLegacyScanResponseReportEvent() const {
  BT_DEBUG_ASSERT(scannable_);
  BT_DEBUG_ASSERT(scan_response_.size() <=
                  hci_spec::kMaxLEAdvertisingDataLength);
  size_t param_size = sizeof(hci_spec::LEMetaEventParams) +
                      sizeof(hci_spec::LEAdvertisingReportSubeventParams) +
                      sizeof(hci_spec::LEAdvertisingReportData) +
                      scan_response_.size() + sizeof(int8_t);

  DynamicByteBuffer buffer(sizeof(hci_spec::EventHeader) + param_size);
  MutablePacketView<hci_spec::EventHeader> event(&buffer, param_size);
  event.mutable_header()->event_code = hci_spec::kLEMetaEventCode;
  event.mutable_header()->parameter_total_size = param_size;

  auto payload = event.mutable_payload<hci_spec::LEMetaEventParams>();
  payload->subevent_code = hci_spec::kLEAdvertisingReportSubeventCode;

  auto subevent_payload =
      reinterpret_cast<hci_spec::LEAdvertisingReportSubeventParams*>(
          payload->subevent_parameters);
  subevent_payload->num_reports = 1;

  auto report = reinterpret_cast<hci_spec::LEAdvertisingReportData*>(
      subevent_payload->reports);
  WriteScanResponseReport(report);

  return buffer;
}

void FakePeer::FillExtendedAdvertisingReport(
    LEExtendedAdvertisingReportDataWriter report,
    const ByteBuffer& data,
    bool is_fragmented,
    bool is_scan_response) const {
  if (use_extended_advertising_pdus_) {
    report.event_type().directed().Write(directed_);
    report.event_type().connectable().Write(connectable_);
    report.event_type().scannable().Write(scannable_);
    report.event_type().scan_response().Write(is_scan_response);

    if (is_fragmented) {
      report.event_type().data_status().Write(
          pw::bluetooth::emboss::LEAdvertisingDataStatus::INCOMPLETE);
    } else {
      report.event_type().data_status().Write(
          pw::bluetooth::emboss::LEAdvertisingDataStatus::COMPLETE);
    }
  } else {
    report.event_type().legacy().Write(true);
    if (is_scan_response) {
      report.event_type().scan_response().Write(true);
    }

    if (directed_) {  // ADV_DIRECT_IND
      report.event_type().directed().Write(true);
      report.event_type().connectable().Write(true);
    } else if (connectable_) {  // ADV_IND
      report.event_type().connectable().Write(true);
      report.event_type().scannable().Write(true);
    } else if (scannable_) {  // ADV_SCAN_IND
      report.event_type().scannable().Write(true);
    }
    // else ADV_NONCONN_IND
  }

  if (address_.type() == DeviceAddress::Type::kLERandom) {
    if (address_resolved_) {
      report.address_type().Write(
          pw::bluetooth::emboss::LEExtendedAddressType::RANDOM_IDENTITY);
    } else {
      report.address_type().Write(
          pw::bluetooth::emboss::LEExtendedAddressType::RANDOM);
    }
  } else {
    if (address_resolved_) {
      report.address_type().Write(
          pw::bluetooth::emboss::LEExtendedAddressType::PUBLIC_IDENTITY);
    } else {
      report.address_type().Write(
          pw::bluetooth::emboss::LEExtendedAddressType::PUBLIC);
    }
  }

  report.address().bd_addr().CopyFrom(address_.value().view().bd_addr());
  report.primary_phy().Write(
      pw::bluetooth::emboss::LEPrimaryAdvertisingPHY::LE_1M);
  report.secondary_phy().Write(
      pw::bluetooth::emboss::LESecondaryAdvertisingPHY::NONE);
  report.advertising_sid().Write(0);
  report.tx_power().Write(tx_power());
  report.rssi().Write(rssi());
  report.periodic_advertising_interval().Write(0);

  // skip direct_address_type and direct_address for now since we don't use it

  report.data_length().Write(data.size());
  std::memcpy(report.data().BackingStorage().begin(), data.data(), data.size());
}

DynamicByteBuffer FakePeer::BuildExtendedAdvertisingReports(
    const ByteBuffer& data, bool is_scan_response) const {
  using pw::bluetooth::emboss::LEExtendedAdvertisingReportDataWriter;
  using pw::bluetooth::emboss::LEExtendedAdvertisingReportSubeventWriter;

  size_t num_full_reports =
      data.size() / hci_spec::kMaxPduLEExtendedAdvertisingDataLength;
  size_t full_report_size =
      pw::bluetooth::emboss::LEExtendedAdvertisingReportData::MinSizeInBytes() +
      hci_spec::kMaxPduLEExtendedAdvertisingDataLength;
  size_t last_report_size =
      pw::bluetooth::emboss::LEExtendedAdvertisingReportData::MinSizeInBytes() +
      (data.size() % hci_spec::kMaxPduLEExtendedAdvertisingDataLength);

  size_t reports_size = num_full_reports * full_report_size + last_report_size;
  size_t packet_size =
      pw::bluetooth::emboss::LEExtendedAdvertisingReportSubevent::
          MinSizeInBytes() +
      reports_size;

  auto event =
      hci::EmbossEventPacket::New<LEExtendedAdvertisingReportSubeventWriter>(
          hci_spec::kLEMetaEventCode, packet_size);
  auto packet =
      event.view<LEExtendedAdvertisingReportSubeventWriter>(reports_size);
  packet.le_meta_event().subevent_code().Write(
      hci_spec::kLEExtendedAdvertisingReportSubeventCode);

  uint8_t num_reports = num_full_reports + 1;
  packet.num_reports().Write(num_reports);

  for (size_t i = 0; i < num_full_reports; i++) {
    bool is_fragmented = false;
    if (num_reports > 1) {
      is_fragmented = true;
    }

    LEExtendedAdvertisingReportDataWriter report(
        packet.reports().BackingStorage().begin() + full_report_size * i,
        full_report_size);
    FillExtendedAdvertisingReport(
        report, data, is_fragmented, is_scan_response);
  }

  LEExtendedAdvertisingReportDataWriter report(
      packet.reports().BackingStorage().begin() +
          full_report_size * num_full_reports,
      last_report_size);
  FillExtendedAdvertisingReport(
      report, data, /*is_fragmented=*/false, is_scan_response);

  return event.release();
}

DynamicByteBuffer FakePeer::BuildExtendedAdvertisingReportEvent() const {
  BT_DEBUG_ASSERT(advertising_data_.size() <=
                  hci_spec::kMaxLEExtendedAdvertisingDataLength);
  return BuildExtendedAdvertisingReports(advertising_data_,
                                         /*is_scan_response=*/false);
}

DynamicByteBuffer FakePeer::BuildExtendedScanResponseEvent() const {
  BT_DEBUG_ASSERT(scannable_);
  BT_DEBUG_ASSERT(scan_response_.size() <=
                  hci_spec::kMaxLEExtendedAdvertisingDataLength);
  return BuildExtendedAdvertisingReports(scan_response_,
                                         /*is_scan_response=*/true);
}

}  // namespace bt::testing
