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

#include <tuple>
#include <type_traits>

#include "gtest/gtest.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/raw_method.h"
#include "pw_rpc/server_context.h"

namespace pw::rpc::internal {
namespace {

// This class tests Method implementation classes and MethodTraits
// specializations. It verifies that they provide the expected functions and
// that they correctly identify and construct the various method types.
//
// The TestService class must inherit from Service and provide the following
// methods with valid signatures for RPCs:
//
//   - Unary: a valid unary RPC member function
//   - StaticUnary: valid unary RPC static member function
//   - ServerStreaming: valid server streaming RPC member function
//   - StaticServerStreaming: valid server streaming static RPC member function
//
// The class must also provide methods with errors as described in their names:
//
//   - UnaryWrongArg
//   - StaticUnaryVoidReturn
//   - ServerStreamingBadReturn
//   - StaticServerStreamingMissingArg
//
template <typename MethodImpl, typename TestService, auto... extra_method_args>
struct MethodImplTester {
  // Test that the matches() function matches valid signatures.
  static_assert(MethodImpl::template matches<&TestService::Unary>());
  static_assert(MethodImpl::template matches<&TestService::ServerStreaming>());
  static_assert(MethodImpl::template matches<&TestService::StaticUnary>());
  static_assert(
      MethodImpl::template matches<&TestService::StaticServerStreaming>());

  // Test that the matches() function doesn't match invalid signatures.
  static_assert(!MethodImpl::template matches<&TestService::UnaryWrongArg>());
  static_assert(
      !MethodImpl::template matches<&TestService::StaticUnaryVoidReturn>());
  static_assert(
      !MethodImpl::template matches<&TestService::ServerStreamingBadReturn>());
  static_assert(!MethodImpl::template matches<
                &TestService::StaticServerStreamingMissingArg>());

  // Test the MethodTraits::kType member.
  static_assert(MethodTraits<decltype(&TestService::Unary)>::kType ==
                MethodType::kUnary);
  static_assert(MethodTraits<decltype(&TestService::StaticUnary)>::kType ==
                MethodType::kUnary);
  static_assert(MethodTraits<decltype(&TestService::ServerStreaming)>::kType ==
                MethodType::kServerStreaming);
  static_assert(
      MethodTraits<decltype(&TestService::StaticServerStreaming)>::kType ==
      MethodType::kServerStreaming);

  // Test method creation.
  static constexpr MethodImpl kUnaryMethod =
      MethodImpl::template Unary<&TestService::Unary>(1, extra_method_args...);
  static_assert(kUnaryMethod.id() == 1);

  static constexpr MethodImpl kStaticUnaryMethod =
      MethodImpl::template Unary<&TestService::StaticUnary>(
          2, extra_method_args...);
  static_assert(kStaticUnaryMethod.id() == 2);

  static constexpr MethodImpl kServerStreamingMethod =
      MethodImpl::template ServerStreaming<&TestService::ServerStreaming>(
          3, extra_method_args...);
  static_assert(kServerStreamingMethod.id() == 3);

  static constexpr MethodImpl kStaticServerStreamingMethod =
      MethodImpl::template ServerStreaming<&TestService::StaticServerStreaming>(
          4, extra_method_args...);
  static_assert(kStaticServerStreamingMethod.id() == 4);

  // Test that there is an Invalid method creation function.
  static constexpr MethodImpl kInvalidMethod = MethodImpl::Invalid();
  static_assert(kInvalidMethod.id() == 0);

  // Provide a method that tests can call to ensure this class is instantiated.
  bool MethodImplIsValid() const {
    return true;  // If this class compiles, the MethodImpl passes this test.
  }
};

}  // namespace
}  // namespace pw::rpc::internal
