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

#include "pw_channel/loopback_channel.h"

#include "pw_multibuf/multibuf.h"

namespace pw::channel {

using ::pw::async2::Context;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::WaitReason;
using ::pw::channel::WriteToken;
using ::pw::multibuf::MultiBuf;

Poll<Result<MultiBuf>> LoopbackChannel<DataType::kDatagram>::DoPollRead(
    Context& cx) {
  if (!queue_.has_value()) {
    waker_ = cx.GetWaker(WaitReason::Unspecified());
    return Pending();
  }
  MultiBuf data = std::move(*queue_);
  queue_ = std::nullopt;
  std::move(waker_).Wake();
  return data;
}

Poll<> LoopbackChannel<DataType::kDatagram>::DoPollReadyToWrite(Context& cx) {
  if (queue_.has_value()) {
    waker_ = cx.GetWaker(WaitReason::Unspecified());
    return Pending();
  }
  return Ready();
}

Result<WriteToken> LoopbackChannel<DataType::kDatagram>::DoWrite(
    MultiBuf&& data) {
  PW_DASSERT(!queue_.has_value());
  queue_ = std::move(data);
  const uint32_t token = ++write_token_;
  std::move(waker_).Wake();
  return CreateWriteToken(token);
}

async2::Poll<Result<channel::WriteToken>>
LoopbackChannel<DataType::kDatagram>::DoPollFlush(async2::Context&) {
  return async2::Ready(CreateWriteToken(write_token_));
}

async2::Poll<Status> LoopbackChannel<DataType::kDatagram>::DoPollClose(
    async2::Context&) {
  queue_.reset();
  return OkStatus();
}

Poll<Result<MultiBuf>> LoopbackChannel<DataType::kByte>::DoPollRead(
    Context& cx) {
  if (queue_.empty()) {
    read_waker_ = cx.GetWaker(WaitReason::Unspecified());
    return Pending();
  }
  return std::move(queue_);
}

Result<WriteToken> LoopbackChannel<DataType::kByte>::DoWrite(MultiBuf&& data) {
  const uint32_t token = ++write_token_;
  if (!data.empty()) {
    bool was_empty = queue_.empty();
    queue_.PushSuffix(std::move(data));
    if (was_empty) {
      std::move(read_waker_).Wake();
    }
  }
  return CreateWriteToken(token);
}

async2::Poll<Result<channel::WriteToken>>
LoopbackChannel<DataType::kByte>::DoPollFlush(async2::Context&) {
  return async2::Ready(CreateWriteToken(write_token_));
}

async2::Poll<Status> LoopbackChannel<DataType::kByte>::DoPollClose(
    async2::Context&) {
  queue_.Release();
  return OkStatus();
}

}  // namespace pw::channel
