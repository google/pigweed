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

template <typename...>
struct MatchesTypes {};

template <auto...>
struct CreationArgs {};

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
//   - ClientStreaming: valid client streaming RPC member function
//   - StaticClientStreaming: valid client streaming static RPC member function
//   - BidirectionalStreaming: valid bidirectional streaming RPC member function
//   - StaticBidirectionalStreaming: bidirectional streaming static RPC
//     member function
//
template <typename MethodImpl, typename TestService>
class MethodImplTests {
 public:
  template <typename... ExtraTypes, auto... extra_args>
  constexpr bool Pass(const MatchesTypes<ExtraTypes...>& = {},
                      const CreationArgs<extra_args...>& = {}) const {
    return Matches<ExtraTypes...>().Pass() && Type().Pass() &&
           Creation<extra_args...>().Pass();
  }

 private:
  template <typename... ExtraTypes>
  struct Matches {
    constexpr bool Pass() const { return true; }

    // Test that the matches() function matches valid signatures.
    static_assert(
        MethodImpl::template matches<&TestService::Unary, ExtraTypes...>());
    static_assert(MethodImpl::template matches<&TestService::StaticUnary,
                                               ExtraTypes...>());

    static_assert(MethodImpl::template matches<&TestService::ServerStreaming,
                                               ExtraTypes...>());
    static_assert(
        MethodImpl::template matches<&TestService::StaticServerStreaming,
                                     ExtraTypes...>());

    static_assert(MethodImpl::template matches<&TestService::ClientStreaming,
                                               ExtraTypes...>());
    static_assert(
        MethodImpl::template matches<&TestService::StaticClientStreaming,
                                     ExtraTypes...>());

    static_assert(
        MethodImpl::template matches<&TestService::BidirectionalStreaming,
                                     ExtraTypes...>());
    static_assert(
        MethodImpl::template matches<&TestService::StaticBidirectionalStreaming,
                                     ExtraTypes...>());

    // Test that the matches() function does not match the wrong method type.
    static_assert(!MethodImpl::template matches<&TestService::UnaryWrongArg,
                                                ExtraTypes...>());
    static_assert(
        !MethodImpl::template matches<&TestService::StaticUnaryVoidReturn,
                                      ExtraTypes...>());

    static_assert(
        !MethodImpl::template matches<&TestService::ServerStreamingBadReturn,
                                      ExtraTypes...>());
    static_assert(!MethodImpl::template matches<
                  &TestService::StaticServerStreamingMissingArg,
                  ExtraTypes...>());

    static_assert(
        !MethodImpl::template matches<&TestService::ClientStreamingBadReturn,
                                      ExtraTypes...>());
    static_assert(!MethodImpl::template matches<
                  &TestService::StaticClientStreamingMissingArg,
                  ExtraTypes...>());

    static_assert(!MethodImpl::template matches<
                  &TestService::BidirectionalStreamingBadReturn,
                  ExtraTypes...>());
    static_assert(!MethodImpl::template matches<
                  &TestService::StaticBidirectionalStreamingMissingArg,
                  ExtraTypes...>());
  };

  // Check that MethodTraits resolves to the correct value for kType.
  struct Type {
    constexpr bool Pass() const { return true; }

    static_assert(MethodTraits<decltype(&TestService::Unary)>::kType ==
                  MethodType::kUnary);
    static_assert(MethodTraits<decltype(&TestService::StaticUnary)>::kType ==
                  MethodType::kUnary);
    static_assert(
        MethodTraits<decltype(&TestService::ServerStreaming)>::kType ==
        MethodType::kServerStreaming);
    static_assert(
        MethodTraits<decltype(&TestService::StaticServerStreaming)>::kType ==
        MethodType::kServerStreaming);
    static_assert(
        MethodTraits<decltype(&TestService::ClientStreaming)>::kType ==
        MethodType::kClientStreaming);
    static_assert(
        MethodTraits<decltype(&TestService::StaticClientStreaming)>::kType ==
        MethodType::kClientStreaming);
    static_assert(
        MethodTraits<decltype(&TestService::BidirectionalStreaming)>::kType ==
        MethodType::kBidirectionalStreaming);
    static_assert(
        MethodTraits<
            decltype(&TestService::StaticBidirectionalStreaming)>::kType ==
        MethodType::kBidirectionalStreaming);
  };

  // Test method creation.
  template <auto... extra_args>
  struct Creation {
    constexpr bool Pass() const { return true; }

    static constexpr MethodImpl kUnaryMethod =
        MethodImpl::template Unary<&TestService::Unary>(1, extra_args...);
    static_assert(kUnaryMethod.id() == 1);

    static constexpr MethodImpl kStaticUnaryMethod =
        MethodImpl::template Unary<&TestService::StaticUnary>(2, extra_args...);
    static_assert(kStaticUnaryMethod.id() == 2);

    static constexpr MethodImpl kServerStreamingMethod =
        MethodImpl::template ServerStreaming<&TestService::ServerStreaming>(
            3, extra_args...);
    static_assert(kServerStreamingMethod.id() == 3);

    static constexpr MethodImpl kStaticServerStreamingMethod =
        MethodImpl::template ServerStreaming<
            &TestService::StaticServerStreaming>(4, extra_args...);
    static_assert(kStaticServerStreamingMethod.id() == 4);

    static constexpr MethodImpl kClientStreamingMethod =
        MethodImpl::template ClientStreaming<&TestService::ClientStreaming>(
            5, extra_args...);
    static_assert(kClientStreamingMethod.id() == 5);

    static constexpr MethodImpl kStaticClientStreamingMethod =
        MethodImpl::template ClientStreaming<
            &TestService::StaticClientStreaming>(6, extra_args...);
    static_assert(kStaticClientStreamingMethod.id() == 6);

    static constexpr MethodImpl kBidirectionalStreamingMethod =
        MethodImpl::template BidirectionalStreaming<
            &TestService::BidirectionalStreaming>(7, extra_args...);
    static_assert(kBidirectionalStreamingMethod.id() == 7);

    static constexpr MethodImpl kStaticBidirectionalStreamingMethod =
        MethodImpl::template BidirectionalStreaming<
            &TestService::StaticBidirectionalStreaming>(8, extra_args...);
    static_assert(kStaticBidirectionalStreamingMethod.id() == 8);

    // Test that there is an Invalid method creation function.
    static constexpr MethodImpl kInvalidMethod = MethodImpl::Invalid();
    static_assert(kInvalidMethod.id() == 0);
  };
};

}  // namespace pw::rpc::internal
