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

#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_i2c/address.h"
#include "pw_i2c/responder.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::i2c::test {

/// @module{pw_i2c}

/// Structure to hold all mockable callbacks for the Responder.
/// Test fixtures can use this to set up and verify interactions.
/// The generic ResponderTest fixture will own an instance of this struct and
/// pass it to the NativeResponderTest constructor.
class ResponderEventsForTest : public ResponderEvents {
 public:
  bool OnStartRead() override { return on_start_read_cb(); }

  bool OnStartWrite() override { return on_start_write_cb(); }

  bool OnWrite(ConstByteSpan data) override { return on_write_cb(data); }

  Result<ConstByteSpan> OnRead() override { return on_read_cb(); }

  bool OnStop() override { return on_stop_cb(); }

  void SetOnStartReadCb(pw::Function<bool()>&& cb) {
    on_start_read_cb = std::move(cb);
  }

  void SetOnStartWriteCb(pw::Function<bool()>&& cb) {
    on_start_write_cb = std::move(cb);
  }

  void SetOnWriteCb(pw::Function<bool(ConstByteSpan)>&& cb) {
    on_write_cb = std::move(cb);
  }

  void SetOnReadCb(pw::Function<Result<ConstByteSpan>()>&& cb) {
    on_read_cb = std::move(cb);
  }

  void SetOnStopCb(pw::Function<bool()>&& cb) { on_stop_cb = std::move(cb); }

 private:
  /// Callback used to implement OnStartRead()
  pw::Function<bool()> on_start_read_cb;

  /// Callback used to implement OnStartWrite()
  pw::Function<bool()> on_start_write_cb;

  /// Callback used to implement OnWrite()
  pw::Function<bool(ConstByteSpan)> on_write_cb;

  /// Callback used to implement OnRead()
  pw::Function<Result<ConstByteSpan>()> on_read_cb;

  /// Callback used to implement OnStop()
  pw::Function<bool()> on_stop_cb;
};

/// This interface defines the contract that a backend-specific test harness
/// (NativeResponderTest) must implement to be used with the generic
/// pw::i2c::Responder tests.
class NativeResponderTestInterface : public ::testing::Test {
 public:
  /// Provides access to the backend-specific responder instance.
  /// The responder should have been configured with the callbacks provided
  /// to the NativeResponderTest constructor.
  virtual Responder& GetResponder() = 0;

  /// Simulates an I2C initiator writing data to the responder.
  /// @param write_data The data to be written by the initiator.
  /// @param send_stop If true, a STOP condition is simulated after the write.
  /// @returns
  /// * @OK: on successful simulation
  /// * Error status otherwise.
  virtual Status SimulateInitiatorWrite(ConstByteSpan write_data,
                                        bool send_stop) = 0;

  /// Simulates an I2C initiator reading data from the responder.
  /// @param buffer The buffer into which the initiator will read data.
  /// @param send_stop If true, a STOP condition is simulated after the read.
  /// @returns
  /// * @OK: on success, all bytes were written.
  /// * Error code when not all the bytes could be written.
  virtual Status SimulateInitiatorRead(ByteSpan buffer, bool send_stop) = 0;
};

/// @}

}  // namespace pw::i2c::test
