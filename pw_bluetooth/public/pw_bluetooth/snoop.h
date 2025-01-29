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

#pragma once

#include <cstdint>
#include <mutex>

#include "pw_bluetooth/snoop.emb.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bytes/span.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/inline_var_len_entry_queue.h"
#include "pw_status/status.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth {

/// Snoop will record Rx & Tx transactions in a circular buffer. The most
/// recent transactions are saved when the buffer is full.
class Snoop {
 public:
  Snoop(chrono::VirtualSystemClock& system_clock,
        InlineVarLenEntryQueue<>& queue)
      : system_clock_(system_clock), queue_(queue) {}

  // Dump the snoop log to the log as a hex string
  Status DumpToLog();

  /// Dump the snoop log via callback
  ///
  /// The callback will be invoked multiple times as the circular buffer is
  /// traversed. The data returned in the callback should be saved directly to
  /// a file. Each callback will contain part of the file. The number of
  /// callbacks is not known ahead of time.
  ///
  /// @param callback callback to invoke
  Status Dump(const Function<Status(ConstByteSpan data)>& callback) {
    std::lock_guard lock(queue_mutex_);
    return DumpUnlocked(callback);
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
  Status DumpUnlocked(const Function<Status(ConstByteSpan data)>& callback)
      PW_NO_LOCK_SAFETY_ANALYSIS;

  /// Add an entry to the snoop log
  ///
  /// @param packet_flag Packet flags (rx/tx)
  /// @param packet Packet to save to snoop log
  /// @param scratch_entry scratch buffer used to assemble the entry
  void AddEntry(emboss::snoop_log::PacketFlags emboss_packet_flag,
                proxy::H4PacketInterface& hci_packet,
                span<uint8_t> scratch_entry);

 private:
  /// Generates the snoop log file header and sends it to the callback
  ///
  /// @param callback callback to invoke
  Status DumpSnoopLogFileHeader(
      const Function<Status(ConstByteSpan data)>& callback);

  constexpr static uint32_t kEmbossFileVersion = 1;
  chrono::VirtualSystemClock& system_clock_;
  InlineVarLenEntryQueue<>& queue_ PW_GUARDED_BY(queue_mutex_);
  sync::Mutex queue_mutex_;
};

/// SnoopBuffer is a buffer backed snoop log.
///
/// @param kTotalSize total size of the snoop log
/// @param kMaxHciPacketSize max size of an hci packet to record
template <size_t kTotalSize, size_t kMaxHciPacketSize>
class SnoopBuffer : public Snoop {
 public:
  SnoopBuffer(chrono::VirtualSystemClock& system_clock)
      : Snoop(system_clock, queue_buffer_) {}

  /// Add a Tx transaction
  ///
  /// @param packet Packet to save to snoop log
  void AddTx(proxy::H4PacketInterface& packet) {
    std::array<uint8_t, kScratchEntrySize> entry{};
    AddEntry(emboss::snoop_log::PacketFlags::SENT, packet, entry);
  }

  // Add an Rx transaction
  ///
  /// @param packet Packet to save to snoop log
  void AddRx(proxy::H4PacketInterface& packet) {
    std::array<uint8_t, kScratchEntrySize> entry{};
    AddEntry(emboss::snoop_log::PacketFlags::RECEIVED, packet, entry);
  }

 private:
  // Entry max size
  constexpr static size_t kScratchEntrySize =
      emboss::snoop_log::EntryHeader::MaxSizeInBytes() + /* hci type */ 1 +
      kMaxHciPacketSize;
  InlineVarLenEntryQueue<kTotalSize> queue_buffer_;
};

}  // namespace pw::bluetooth
