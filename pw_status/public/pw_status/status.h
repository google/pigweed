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

#include "pw_status/internal/config.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// This is the pw_Status enum. pw_Status is used to return the status from an
// operation.
//
// In C++, use the pw::Status class instead of the pw_Status enum. pw_Status and
// Status implicitly convert to one another and can be passed cleanly between C
// and C++ APIs.
//
// pw_Status uses the canonical Google error codes. The following enum was based
// on Abseil's status/status.h. The values are all-caps and prefixed with
// PW_STATUS_ instead of using C++ constant style.
//
// The status codes are described at https://pigweed.dev/pw_status#status-codes.
// Consult that guide when deciding which status code to use.
typedef enum {
  PW_STATUS_OK = 0,                   // Use OkStatus() in C++
  PW_STATUS_CANCELLED = 1,            // Use Status::Cancelled() in C++
  PW_STATUS_UNKNOWN = 2,              // Use Status::Unknown() in C++
  PW_STATUS_INVALID_ARGUMENT = 3,     // Use Status::InvalidArgument() in C++
  PW_STATUS_DEADLINE_EXCEEDED = 4,    // Use Status::DeadlineExceeded() in C++
  PW_STATUS_NOT_FOUND = 5,            // Use Status::NotFound() in C++
  PW_STATUS_ALREADY_EXISTS = 6,       // Use Status::AlreadyExists() in C++
  PW_STATUS_PERMISSION_DENIED = 7,    // Use Status::PermissionDenied() in C++
  PW_STATUS_RESOURCE_EXHAUSTED = 8,   // Use Status::ResourceExhausted() in C++
  PW_STATUS_FAILED_PRECONDITION = 9,  // Use Status::FailedPrecondition() in C++
  PW_STATUS_ABORTED = 10,             // Use Status::Aborted() in C++
  PW_STATUS_OUT_OF_RANGE = 11,        // Use Status::OutOfRange() in C++
  PW_STATUS_UNIMPLEMENTED = 12,       // Use Status::Unimplemented() in C++
  PW_STATUS_INTERNAL = 13,            // Use Status::Internal() in C++
  PW_STATUS_UNAVAILABLE = 14,         // Use Status::Unavailable() in C++
  PW_STATUS_DATA_LOSS = 15,           // Use Status::DataLoss() in C++
  PW_STATUS_UNAUTHENTICATED = 16,     // Use Status::Unauthenticated() in C++

  // NOTE: this error code entry should not be used and you should not rely on
  // its value, which may change.
  //
  // The purpose of this enumerated value is to force people who handle status
  // codes with `switch()` statements to *not* simply enumerate all possible
  // values, but instead provide a "default:" case. Providing such a default
  // case ensures that code will compile when new codes are added.
  PW_STATUS_DO_NOT_USE_RESERVED_FOR_FUTURE_EXPANSION_USE_DEFAULT_IN_SWITCH_INSTEAD_,
} pw_Status;  // Use pw::Status in C++

// Returns a null-terminated string representation of the pw_Status.
const char* pw_StatusString(pw_Status status);

// Indicates the status code with the highest valid value.
#define PW_STATUS_LAST PW_STATUS_UNAUTHENTICATED

#ifdef __cplusplus

}  // extern "C"

namespace pw {

/// `Status` is a thin, zero-cost abstraction around the `pw_Status` enum. It
/// initializes to @pw_status{OK} by default and adds `ok()` and `str()`
/// methods. Implicit conversions are permitted between `pw_Status` and
/// `pw::Status`.
///
/// An @pw_status{OK} `Status` is created by the @cpp_func{pw::OkStatus}
/// function or by the default `Status` constructor.  Non-OK `Status` is created
/// with a static member function that corresponds with the status code.
class _PW_STATUS_NO_DISCARD Status {
 public:
  using Code = pw_Status;

  // Functions that create a Status with the specified code.
  //
  // The status codes are described at
  // https://pigweed.dev/pw_status#status-codes. Consult that guide when
  // deciding which status code to use.
  // clang-format off
  [[nodiscard]] static constexpr Status Cancelled() {
    return PW_STATUS_CANCELLED;
  }
  [[nodiscard]] static constexpr Status Unknown() {
    return PW_STATUS_UNKNOWN;
  }
  [[nodiscard]] static constexpr Status InvalidArgument() {
    return PW_STATUS_INVALID_ARGUMENT;
  }
  [[nodiscard]] static constexpr Status DeadlineExceeded() {
    return PW_STATUS_DEADLINE_EXCEEDED;
  }
  [[nodiscard]] static constexpr Status NotFound() {
    return PW_STATUS_NOT_FOUND;
  }
  [[nodiscard]] static constexpr Status AlreadyExists() {
    return PW_STATUS_ALREADY_EXISTS;
  }
  [[nodiscard]] static constexpr Status PermissionDenied() {
    return PW_STATUS_PERMISSION_DENIED;
  }
  [[nodiscard]] static constexpr Status ResourceExhausted() {
    return PW_STATUS_RESOURCE_EXHAUSTED;
  }
  [[nodiscard]] static constexpr Status FailedPrecondition() {
    return PW_STATUS_FAILED_PRECONDITION;
  }
  [[nodiscard]] static constexpr Status Aborted() {
    return PW_STATUS_ABORTED;
  }
  [[nodiscard]] static constexpr Status OutOfRange() {
    return PW_STATUS_OUT_OF_RANGE;
  }
  [[nodiscard]] static constexpr Status Unimplemented() {
    return PW_STATUS_UNIMPLEMENTED;
  }
  [[nodiscard]] static constexpr Status Internal() {
    return PW_STATUS_INTERNAL;
  }
  [[nodiscard]] static constexpr Status Unavailable() {
    return PW_STATUS_UNAVAILABLE;
  }
  [[nodiscard]] static constexpr Status DataLoss() {
    return PW_STATUS_DATA_LOSS;
  }
  [[nodiscard]] static constexpr Status Unauthenticated() {
    return PW_STATUS_UNAUTHENTICATED;
  }
  // clang-format on

  // Statuses are created with a Status::Code.
  constexpr Status(Code code = PW_STATUS_OK) : code_(code) {}

  constexpr Status(const Status&) = default;
  constexpr Status& operator=(const Status&) = default;

  /// Returns the `Status::Code` (`pw_Status`) for this `Status`.
  constexpr Code code() const { return code_; }

  /// True if the status is @pw_status{OK}.
  ///
  /// This function is provided in place of an `IsOk()` function.
  [[nodiscard]] constexpr bool ok() const { return code_ == PW_STATUS_OK; }

  // Functions for checking which status this is.
  [[nodiscard]] constexpr bool IsCancelled() const {
    return code_ == PW_STATUS_CANCELLED;
  }
  [[nodiscard]] constexpr bool IsUnknown() const {
    return code_ == PW_STATUS_UNKNOWN;
  }
  [[nodiscard]] constexpr bool IsInvalidArgument() const {
    return code_ == PW_STATUS_INVALID_ARGUMENT;
  }
  [[nodiscard]] constexpr bool IsDeadlineExceeded() const {
    return code_ == PW_STATUS_DEADLINE_EXCEEDED;
  }
  [[nodiscard]] constexpr bool IsNotFound() const {
    return code_ == PW_STATUS_NOT_FOUND;
  }
  [[nodiscard]] constexpr bool IsAlreadyExists() const {
    return code_ == PW_STATUS_ALREADY_EXISTS;
  }
  [[nodiscard]] constexpr bool IsPermissionDenied() const {
    return code_ == PW_STATUS_PERMISSION_DENIED;
  }
  [[nodiscard]] constexpr bool IsResourceExhausted() const {
    return code_ == PW_STATUS_RESOURCE_EXHAUSTED;
  }
  [[nodiscard]] constexpr bool IsFailedPrecondition() const {
    return code_ == PW_STATUS_FAILED_PRECONDITION;
  }
  [[nodiscard]] constexpr bool IsAborted() const {
    return code_ == PW_STATUS_ABORTED;
  }
  [[nodiscard]] constexpr bool IsOutOfRange() const {
    return code_ == PW_STATUS_OUT_OF_RANGE;
  }
  [[nodiscard]] constexpr bool IsUnimplemented() const {
    return code_ == PW_STATUS_UNIMPLEMENTED;
  }
  [[nodiscard]] constexpr bool IsInternal() const {
    return code_ == PW_STATUS_INTERNAL;
  }
  [[nodiscard]] constexpr bool IsUnavailable() const {
    return code_ == PW_STATUS_UNAVAILABLE;
  }
  [[nodiscard]] constexpr bool IsDataLoss() const {
    return code_ == PW_STATUS_DATA_LOSS;
  }
  [[nodiscard]] constexpr bool IsUnauthenticated() const {
    return code_ == PW_STATUS_UNAUTHENTICATED;
  }

  /// Updates this `Status` to the provided `Status` IF this status is
  /// @pw_status{OK}. This is useful for tracking the first encountered error,
  /// as calls to this helper will not change one error status to another error
  /// status.
  constexpr void Update(Status other) {
    if (ok()) {
      code_ = other.code();
    }
  }

  /// Ignores any errors. This method does nothing except potentially suppress
  /// complaints from any tools that are checking that errors are not dropped on
  /// the floor.
  constexpr void IgnoreError() const {}

  /// Returns a null-terminated string representation of the `Status`.
  [[nodiscard]] const char* str() const { return pw_StatusString(code_); }

 private:
  Code code_;
};

/// Returns an @pw_status{OK} status. Equivalent to `Status()` or
/// `Status(PW_STATUS_OK)`.  This function is used instead of a `Status::Ok()`
/// function, which would be too similar to `Status::ok()`.
[[nodiscard]] constexpr Status OkStatus() { return Status(); }

constexpr bool operator==(const Status& lhs, const Status& rhs) {
  return lhs.code() == rhs.code();
}

constexpr bool operator!=(const Status& lhs, const Status& rhs) {
  return lhs.code() != rhs.code();
}

}  // namespace pw

// Create a C++ overload of pw_StatusString so that it supports pw::Status in
// addition to pw_Status.
inline const char* pw_StatusString(pw::Status status) {
  return pw_StatusString(status.code());
}

#endif  // __cplusplus
