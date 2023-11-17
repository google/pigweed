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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ADVERTISING_REPORT_PARSER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ADVERTISING_REPORT_PARSER_H_

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/transport/control_packets.h"

namespace bt::hci {

// Convenience class for extracting the contents of a HCI LE Advertising Report
// Event.
class AdvertisingReportParser final {
 public:
  // |event| must represent a LE Meta Event containing a LE Advertising Report
  // sub-event. The buffer that backs |event| must remain valid for the duration
  // in which this parser instance will be used.
  explicit AdvertisingReportParser(const EventPacket& event);

  // Returns the next LE Advertising report contained in this event in
  // |out_data| and the RSSI in |out_rssi|. Returns false if there were no
  // more reports to return or if a report is malformed.
  bool GetNextReport(const hci_spec::LEAdvertisingReportData** out_data,
                     int8_t* out_rssi);

  // Returns true if there are more reports to process.
  bool HasMoreReports();

  // Returns true if the parsing stopped due to malformed packet contents. This
  // is only possible in the very rare scenario in which the controller sent us
  // a payload that could not be parsed correctly.
  //
  // Users should check this after iterating through the reports to make sure
  // there was no error and avoid any further processing if necessary. This flag
  // will be lazily set if-and-only-if GetNextReport() or HasMoreReports()
  // returns false due to a parse error.
  bool encountered_error() const { return encountered_error_; }

 private:
  // True if we encountered an error while parsing the report.
  bool encountered_error_;

  // The number remaining reports that have not been processed via a call to
  // GetNextReport.
  uint8_t remaining_reports_;

  // Number of unprocessed bytes left in the report.
  size_t remaining_bytes_;

  // Pointer to the beginning of the next advertising report segment.
  const uint8_t* ptr_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisingReportParser);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ADVERTISING_REPORT_PARSER_H_
