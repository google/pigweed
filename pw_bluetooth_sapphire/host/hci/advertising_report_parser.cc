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

#include "pw_bluetooth_sapphire/internal/host/hci/advertising_report_parser.h"

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/transport/control_packets.h"

namespace bt::hci {

AdvertisingReportParser::AdvertisingReportParser(const EventPacket& event)
    : encountered_error_(false) {
  BT_DEBUG_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);

  const auto& params = event.params<hci_spec::LEMetaEventParams>();
  BT_DEBUG_ASSERT(params.subevent_code ==
                  hci_spec::kLEAdvertisingReportSubeventCode);

  static const size_t report_packet_header_size =
      sizeof(hci_spec::LEMetaEventParams) +
      sizeof(hci_spec::LEAdvertisingReportSubeventParams);

  BT_DEBUG_ASSERT(event.view().payload_size() <= UINT8_MAX);
  BT_DEBUG_ASSERT(event.view().payload_size() >= report_packet_header_size);

  auto subevent_params =
      event.subevent_params<hci_spec::LEAdvertisingReportSubeventParams>();

  remaining_reports_ = subevent_params->num_reports;

  remaining_bytes_ = event.view().payload_size() - report_packet_header_size;
  ptr_ = subevent_params->reports;
}

bool AdvertisingReportParser::GetNextReport(
    const hci_spec::LEAdvertisingReportData** out_data, int8_t* out_rssi) {
  BT_DEBUG_ASSERT(out_data);
  BT_DEBUG_ASSERT(out_rssi);

  if (encountered_error_ || !HasMoreReports())
    return false;

  const hci_spec::LEAdvertisingReportData* data =
      reinterpret_cast<const hci_spec::LEAdvertisingReportData*>(ptr_);

  // Each report contains the all the report data, followed by the advertising
  // payload, followed by a single octet for the RSSI.
  size_t report_size = sizeof(*data) + data->length_data + 1;
  if (report_size > remaining_bytes_) {
    // Report exceeds the bounds of the packet.
    encountered_error_ = true;
    return false;
  }

  remaining_bytes_ -= report_size;
  remaining_reports_--;
  ptr_ += report_size;

  *out_data = data;
  *out_rssi = *(ptr_ - 1);

  return true;
}

bool AdvertisingReportParser::HasMoreReports() {
  if (encountered_error_)
    return false;

  if (!!remaining_reports_ != !!remaining_bytes_) {
    // There should be no bytes remaining if there are no reports left to parse.
    encountered_error_ = true;
    return false;
  }
  return !!remaining_reports_;
}

}  // namespace bt::hci
