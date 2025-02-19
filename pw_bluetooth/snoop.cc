// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth/snoop.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/snoop.emb.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bytes/span.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/inline_var_len_entry_queue.h"
#include "pw_hex_dump/hex_dump.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth {

// Dump the snoop log to the log as a hex string
Status Snoop::DumpToLog() {
  PW_LOG_INFO("Snoop Log Start");
  PW_LOG_INFO("Step 1: Copy and paste the hex data into a text file");
  PW_LOG_INFO("Step 2: Remove any extra text (e.g. file, timestamp, etc)");
  PW_LOG_INFO("Step 3: $ xxd -r -p input.hex output.snoop");
  PW_LOG_INFO("Step 4: $ wireshark output.snoop");
  Status status = Dump([](ConstByteSpan data) {
    std::array<char, 80> temp;
    pw::dump::FormattedHexDumper hex_dumper(temp);
    hex_dumper.flags.prefix_mode =
        pw::dump::FormattedHexDumper::AddressMode::kDisabled;
    hex_dumper.flags.show_ascii = false;
    hex_dumper.flags.bytes_per_line = 32;
    hex_dumper.flags.group_every = 32;
    hex_dumper.flags.show_header = false;
    Status hex_dumper_status = hex_dumper.BeginDump(data);
    if (!hex_dumper_status.ok()) {
      return hex_dumper_status;
    }
    while (hex_dumper.DumpLine().ok()) {
      PW_LOG_INFO("%s", temp.data());
    }
    return pw::OkStatus();
  });
  PW_LOG_INFO("Snoop Log End");
  return status;
}

/// Dump the snoop log via callback without locking
///
/// The callback will be invoked multiple times as the circular buffer is
/// traversed.
///
/// Note: this function does NOT lock the snoop log. Do not invoke it unless
/// the snoop log is not being used. For example, use this API to read the
/// snoop log in a crash handler where mutexes are not allowed to be taken.
///
/// @param callback callback to invoke
Status Snoop::DumpUnlocked(
    const Function<Status(ConstByteSpan data)>& callback) {
  Status status = DumpSnoopLogFileHeader(callback);
  if (!status.ok()) {
    return status;
  }

  for (const auto& entry : queue_) {
    std::pair<ConstByteSpan, ConstByteSpan> data = entry.contiguous_data();
    status = callback(data.first);
    if (!status.ok()) {
      return status;
    }
    if (!data.second.empty()) {
      status = callback(data.second);
      if (!status.ok()) {
        return status;
      }
    }
  }
  return OkStatus();
}

void Snoop::AddEntry(emboss::snoop_log::PacketFlags emboss_packet_flag,
                     proxy::H4PacketInterface& hci_packet) {
  std::lock_guard lock(queue_lock_);

  size_t hci_packet_length_to_include = std::min(
      hci_packet.GetHciSpan().size(),
      static_cast<size_t>(scratch_buffer_.size() -
                          emboss::snoop_log::EntryHeader::MaxSizeInBytes() -
                          /* hci type*/ 1));
  size_t total_entry_size = hci_packet_length_to_include + /* hci type*/ 1 +
                            emboss::snoop_log::EntryHeader::MaxSizeInBytes();
  // Ensure the scratch buffer can fit the entire entry
  PW_CHECK_INT_GE(scratch_buffer_.size(), total_entry_size);

  pw::Result<emboss::snoop_log::EntryWriter> result =
      MakeEmbossWriter<emboss::snoop_log::EntryWriter>(scratch_buffer_);
  PW_CHECK_OK(result);
  emboss::snoop_log::EntryWriter writer = result.value();
  writer.header().original_length().Write(hci_packet.GetHciSpan().size() +
                                          /* hci type*/ 1);
  writer.header().included_length().Write(hci_packet_length_to_include +
                                          /* hci type*/ 1);
  writer.header().packet_flags().Write(emboss_packet_flag);
  writer.header().cumulative_drops().Write(0);
  writer.header().timestamp_us().Write(static_cast<int64_t>(
      std::chrono::time_point_cast<std::chrono::microseconds>(
          system_clock_.now())
          .time_since_epoch()
          .count()));

  // write h4 type
  writer.packet_h4_type().Write(static_cast<uint8_t>(hci_packet.GetH4Type()));

  // write hci packet
  pw::span<uint8_t> hci_packet_trimmed{hci_packet.GetHciSpan().data(),
                                       hci_packet_length_to_include};
  PW_CHECK(TryToCopyToEmbossStruct(/*emboss_dest=*/writer.packet_hci_data(),
                                   /*src=*/hci_packet_trimmed));

  // save the entry!
  queue_.push_overwrite(
      as_bytes(span{scratch_buffer_.data(), total_entry_size}));
}

/// Generates the snoop log file header
///
/// @returns the file header
Status Snoop::DumpSnoopLogFileHeader(
    const Function<Status(ConstByteSpan data)>& callback) {
  std::array<uint8_t, 16> file_header_data;
  pw::Result<emboss::snoop_log::FileHeaderWriter> result =
      MakeEmbossWriter<emboss::snoop_log::FileHeaderWriter>(file_header_data);
  if (!result.ok()) {
    return result.status();
  }
  emboss::snoop_log::FileHeaderWriter writer = result.value();
  constexpr std::array<uint8_t, 8> kBtSnoopIdentificationPatternData = {
      0x62, 0x74, 0x73, 0x6E, 0x6F, 0x6F, 0x70, 0x00};

  auto identification_pattern_storage =
      writer.identification_pattern().BackingStorage();
  PW_ASSERT(kBtSnoopIdentificationPatternData.size() ==
            identification_pattern_storage.SizeInBytes());
  std::copy(kBtSnoopIdentificationPatternData.begin(),
            kBtSnoopIdentificationPatternData.end(),
            identification_pattern_storage.begin());
  writer.version_number().Write(kEmbossFileVersion);
  writer.datalink_type().Write(emboss::snoop_log::DataLinkType::HCI_UART_H4);
  return callback(as_bytes(span(file_header_data)));
}

}  // namespace pw::bluetooth
