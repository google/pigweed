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
// pw_Status uses the canonical Google error codes. The following was copied
// from Tensorflow and prefixed with PW_STATUS_.
typedef enum {
  PW_STATUS_OK = 0,  // Use Status::OK in C++

  // The operation was cancelled (typically by the caller).
  PW_STATUS_CANCELLED = 1,  // Use Status::CANCELLED in C++

  // Unknown error.  An example of where this error may be returned is
  // if a Status value received from another address space belongs to
  // an error-space that is not known in this address space.  Also,
  // errors raised by APIs that do not return enough error information
  // may be converted to this error.
  PW_STATUS_UNKNOWN = 2,  // Use Status::UNKNOWN in C++

  // Client specified an invalid argument.  Note that this differs
  // from FAILED_PRECONDITION.  INVALID_ARGUMENT indicates arguments
  // that are problematic regardless of the state of the system
  // (e.g. a malformed file name).
  PW_STATUS_INVALID_ARGUMENT = 3,  // Use Status::INVALID_ARGUMENT in C++

  // Deadline expired before operation could complete.  For operations
  // that change the state of the system, this error may be returned
  // even if the operation has completed successfully.  For example, a
  // successful response from a server could have been delayed long
  // enough for the deadline to expire.
  PW_STATUS_DEADLINE_EXCEEDED = 4,  // Use Status::DEADLINE_EXCEEDED in C++

  // Some requested entity (e.g. file or directory) was not found.
  // For privacy reasons, this code *may* be returned when the client
  // does not have the access right to the entity.
  PW_STATUS_NOT_FOUND = 5,  // Use Status::NOT_FOUND in C++

  // Some entity that we attempted to create (e.g. file or directory)
  // already exists.
  PW_STATUS_ALREADY_EXISTS = 6,  // Use Status::ALREADY_EXISTS in C++

  // The caller does not have permission to execute the specified
  // operation.  PERMISSION_DENIED must not be used for rejections
  // caused by exhausting some resource (use RESOURCE_EXHAUSTED
  // instead for those errors).  PERMISSION_DENIED must not be
  // used if the caller cannot be identified (use UNAUTHENTICATED
  // instead for those errors).
  PW_STATUS_PERMISSION_DENIED = 7,  // Use Status::PERMISSION_DENIED in C++

  // The request does not have valid authentication credentials for the
  // operation.
  PW_STATUS_UNAUTHENTICATED = 16,  // Use Status::UNAUTHENTICATED in C++

  // Some resource has been exhausted, perhaps a per-user quota, or
  // perhaps the entire filesystem is out of space.
  PW_STATUS_RESOURCE_EXHAUSTED = 8,  // Use Status::RESOURCE_EXHAUSTED in C++

  // Operation was rejected because the system is not in a state
  // required for the operation's execution.  For example, directory
  // to be deleted may be non-empty, an rmdir operation is applied to
  // a non-directory, etc.
  //
  // A litmus test that may help a service implementer in deciding
  // between FAILED_PRECONDITION, ABORTED, and UNAVAILABLE:
  //  (a) Use UNAVAILABLE if the client can retry just the failing call.
  //  (b) Use ABORTED if the client should retry at a higher-level
  //      (e.g. restarting a read-modify-write sequence).
  //  (c) Use FAILED_PRECONDITION if the client should not retry until
  //      the system state has been explicitly fixed.  E.g. if an "rmdir"
  //      fails because the directory is non-empty, FAILED_PRECONDITION
  //      should be returned since the client should not retry unless
  //      they have first fixed up the directory by deleting files from it.
  //  (d) Use FAILED_PRECONDITION if the client performs conditional
  //      REST Get/Update/Delete on a resource and the resource on the
  //      server does not match the condition. E.g. conflicting
  //      read-modify-write on the same resource.
  PW_STATUS_FAILED_PRECONDITION = 9,  // Use Status::FAILED_PRECONDITION in C++

  // The operation was aborted, typically due to a concurrency issue
  // like sequencer check failures, transaction aborts, etc.
  //
  // See litmus test above for deciding between FAILED_PRECONDITION,
  // ABORTED, and UNAVAILABLE.
  PW_STATUS_ABORTED = 10,  // Use Status::ABORTED in C++

  // Operation tried to iterate past the valid input range.  E.g. seeking or
  // reading past end of file.
  //
  // Unlike INVALID_ARGUMENT, this error indicates a problem that may
  // be fixed if the system state changes. For example, a 32-bit file
  // system will generate INVALID_ARGUMENT if asked to read at an
  // offset that is not in the range [0,2^32-1], but it will generate
  // OUT_OF_RANGE if asked to read from an offset past the current
  // file size.
  //
  // There is a fair bit of overlap between FAILED_PRECONDITION and
  // OUT_OF_RANGE.  We recommend using OUT_OF_RANGE (the more specific
  // error) when it applies so that callers who are iterating through
  // a space can easily look for an OUT_OF_RANGE error to detect when
  // they are done.
  PW_STATUS_OUT_OF_RANGE = 11,  // Use Status::OUT_OF_RANGE in C++

  // Operation is not implemented or not supported/enabled in this service.
  PW_STATUS_UNIMPLEMENTED = 12,  // Use Status::UNIMPLEMENTED in C++

  // Internal errors.  Means some invariants expected by underlying
  // system has been broken.  If you see one of these errors,
  // something is very broken.
  PW_STATUS_INTERNAL = 13,  // Use Status::INTERNAL in C++

  // The service is currently unavailable.  This is a most likely a
  // transient condition and may be corrected by retrying with
  // a backoff.
  //
  // See litmus test above for deciding between FAILED_PRECONDITION,
  // ABORTED, and UNAVAILABLE.
  PW_STATUS_UNAVAILABLE = 14,  // Use Status::UNAVAILABLE in C++

  // Unrecoverable data loss or corruption.
  PW_STATUS_DATA_LOSS = 15,  // Use Status::DATA_LOSS in C++

  // An extra enum entry to prevent people from writing code that
  // fails to compile when a new code is added.
  //
  // Nobody should ever reference this enumeration entry. In particular,
  // if you write C++ code that switches on this enumeration, add a default:
  // case instead of a case that mentions this enumeration entry.
  //
  // Nobody should rely on the value listed here. It may change in the future.
  PW_STATUS_DO_NOT_USE_RESERVED_FOR_FUTURE_EXPANSION_USE_DEFAULT_IN_SWITCH_INSTEAD_,
} pw_Status;  // Use pw::Status in C++

// Returns a null-terminated string representation of the pw_Status.
const char* pw_StatusString(pw_Status status);

#ifdef __cplusplus

}  // extern "C"

namespace pw {

// The Status class is a thin, zero-cost abstraction around the pw_Status enum.
// It initializes to Status::OK by default and adds ok() and str() methods.
// Implicit conversions are permitted between pw_Status and pw::Status.
class Status {
 public:
  using Code = pw_Status;

  // All of the pw_Status codes are available in the Status class as, e.g.
  // pw::Status::OK or pw::Status::OUT_OF_RANGE.
  static constexpr Code OK = PW_STATUS_OK;
  static constexpr Code CANCELLED = PW_STATUS_CANCELLED;
  static constexpr Code UNKNOWN = PW_STATUS_UNKNOWN;
  static constexpr Code INVALID_ARGUMENT = PW_STATUS_INVALID_ARGUMENT;
  static constexpr Code DEADLINE_EXCEEDED = PW_STATUS_DEADLINE_EXCEEDED;
  static constexpr Code NOT_FOUND = PW_STATUS_NOT_FOUND;
  static constexpr Code ALREADY_EXISTS = PW_STATUS_ALREADY_EXISTS;
  static constexpr Code PERMISSION_DENIED = PW_STATUS_PERMISSION_DENIED;
  static constexpr Code UNAUTHENTICATED = PW_STATUS_UNAUTHENTICATED;
  static constexpr Code RESOURCE_EXHAUSTED = PW_STATUS_RESOURCE_EXHAUSTED;
  static constexpr Code FAILED_PRECONDITION = PW_STATUS_FAILED_PRECONDITION;
  static constexpr Code ABORTED = PW_STATUS_ABORTED;
  static constexpr Code OUT_OF_RANGE = PW_STATUS_OUT_OF_RANGE;
  static constexpr Code UNIMPLEMENTED = PW_STATUS_UNIMPLEMENTED;
  static constexpr Code INTERNAL = PW_STATUS_INTERNAL;
  static constexpr Code UNAVAILABLE = PW_STATUS_UNAVAILABLE;
  static constexpr Code DATA_LOSS = PW_STATUS_DATA_LOSS;

  // Statuses are created with a Status::Code.
  constexpr Status(Code code = OK) : code_(code) {}

  constexpr Status(const Status&) = default;
  constexpr Status& operator=(const Status&) = default;

  // Status implicitly converts to a Status::Code.
  constexpr operator Code() const { return code_; }

  // True if the status is Status::OK.
  constexpr bool ok() const { return code_ == OK; }

  // Returns a null-terminated string representation of the Status.
  const char* str() const { return pw_StatusString(code_); }

 private:
  Code code_;
};

}  // namespace pw

#endif  // __cplusplus
