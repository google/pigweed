// Copyright 2020 The Pigweed Authors
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
#include <cstring>
#include <span>

#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/method_union.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/server_context.h"
#include "pw_status/status_with_size.h"

namespace pw::rpc::internal {

// This is a fake RPC method implementation for testing only. It stores the
// channel ID, request, and payload buffer, and optionally provides a response.
class TestMethod : public Method {
 public:
  constexpr TestMethod(uint32_t id)
      : Method(id, InvokeForTest), last_channel_id_(0) {}

  uint32_t last_channel_id() const { return last_channel_id_; }
  const Packet& last_request() const { return last_request_; }

  void set_response(std::span<const std::byte> payload) { response_ = payload; }
  void set_status(Status status) { response_status_ = status; }

 private:
  static void InvokeForTest(const Method& method,
                            ServerCall& call,
                            const Packet& request) {
    const auto& test_method = static_cast<const TestMethod&>(method);
    test_method.last_channel_id_ = call.channel().id();
    test_method.last_request_ = request;
  }

  // Make these mutable so they can be set in the Invoke method, which is const.
  // The Method class is used exclusively in tests. Having these members mutable
  // allows tests to verify that the Method is invoked correctly.
  mutable uint32_t last_channel_id_;
  mutable Packet last_request_;

  std::span<const std::byte> response_;
  Status response_status_;
};

class TestMethodUnion : public MethodUnion {
 public:
  constexpr TestMethodUnion(TestMethod&& method) : impl_({.test = method}) {}

  constexpr const Method& method() const { return impl_.method; }
  constexpr const TestMethod& test_method() const { return impl_.test; }

 private:
  union {
    Method method;
    TestMethod test;
  } impl_;
};

}  // namespace pw::rpc::internal
