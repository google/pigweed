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

#include "pw_channel/forwarding_channel.h"

namespace pw::channel::internal {

async2::Poll<Result<multibuf::MultiBuf>>
ForwardingChannel<DataType::kDatagram>::DoPendRead(async2::Context& cx)
    PW_NO_LOCK_SAFETY_ANALYSIS {
  std::lock_guard lock(pair_.mutex_);
  if (pair_.closed_) {
    return Status::FailedPrecondition();
  }
  if (!read_queue_.has_value()) {
    waker_ = cx.GetWaker(async2::WaitReason::Unspecified());
    return async2::Pending();
  }
  auto read_data = std::move(*read_queue_);
  read_queue_.reset();
  std::move(sibling_.waker_).Wake();
  return read_data;
}
async2::Poll<> ForwardingChannel<DataType::kDatagram>::DoPendReadyToWrite(
    async2::Context& cx) PW_NO_LOCK_SAFETY_ANALYSIS {
  std::lock_guard lock(pair_.mutex_);
  if (pair_.closed_) {
    return async2::Ready();
  }
  if (sibling_.read_queue_.has_value()) {
    waker_ = cx.GetWaker(async2::WaitReason::Unspecified());
    return async2::Pending();
  }
  return async2::Ready();
}

Result<channel::WriteToken> ForwardingChannel<DataType::kDatagram>::DoWrite(
    multibuf::MultiBuf&& data) PW_NO_LOCK_SAFETY_ANALYSIS {
  std::lock_guard lock(pair_.mutex_);
  if (pair_.closed_) {
    return Status::FailedPrecondition();
  }
  PW_DASSERT(!sibling_.read_queue_.has_value());
  sibling_.read_queue_ = std::move(data);
  const uint32_t token = ++write_token_;
  std::move(sibling_.waker_).Wake();
  return CreateWriteToken(token);
}

async2::Poll<Result<channel::WriteToken>>
ForwardingChannel<DataType::kDatagram>::DoPendFlush(async2::Context&) {
  std::lock_guard lock(pair_.mutex_);
  if (pair_.closed_) {
    return Status::FailedPrecondition();
  }
  return async2::Ready(CreateWriteToken(write_token_));
}

async2::Poll<Status> ForwardingChannel<DataType::kDatagram>::DoPendClose(
    async2::Context&) PW_NO_LOCK_SAFETY_ANALYSIS {
  std::lock_guard lock(pair_.mutex_);
  if (pair_.closed_) {
    return Status::FailedPrecondition();
  }
  pair_.closed_ = true;
  read_queue_.reset();
  std::move(sibling_.waker_).Wake();
  return OkStatus();
}

async2::Poll<Result<multibuf::MultiBuf>>
ForwardingChannel<DataType::kByte>::DoPendRead(async2::Context& cx) {
  std::lock_guard lock(pair_.mutex_);
  if (pair_.closed_) {
    return Status::FailedPrecondition();
  }
  if (read_queue_.empty()) {
    read_waker_ = cx.GetWaker(async2::WaitReason::Unspecified());
    return async2::Pending();
  }
  auto read_data = std::move(read_queue_);
  read_queue_ = {};
  return read_data;
}

Result<channel::WriteToken> ForwardingChannel<DataType::kByte>::DoWrite(
    multibuf::MultiBuf&& data) PW_NO_LOCK_SAFETY_ANALYSIS {
  std::lock_guard lock(pair_.mutex_);
  if (pair_.closed_) {
    return Status::FailedPrecondition();
  }
  if (data.empty()) {
    return CreateWriteToken(write_token_);  // no data, nothing to do
  }

  write_token_ += data.size();
  sibling_.read_queue_.PushSuffix(std::move(data));
  return CreateWriteToken(write_token_);
}

async2::Poll<Result<channel::WriteToken>>
ForwardingChannel<DataType::kByte>::DoPendFlush(async2::Context&) {
  std::lock_guard lock(pair_.mutex_);
  if (pair_.closed_) {
    return Status::FailedPrecondition();
  }
  return async2::Ready(CreateWriteToken(write_token_));
}

async2::Poll<Status> ForwardingChannel<DataType::kByte>::DoPendClose(
    async2::Context&) PW_NO_LOCK_SAFETY_ANALYSIS {
  std::lock_guard lock(pair_.mutex_);
  if (pair_.closed_) {
    return Status::FailedPrecondition();
  }
  pair_.closed_ = true;
  read_queue_.Release();
  std::move(sibling_.read_waker_).Wake();
  return OkStatus();
}

}  // namespace pw::channel::internal
