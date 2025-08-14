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
using ::pw::async2::PollResult;
using ::pw::async2::Ready;
using ::pw::multibuf::MultiBuf;

PollResult<MultiBuf> LoopbackChannel<DataType::kDatagram>::DoPendRead(
    Context& cx) {
  if (!queue_.has_value()) {
    PW_ASYNC_STORE_WAKER(
        cx, waker_, "LoopbackChannel is waiting for incoming data");
    return Pending();
  }
  MultiBuf data = std::move(*queue_);
  queue_ = std::nullopt;
  std::move(waker_).Wake();
  return data;
}

Poll<Status> LoopbackChannel<DataType::kDatagram>::DoPendReadyToWrite(
    Context& cx) {
  if (queue_.has_value()) {
    PW_ASYNC_STORE_WAKER(
        cx,
        waker_,
        "LoopbackChannel is waiting for the incoming data to be consumed");
    return Pending();
  }
  return Ready(OkStatus());
}

Status LoopbackChannel<DataType::kDatagram>::DoStageWrite(MultiBuf&& data) {
  PW_DASSERT(!queue_.has_value());
  queue_ = std::move(data);
  std::move(waker_).Wake();
  return OkStatus();
}

async2::Poll<Status> LoopbackChannel<DataType::kDatagram>::DoPendWrite(
    async2::Context&) {
  return OkStatus();
}

async2::Poll<Status> LoopbackChannel<DataType::kDatagram>::DoPendClose(
    async2::Context&) {
  queue_.reset();
  return OkStatus();
}

PollResult<MultiBuf> LoopbackChannel<DataType::kByte>::DoPendRead(Context& cx) {
  if (queue_.empty()) {
    PW_ASYNC_STORE_WAKER(
        cx, read_waker_, "LoopbackChannel is waiting for incoming data");
    return Pending();
  }
  return std::move(queue_);
}

Status LoopbackChannel<DataType::kByte>::DoStageWrite(MultiBuf&& data) {
  if (!data.empty()) {
    bool was_empty = queue_.empty();
    queue_.PushSuffix(std::move(data));
    if (was_empty) {
      std::move(read_waker_).Wake();
    }
  }
  return OkStatus();
}

async2::Poll<Status> LoopbackChannel<DataType::kByte>::DoPendWrite(
    async2::Context&) {
  return OkStatus();
}

async2::Poll<Status> LoopbackChannel<DataType::kByte>::DoPendClose(
    async2::Context&) {
  queue_.Release();
  return OkStatus();
}

}  // namespace pw::channel
