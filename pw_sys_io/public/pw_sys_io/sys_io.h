// Copyright 2019 The Pigweed Authors
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

// This module defines a simple and unoptimized interface for byte-by-byte
// input/output. This can be done over a logging system, stdio, UART, via a
// photodiode and modulated kazoo, or basically any way to get data in and out
// of an application.
//
// This facade doesn't dictate any policies on input and output data encoding,
// format, or transmission protocol. It only requires that backends return a
// OkStatus() if the operation succeeds. Backends may provide useful error
// Status types, but depending on the implementation-specific Status values is
// NOT recommended. Since this facade provides a very vague I/O interface, it
// does NOT provide tests. Backends are expected to provide their own testing to
// validate correctness.
//
// The intent of this module for simplifying bringup or otherwise getting data
// in/out of a CPU in a way that is platform-agnostic. The interface is designed
// to be easy to understand. There's no initialization as part of this
// interface, there's no configuration, and the interface is no-frills WYSIWYG
// byte-by-byte i/o.
//
//
//          PLEASE DON'T BUILD PROJECTS ON TOP OF THIS INTERFACE.

#include <cstddef>
#include <cstring>
#include <string_view>

#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

/// Unoptimized I/O library
namespace pw::sys_io {

/// Reads a single byte from the `pw_sys_io` backend.
/// This function blocks until it either succeeds or fails to read a
/// byte.
///
/// @pre This function must be implemented by the `pw_sys_io` backend.
///
/// @warning Do not build production projects on top of `pw_sys_io`.
///
/// @returns @rst
///
/// .. pw-status-codes::
///
///    OK: A byte was successfully read and is in ``dest``.
///
///    RESOURCE_EXHAUSTED: The underlying source vanished.
///
/// @endrst
Status ReadByte(std::byte* dest);

/// Reads a single byte from the `pw_sys_io` backend, if available.
///
/// @pre This function must be implemented by the `pw_sys_io` backend.
///
/// @warning Do not build production projects on top of `pw_sys_io`.
///
/// @returns @rst
///
/// .. pw-status-codes::
///
///    OK: A byte was successfully read and is in ``dest``.
///
///    UNAVAILABLE: No byte is available to read; try later.
///
///    UNIMPLEMENTED: The function is not supported on this target.
///
/// @endrst
Status TryReadByte(std::byte* dest);

/// Writes a single byte out the `pw_sys_io` backend. The function blocks until
/// it either succeeds or fails to write the byte.
///
/// @pre This function must be implemented by the `pw_sys_io` backend.
///
/// @warning Do not build production projects on top of `pw_sys_io`.
///
/// @returns @rst
///
/// .. pw-status-codes::
///
///    OK: A byte was successfully written.
///
/// @endrst
Status WriteByte(std::byte b);

/// Writes a string out the `pw_sys_io` backend.
///
/// This function takes a `string_view` and writes it out the `pw_sys_io`
/// backend, adding any platform-specific newline character(s) (these are
/// accounted for in the returned `StatusWithSize`).
///
/// @pre This function must be implemented by the `pw_sys_io` backend.
///
/// @warning Do not build production projects on top of `pw_sys_io`.
///
/// @returns @rst
///
/// .. pw-status-codes::
///
///    OK: All the bytes from the source string were successfully
///    written.
///
/// In all cases, the number of bytes successfully written are returned as
/// part of the ``StatusWithSize``.
///
/// @endrst
StatusWithSize WriteLine(std::string_view s);

/// Fills a byte span from the `pw_sys_io` backend using `ReadByte()`.
///
/// This function is implemented by the facade and simply uses `ReadByte()` to
/// read enough bytes to fill the destination span. If there's an error reading
/// a byte, the read is aborted and the contents of the destination span are
/// undefined. This function blocks until either an error occurs or all bytes
/// are successfully read from the backend's `ReadByte()` implementation.
///
/// @returns @rst
///
/// .. pw-status-codes::
///
///    OK: The destination span was successfully filled.
///
/// In all cases, the number of bytes successuflly read to the destination
/// span are returned as part of the ``StatusWithSize``.
///
/// @endrst
StatusWithSize ReadBytes(ByteSpan dest);

/// Writes a span of bytes out the `pw_sys_io` backend using `WriteByte()`.
///
/// This function is implemented by the facade and simply writes the source
/// contents using `WriteByte()`. If an error writing a byte is encountered, the
/// write is aborted and the error status is returned. This function blocks
/// until either an error occurs, or all bytes are successfully written from the
/// backend's `WriteByte()` implementation.
///
/// @returns @rst
///
/// .. pw-status-codes::
///
///    OK: All the bytes from the source span were successfully written.
///
/// In all cases, the number of bytes successfully written are returned as
/// part of the ``StatusWithSize``.
///
/// @endrst
StatusWithSize WriteBytes(ConstByteSpan src);

}  // namespace pw::sys_io
