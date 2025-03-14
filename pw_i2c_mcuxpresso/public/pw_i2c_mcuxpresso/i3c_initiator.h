// Copyright 2024 The Pigweed Authors
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

#include <atomic>
#include <optional>

#include "fsl_clock.h"
#include "fsl_i3c.h"
#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_i2c/initiator.h"
#include "pw_i2c_mcuxpresso/i3c_ccc.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_sync/mutex.h"
#include "pw_sync/timed_thread_notification.h"

namespace pw::i2c {

// I2C initiator interface implementation fsl_i3c driver in NXP MCUXpresso SDK.
class I3cMcuxpressoInitiator final : public pw::i2c::Initiator {
 public:
  struct Config {
    uint32_t base_address;              // I3C peripheral base address.
    uint32_t i2c_baud_rate;             // I2C baud rate in Hz.
    uint32_t i3c_open_drain_baud_rate;  // I3C open drain baud rate in Hz.
    uint32_t i3c_push_pull_baud_rate;   // I3C push pull baud rate in Hz.
    bool enable_open_drain_stop;  // Whether to emit open-drain speed STOP.
    bool enable_open_drain_high;  // Enable Open-Drain High to be 1 PPBAUD count
                                  // for I3C messages, or 1 ODBAUD.
  };
  I3cMcuxpressoInitiator(const Config& config)
      : Initiator(Initiator::Feature::kStandard),
        config_(config),
        base_(reinterpret_cast<I3C_Type*>(config.base_address)) {}

  // Initializes the I3C controller peripheral as configured in the constructor.
  void Enable() PW_LOCKS_EXCLUDED(mutex_);

  // Deinitializes the I3C controller peripheral.
  void Disable() PW_LOCKS_EXCLUDED(mutex_);

  // Set dynamic address list, which will be used to assign dynamic addresses to
  // I3C devices on the bus during bus initialization step, or to determine the
  // type (I2C or I3C) for each transaction.
  //
  // Warning: dynamic address list can only be set once.
  pw::Status SetDynamicAddressList(
      pw::span<const uint8_t> dynamic_address_list);

  // Initialize the I3C bus (Dynamic address assignment)
  //
  // Warning: Users should set correct dynamic addresses (SetDynamicAddressList)
  // before calling this function.
  pw::Status Initialize() PW_LOCKS_EXCLUDED(mutex_);

 private:
  pw::Status DoTransferCcc(I3cCccAction rnw,
                           I3cCcc ccc_id,
                           pw::i2c::Address address,
                           pw::ByteSpan buffer);
  pw::Status DoTransferFor(span<const Message> messages,
                           chrono::SystemClock::duration timeout)
      PW_LOCKS_EXCLUDED(mutex_);

  pw::Result<i3c_bus_type_t> ValidateAndDetermineProtocol(
      span<const Message> messages) const;

  // inclusive-language: disable
  Status InitiateNonBlockingTransferUntil(
      chrono::SystemClock::time_point deadline,
      i3c_master_transfer_t* transfer);

  // Non-blocking I3C transfer callback.
  static void TransferCompleteCallback(I3C_Type* base,
                                       i3c_master_handle_t* handle,
                                       status_t status,
                                       void* initiator_ptr);

  // inclusive-language: enable

  const Config& config_;
  I3C_Type* base_;
  i3c_device_info_t* device_list_ = nullptr;
  uint8_t device_count_ = 0;
  bool enabled_ PW_GUARDED_BY(mutex_) = false;
  pw::sync::Mutex mutex_;
  std::optional<pw::Vector<uint8_t, I3C_MAX_DEVCNT>> i3c_dynamic_address_list_;

  // Transfer completion status for non-blocking I3C transfer.
  sync::TimedThreadNotification callback_complete_notification_;
  std::atomic<status_t> transfer_status_;

  // inclusive-language: disable
  i3c_master_transfer_callback_t initiator_callbacks_;
  i3c_master_handle_t handle_ PW_GUARDED_BY(mutex_);
  // inclusive-language: enable
};

}  // namespace pw::i2c
