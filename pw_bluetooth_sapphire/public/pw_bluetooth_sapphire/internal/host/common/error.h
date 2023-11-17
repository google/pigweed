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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ERROR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ERROR_H_

#include <cpp-string/string_printf.h>
#include <lib/fit/result.h>

#include <type_traits>
#include <variant>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/host_error.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"

#pragma clang diagnostic ignored "-Wvariadic-macros"
#pragma clang diagnostic ignored \
    "-Wgnu-statement-expression-from-macro-expansion"

namespace bt {

// Type used to hold either a HostError or a ProtocolErrorCode, a
// protocol-defined code. This can not be constructed in such a way to represent
// a success or to contain the product of a successful operation, but to be used
// as the error type parameter of a generic result type like
// fit::result<Error<…>> or fit::result<Error<…>, V>.
//
// Errors can be directly constructed from HostErrors. ProtocolErrorCodes whose
// possible values all represent errors can also be used to construct Errors.
// Otherwise, ProtocolErrorCodes like that used by HCI must be converted using
// ToResult in order to capture success values.
template <typename ProtocolErrorCode>
class Error;

// Required trait for ProtocolErrorCode types.
template <typename ProtocolErrorCode>
struct ProtocolErrorTraits {
  // Returns a string representation of the given ProtocolErrorCode value.
  static std::string ToString(ProtocolErrorCode);

  // Optional: returns true if the given ProtocolErrorCode value represents
  // success. If no such value exists, do not declare this static function in
  // the specialization. static constexpr bool is_success(ProtocolErrorCode);
};

// Marker used to indicate that an Error holds only HostError.
class NoProtocolError {
  constexpr NoProtocolError() = delete;
};

template <>
struct ProtocolErrorTraits<NoProtocolError> {
  // This won't be called but still needs to be stubbed out to link correctly.
  static std::string ToString(NoProtocolError) {
    BT_ASSERT(false);
    return std::string();
  }
};

namespace detail {

// Detects whether the given expression implicitly converts to a bt::Error.
template <typename T, typename = void>
struct IsError : std::false_type {};

// This specialization is used when
//   1. T can be deduced as a template template (i.e. T<U>)
//   2. A function that takes Error<U> would accept a T<U>&& value as its
//   parameter
template <template <typename> class T, typename U>
struct IsError<T<U>,
               std::void_t<decltype(std::declval<void (&)(Error<U>)>()(
                   std::declval<T<U>>()))>> : std::true_type {};

template <typename T>
constexpr bool IsErrorV = IsError<T>::value;

// Detects whether ProtocolErrorTraits<ProtocolErrorCode>::is_success has been
// declared.
template <typename ProtocolErrorCode, typename = void>
struct CanRepresentSuccess : std::false_type {};

template <typename ProtocolErrorCode>
struct CanRepresentSuccess<
    ProtocolErrorCode,
    std::void_t<decltype(ProtocolErrorTraits<ProtocolErrorCode>::is_success(
        std::declval<ProtocolErrorCode>()))>> : std::true_type {};

template <typename ProtocolErrorCode>
constexpr bool CanRepresentSuccessV =
    CanRepresentSuccess<ProtocolErrorCode>::value;

}  // namespace detail

// Create a fit::result<Error<…>> from a HostError. The template parameter may
// be omitted to default to an fit::result<Error<NoProtocolError>> in the case
// that it's not useful to specify the kind of protocol error that the result
// could hold instead.
template <typename ProtocolErrorCode = NoProtocolError>
[[nodiscard]] constexpr fit::result<Error<ProtocolErrorCode>> ToResult(
    HostError host_error) {
  return fit::error(Error<ProtocolErrorCode>(host_error));
}

// Create a fit::result<Error<…>> from a protocol error.
// This overload doesn't collide with the above when instantiated with
// <HostError>, because this would try to construct an invalid Error<HostError>.
template <typename ProtocolErrorCode>
[[nodiscard]] constexpr fit::result<Error<ProtocolErrorCode>> ToResult(
    ProtocolErrorCode proto_error) {
  if constexpr (detail::CanRepresentSuccessV<ProtocolErrorCode>) {
    if (ProtocolErrorTraits<ProtocolErrorCode>::is_success(proto_error)) {
      return fit::success();
    }
  }
  return fit::error(Error(std::move(proto_error)));
}

template <typename ProtocolErrorCode = NoProtocolError>
class [[nodiscard]] Error {
  static_assert(!std::is_same_v<HostError, ProtocolErrorCode>,
                "HostError can not be a protocol error");
  static_assert(!detail::IsErrorV<ProtocolErrorCode>,
                "ProtocolErrorCode can not be a bt::Error");

 public:
  Error() = delete;
  ~Error() = default;
  constexpr Error(const Error&) = default;
  constexpr Error(Error&&) noexcept = default;
  constexpr Error& operator=(const Error&) = default;
  constexpr Error& operator=(Error&&) noexcept = default;

  constexpr explicit Error(const HostError& host_error) : error_(host_error) {}

  // This is disabled if ProtocolErrorCode may hold a value that means success,
  // leaving only the private ctor. Instead use ToResult(ProtocolErrorCode),
  // whose return value may hold success.
  template <typename T = ProtocolErrorCode,
            std::enable_if_t<!detail::CanRepresentSuccessV<T>, int> = 0>
  constexpr explicit Error(const ProtocolErrorCode& proto_error)
      : error_(proto_error) {}

  // Intentionally implicit conversion from Error<NoProtocolError> that holds
  // only HostErrors. This allows any Error<…> to be compared to an
  // Error<NoProtocolError>'s HostError payload. Also, functions that accept
  // Error<…> will take Error<NoProtocolError> without an explicit conversion.
  //
  // Example:
  //   void Foo(Error<BarErrorCode>);
  //   Foo(ToResult(HostError::kTimedOut));  // Compiles without having to write
  //   BarErrorCode
  //
  // For safety, this implicit conversion does not "chain" to allow bare
  // ProtocolErrorCodes or HostErrors to be converted into Error or fit::result.
  //
  // The seemingly-extraneous template parameter serves to disable this overload
  // when |*this| is an Error<NoProtocolError>
  template <typename T = ProtocolErrorCode,
            std::enable_if_t<Error<T>::may_hold_protocol_error(), int> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Error(const Error<NoProtocolError>& other)
      : error_(other.host_error()) {}

  // Evaluates to true if and only if both Errors hold the same class of error
  // (host vs protocol) and their codes match in value. Errors with different
  // ProtocolErrorCodes are comparable only if their ProtocolErrorCodes have a
  // defined operator==, which is generally not the case if the codes are
  // strongly-type "enum class" enumerations.
  template <typename RErrorCode>
  constexpr bool operator==(const Error<RErrorCode>& rhs) const {
    auto proto_error_visitor = [&](ProtocolErrorCode held) {
      if constexpr (may_hold_protocol_error() &&
                    Error<RErrorCode>::may_hold_protocol_error()) {
        return held == rhs.protocol_error();
      } else {
        // This unreachable branch makes comparisons to Error<NoProtocolError>
        // well-defined, so that the lambda compiles as long as the protocol
        // error codes are comparable.
        return false;
      }
    };
    if (is_host_error() != rhs.is_host_error()) {
      return false;
    }
    return Visit([&rhs](HostError held) { return held == rhs.host_error(); },
                 proto_error_visitor);
  }

  template <typename RErrorCode>
  constexpr bool operator!=(const Error<RErrorCode>& rhs) const {
    return !(*this == rhs);
  }

  [[nodiscard]] std::string ToString() const {
    return Visit(
        [](HostError held) { return HostErrorToString(held); },
        [](ProtocolErrorCode held) {
          return ProtocolErrorTraits<ProtocolErrorCode>::ToString(held);
        });
  }

  [[nodiscard]] constexpr bool is_host_error() const {
    return std::holds_alternative<HostError>(error_);
  }

  [[nodiscard]] constexpr bool is_protocol_error() const {
    return std::holds_alternative<ProtocolErrorCode>(error_);
  }

  [[nodiscard]] constexpr HostError host_error() const {
    BT_ASSERT_MSG(is_host_error(), "Does not hold HostError");
    return std::get<HostError>(error_);
  }

  [[nodiscard]] constexpr ProtocolErrorCode protocol_error() const {
    BT_ASSERT_MSG(is_protocol_error(), "Does not hold protocol error");
    return std::get<ProtocolErrorCode>(error_);
  }

  [[nodiscard]] constexpr bool is(ProtocolErrorCode proto_error) const {
    return Visit(
        [](HostError) { return false; },
        [proto_error](ProtocolErrorCode held) { return held == proto_error; });
  }

  [[nodiscard]] constexpr bool is(HostError host_error) const {
    return Visit([host_error](HostError held) { return held == host_error; },
                 [](ProtocolErrorCode) { return false; });
  }

  template <typename... Ts>
  [[nodiscard]] constexpr bool is_any_of(Ts... error_codes) const {
    return (is(error_codes) || ...);
  }

  // Given two "visitors" (callable objects that accept HostError and
  // ProtocolErrorCode), invoke the one that corresponds to the error held in
  // storage, but not the other. This pattern allows the code within the
  // visitors to statically presume the type of the error code that they work
  // with. Example:
  //
  //   int ConvertToInt(Error<FooError> error) {
  //     return Visit(
  //         [](HostError held) { return static_cast<int>(held); },
  //         [](ProtocolErrorCode held) { return static_cast<int>(held); });
  //     );
  //   }
  //
  // Unlike std::visit, the two visitors do not need to be differentiated from
  // each other through overload resolution rules: the argument order to
  // invoking Visit(…) is what determines which visitor gets called.
  //
  // Returns the return value of the visitor that was called (which may return
  // void).
  template <typename HostVisitor, typename ProtoVisitor>
  [[nodiscard]] constexpr std::common_type_t<
      std::invoke_result_t<HostVisitor, HostError>,
      std::invoke_result_t<ProtoVisitor, ProtocolErrorCode>>
  Visit(HostVisitor host_error_visitor,
        ProtoVisitor proto_error_visitor) const {
    if (is_host_error()) {
      return host_error_visitor(host_error());
    }
    return proto_error_visitor(protocol_error());
  }

  static constexpr bool may_hold_protocol_error() {
    return !std::is_same_v<ProtocolErrorCode, NoProtocolError>;
  }

 private:
  // Factory functions
  friend constexpr fit::result<Error<ProtocolErrorCode>>
      ToResult<ProtocolErrorCode>(ProtocolErrorCode);

  template <typename T = ProtocolErrorCode,
            std::enable_if_t<detail::CanRepresentSuccessV<T>, int> = 0>
  constexpr explicit Error(const ProtocolErrorCode& proto_error)
      : error_(proto_error) {
    BT_ASSERT(!ProtocolErrorTraits<ProtocolErrorCode>::is_success(proto_error));
  }

  std::variant<HostError, ProtocolErrorCode> error_;
};

// Deduction guide to allow Errors to be constructed from a HostError without
// specifying what protocol error the Error can hold instead.
Error(HostError) -> Error<NoProtocolError>;

// Comparison operators overloads useful for testing using
// {ASSERT,EXPECT}_{EQ,NE} GoogleTest macros. Each of these must explicitly
// define a operator!= as well as account for commutative calls, because C++
// does not automatically generate these. Those variant overloads can not be
// generically defined because there's no way to test if those variants can
// actually be instantiated (using decltype etc), causing problems with e.g.
// fit::result<E, T> == fit::result<F, U>.

// Comparisons to fit::result<Error<ProtocolErrorCode>>
template <typename LErrorCode, typename RErrorCode, typename... Ts>
constexpr bool operator==(const Error<LErrorCode>& lhs,
                          const fit::result<Error<RErrorCode>, Ts...>& rhs) {
  static_assert((!detail::IsErrorV<Ts> && ...),
                "fit::result should not contain Error as a success value");
  return rhs.is_error() && (rhs.error_value() == lhs);
}

template <typename LErrorCode, typename RErrorCode, typename... Ts>
constexpr bool operator==(const fit::result<Error<LErrorCode>, Ts...>& lhs,
                          const Error<RErrorCode>& rhs) {
  return rhs == lhs;
}

template <typename LErrorCode, typename RErrorCode, typename... Ts>
constexpr bool operator!=(const Error<LErrorCode>& lhs,
                          const fit::result<Error<RErrorCode>, Ts...>& rhs) {
  return !(lhs == rhs);
}

template <typename LErrorCode, typename RErrorCode, typename... Ts>
constexpr bool operator!=(const fit::result<Error<LErrorCode>, Ts...>& lhs,
                          const Error<RErrorCode>& rhs) {
  return !(rhs == lhs);
}

// Comparisons between fit::result<Error<…>> objects
// Note that this is not standard fit::result relation behavior which normally
// compares all error results to be equal. These are preferred in overload
// resolution because they are more specialized templates than the ones provided
// by fit. However, because they are more specialized, all of the combinations
// must be typed out separately to avoid ambiguous overload errors:
//   1. operands having zero or one success values
//   2. operation is == or !=
// The case of comparing a result with a success value to a result without is
// intentionally not defined because it's not obvious what behavior it should
// have when both results hold success.
template <typename LErrorCode, typename RErrorCode, typename T>
constexpr bool operator==(const fit::result<Error<LErrorCode>, T>& lhs,
                          const fit::result<Error<RErrorCode>, T>& rhs) {
  static_assert(!detail::IsErrorV<T>,
                "fit::result should not contain Error as a success value");
  if (lhs.is_ok() != rhs.is_ok()) {
    return false;
  }
  if (lhs.is_ok()) {
    return lhs.value() == rhs.value();
  }
  return lhs.error_value() == rhs.error_value();
}

template <typename LErrorCode, typename RErrorCode, typename T>
constexpr bool operator!=(const fit::result<Error<LErrorCode>, T>& lhs,
                          const fit::result<Error<RErrorCode>, T>& rhs) {
  return !(lhs == rhs);
}

template <typename LErrorCode, typename RErrorCode>
constexpr bool operator==(const fit::result<Error<LErrorCode>>& lhs,
                          const fit::result<Error<RErrorCode>>& rhs) {
  if (lhs.is_ok() != rhs.is_ok()) {
    return false;
  }
  if (lhs.is_ok()) {
    return true;
  }
  return lhs.error_value() == rhs.error_value();
}

template <typename LErrorCode, typename RErrorCode>
constexpr bool operator!=(const fit::result<Error<LErrorCode>>& lhs,
                          const fit::result<Error<RErrorCode>>& rhs) {
  return !(lhs == rhs);
}

namespace internal {

// Helper to build a string from result using generic concatenation calls.
template <typename Result, typename StringBuilder, typename ValueStringBuilder>
void BuildResultToString(const Result& result,
                         StringBuilder builder,
                         ValueStringBuilder append_value) {
  builder("[result: ");
  if (result.is_ok()) {
    builder("ok(");
    append_value();
  } else {
    builder("error(");
    builder(result.error_value().ToString());
  }
  builder(")]");
}

// Produces a human-readable representation of a fit::result<Error<…>>
template <typename ProtocolErrorCode, typename... Ts>
std::string ToString(
    const fit::result<Error<ProtocolErrorCode>, Ts...>& result) {
  std::string out;
  auto append_value_string = [&] {
    if constexpr (sizeof...(Ts) > 0) {
      if constexpr ((bt::internal::HasToStringV<Ts> && ...)) {
        out += ToString(result.value());
      } else {
        // It's not possible to portably print e.g. the name of the value's
        // type, so fall back to a placeholder. It may be useful to print the
        // size and a hexdump, however.
        out += "?";
      }
    }
  };
  bt::internal::BuildResultToString(
      result, [&](auto s) { out += s; }, append_value_string);
  return out;
}

}  // namespace internal

namespace detail {

// Contains a |value| bool member that is true if overload operator<<(Lhs&,
// const Rhs&) exists
template <typename Lhs, typename Rhs, typename = void>
struct IsStreamable : std::false_type {};

template <typename Lhs, typename Rhs>
struct IsStreamable<
    Lhs,
    Rhs,
    std::void_t<decltype(std::declval<Lhs&>() << std::declval<const Rhs&>())>>
    : std::is_same<Lhs&,
                   decltype(std::declval<Lhs&>()
                            << std::declval<const Rhs&>())> {};

template <typename Lhs, typename Rhs>
constexpr bool IsStreamableV = IsStreamable<Lhs, Rhs>::value;

}  // namespace detail
}  // namespace bt

// Extends the GoogleTest value printer to print fit::result<bt::Error<…>, …>
// types, including converting the contained value of "success" results to
// strings when possible.
//
// This must be defined in namespace fit because GoogleTest uses
// argument-dependent lookup (ADL) to find this overload. |os|'s type is
// templated in order to avoid including <iostream>.
namespace fit {

// Some GoogleTest internal objects (like testing::Message, the return type of
// ADD_FAILURE()) declare broad operator<< overloads that conflict with this
// one. In those cases, it's likely easiest to wrap the result in bt_str(…).
template <typename OStream, typename ProtocolErrorCode, typename... Ts>
OStream& operator<<(
    OStream& os,
    const fit::result<::bt::Error<ProtocolErrorCode>, Ts...>& result) {
  auto stream_value_string = [&] {
    if constexpr (sizeof...(Ts) > 0) {
      if constexpr ((::bt::internal::HasToStringV<Ts> && ...)) {
        os << ::bt::internal::ToString(result.value());
      } else if constexpr ((::bt::detail::IsStreamableV<OStream, Ts> && ...)) {
        os << result.value();
      } else {
        // It may be prettier to default to ::testing::PrintToString here but
        // that would require including <gtest/gtest.h> here, which is not
        // ideal.
        os << "?";
      }
    }
  };
  ::bt::internal::BuildResultToString(
      result, [&](auto s) { os << s; }, stream_value_string);
  return os;
}

}  // namespace fit

// Macro to check and log any non-Success status of an event.
// Use these like:
// if (bt_is_error(result, WARN, "gap", "failed to set event mask")) {
//   ...
//   return;
// }
//
// It will log with the string prepended to the stringified result if result is
// a failure. Evaluates to true if the result indicates failure.
#define bt_is_error(result, level, tag, fmt, args...)          \
  ({                                                           \
    auto _result = result;                                     \
    if (_result.is_error())                                    \
      bt_log(level, tag, "%s: " fmt, bt_str(_result), ##args); \
    _result.is_error();                                        \
  })

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ERROR_H_
