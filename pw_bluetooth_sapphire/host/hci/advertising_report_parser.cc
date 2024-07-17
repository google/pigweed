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

namespace bt::hci {

AdvertisingReportParser::AdvertisingReportParser(const EmbossEventPacket& event)
    : encountered_error_(false) {
  BT_DEBUG_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);

  auto view = event.view<pw::bluetooth::emboss::LEMetaEventView>();

  BT_DEBUG_ASSERT(view.subevent_code().Read() ==
                  hci_spec::kLEAdvertisingReportSubeventCode);

  auto subevent_view =
      event.view<pw::bluetooth::emboss::LEAdvertisingReportSubeventView>();

  remaining_reports_ = subevent_view.num_reports().Read();
  remaining_bytes_ = subevent_view.reports_size().Read();
  ptr_ = subevent_view.reports().BackingStorage().data();
}

bool AdvertisingReportParser::GetNextReport(
    pw::bluetooth::emboss::LEAdvertisingReportDataView* out_data,
    int8_t* out_rssi) {
  BT_DEBUG_ASSERT(out_rssi);

  if (encountered_error_ || !HasMoreReports()) {
    return false;
  }

  // Construct an incomplete view at first to read the |data_length| field.
  auto report = pw::bluetooth::emboss::MakeLEAdvertisingReportDataView(
      ptr_, pw::bluetooth::emboss::LEAdvertisingReportData::MinSizeInBytes());

  int32_t data_size = report.data_length().Read();
  size_t report_size =
      pw::bluetooth::emboss::LEAdvertisingReportData::MinSizeInBytes() +
      data_size;
  if (report_size > remaining_bytes_) {
    // Report exceeds the bounds of the packet.
    encountered_error_ = true;
    return false;
  }

  // Remake the view with the proper size.
  *out_data =
      pw::bluetooth::emboss::MakeLEAdvertisingReportDataView(ptr_, report_size);
  *out_rssi = out_data->rssi().Read();

  remaining_bytes_ -= report_size;
  --remaining_reports_;
  ptr_ += report_size;

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
