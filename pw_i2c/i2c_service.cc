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
#include "pw_i2c/i2c_service.h"

#include <algorithm>
#include <chrono>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/vector.h"
#include "pw_i2c/address.h"
#include "pw_rpc/pwpb/server_reader_writer.h"
#include "pw_status/status.h"

namespace pw::i2c {
namespace {

constexpr auto kI2cTimeout =
    chrono::SystemClock::for_at_least(std::chrono::milliseconds(100));

}  // namespace

void I2cService::I2cWrite(
    const pwpb::I2cWriteRequest::Message& request,
    rpc::PwpbUnaryResponder<pwpb::I2cWriteResponse::Message>& responder) {
  Initiator* initiator = initiator_selector_(request.bus_index);
  if (initiator == nullptr) {
    responder.Finish({}, Status::InvalidArgument()).IgnoreError();
    return;
  }

  constexpr auto kMaxWriteSize =
      pwpb::I2cWriteRequest::kRegisterAddressMaxSize +
      pwpb::I2cWriteRequest::kValueMaxSize;
  Vector<std::byte, kMaxWriteSize> write_buffer{};
  write_buffer.assign(std::begin(request.register_address),
                      std::end(request.register_address));
  std::copy(std::begin(request.value),
            std::end(request.value),
            std::back_inserter(write_buffer));
  auto result = initiator->WriteFor(
      Address{static_cast<uint16_t>(request.target_address)},
      write_buffer,
      kI2cTimeout);
  responder.Finish({}, result).IgnoreError();
}

void I2cService::I2cRead(
    const pwpb::I2cReadRequest::Message& request,
    rpc::PwpbUnaryResponder<pwpb::I2cReadResponse::Message>& responder) {
  constexpr auto kMaxReadSize = pwpb::I2cReadResponse::kValueMaxSize;

  Initiator* initiator = initiator_selector_(request.bus_index);
  if (initiator == nullptr || request.read_size > kMaxReadSize) {
    responder.Finish({}, Status::InvalidArgument()).IgnoreError();
    return;
  }
  Vector<std::byte, kMaxReadSize> value{};
  value.resize(request.read_size);
  auto result = initiator->WriteReadFor(
      Address{static_cast<uint16_t>(request.target_address)},
      request.register_address,
      {value.data(), value.size()},
      kI2cTimeout);

  if (result.ok()) {
    responder.Finish({value}, OkStatus()).IgnoreError();
  } else {
    responder.Finish({}, result).IgnoreError();
  }
}

}  // namespace pw::i2c
