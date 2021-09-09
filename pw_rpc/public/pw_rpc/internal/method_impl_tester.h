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
#include "pw_rpc/raw/internal/method.h"
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
//   - Unary: synchronous unary RPC member function
//   - StaticUnary: synchronous unary RPC static member function
//   - AsyncUnary: asynchronous unary RPC member function
//   - StaticAsyncUnary: asynchronous unary RPC static member function
//   - ServerStreaming: server streaming RPC member function
//   - StaticServerStreaming: server streaming static RPC member function
//   - ClientStreaming: client streaming RPC member function
//   - StaticClientStreaming: client streaming static RPC member function
//   - BidirectionalStreaming: bidirectional streaming RPC member function
//   - StaticBidirectionalStreaming: bidirectional streaming static RPC
//         member function
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

    static_assert(MethodImpl::template matches<&TestService::AsyncUnary,
                                               ExtraTypes...>());
    static_assert(MethodImpl::template matches<&TestService::StaticAsyncUnary,
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
    static_assert(MethodTraits<decltype(&TestService::Unary)>::kSynchronous);
    static_assert(MethodTraits<decltype(&TestService::StaticUnary)>::kType ==
                  MethodType::kUnary);
    static_assert(
        MethodTraits<decltype(&TestService::StaticUnary)>::kSynchronous);

    static_assert(MethodTraits<decltype(&TestService::AsyncUnary)>::kType ==
                  MethodType::kUnary);
    static_assert(
        !MethodTraits<decltype(&TestService::AsyncUnary)>::kSynchronous);
    static_assert(
        MethodTraits<decltype(&TestService::StaticAsyncUnary)>::kType ==
        MethodType::kUnary);
    static_assert(
        !MethodTraits<decltype(&TestService::StaticAsyncUnary)>::kSynchronous);

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
        MethodImpl::template SynchronousUnary<&TestService::Unary>(
            1, extra_args...);
    static_assert(kUnaryMethod.id() == 1);

    static constexpr MethodImpl kStaticUnaryMethod =
        MethodImpl::template SynchronousUnary<&TestService::StaticUnary>(
            2, extra_args...);
    static_assert(kStaticUnaryMethod.id() == 2);

    static constexpr MethodImpl kAsyncUnaryMethod =
        MethodImpl::template AsynchronousUnary<&TestService::AsyncUnary>(
            3, extra_args...);
    static_assert(kAsyncUnaryMethod.id() == 3);

    static constexpr MethodImpl kStaticAsyncUnaryMethod =
        MethodImpl::template AsynchronousUnary<&TestService::StaticAsyncUnary>(
            4, extra_args...);
    static_assert(kStaticAsyncUnaryMethod.id() == 4);

    static constexpr MethodImpl kServerStreamingMethod =
        MethodImpl::template ServerStreaming<&TestService::ServerStreaming>(
            5, extra_args...);
    static_assert(kServerStreamingMethod.id() == 5);

    static constexpr MethodImpl kStaticServerStreamingMethod =
        MethodImpl::template ServerStreaming<
            &TestService::StaticServerStreaming>(6, extra_args...);
    static_assert(kStaticServerStreamingMethod.id() == 6);

    static constexpr MethodImpl kClientStreamingMethod =
        MethodImpl::template ClientStreaming<&TestService::ClientStreaming>(
            7, extra_args...);
    static_assert(kClientStreamingMethod.id() == 7);

    static constexpr MethodImpl kStaticClientStreamingMethod =
        MethodImpl::template ClientStreaming<
            &TestService::StaticClientStreaming>(8, extra_args...);
    static_assert(kStaticClientStreamingMethod.id() == 8);

    static constexpr MethodImpl kBidirectionalStreamingMethod =
        MethodImpl::template BidirectionalStreaming<
            &TestService::BidirectionalStreaming>(9, extra_args...);
    static_assert(kBidirectionalStreamingMethod.id() == 9);

    static constexpr MethodImpl kStaticBidirectionalStreamingMethod =
        MethodImpl::template BidirectionalStreaming<
            &TestService::StaticBidirectionalStreaming>(10, extra_args...);
    static_assert(kStaticBidirectionalStreamingMethod.id() == 10);

    // Test that there is an Invalid method creation function.
    static constexpr MethodImpl kInvalidMethod = MethodImpl::Invalid();
    static_assert(kInvalidMethod.id() == 0);
  };
};

}  // namespace pw::rpc::internal
