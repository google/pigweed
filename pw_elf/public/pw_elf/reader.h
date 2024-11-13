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

#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "pw_bytes/span.h"
#include "pw_elf/internal/reader_impl.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_stream/stream.h"

namespace pw::elf {

/// A basic reader for ELF files.
class ElfReader {
 public:
  /// Creates an ElfReader from a stream.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The reader was initialized successfully.
  ///
  ///    DATA_LOSS: The input file was invalid.
  ///
  ///    OUT_OF_RANGE: Input stream exhausted (EOF).
  ///
  ///    UNIMPLEMENTED: Some aspect of the ELF file is not (yet) supported by
  ///    this class, e.g., non-native endianness, or 64-bit ELF on a 32-bit
  ///    host.
  ///
  /// May return other error codes from the underlying stream.
  ///
  /// @endrst
  static Result<ElfReader> FromStream(stream::SeekableReader& stream);

  /// Gets the associated stream.
  stream::SeekableReader& stream() const {
    return std::visit([](auto&& impl) -> auto& { return impl.stream(); },
                      impl_);
  }

  /// Seeks the associated stream to the beginning of the data of the section
  /// with the given name.
  ///
  /// @param[in] name The name of the desired section.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: Successfully found the desired section and seeked the stream to
  ///    it. The associated size is the size of the associated section.
  ///
  ///    NOT_FOUND: No section was found with the desired name.
  ///
  /// May return other error codes from the underlying stream.
  ///
  /// @endrst
  StatusWithSize SeekToSection(std::string_view name) {
    return std::visit([name](auto&& impl) { return impl.SeekToSection(name); },
                      impl_);
  }

  /// Reads a section with the given name.
  ///
  /// @param[in] name The name of the desired section.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: Successfully read the desired section.
  ///    The result value is a vector of the section data.
  ///
  ///    NOT_FOUND: No section was found with the desired name.
  ///
  /// May return other error codes from the underlying stream.
  ///
  /// @endrst
  Result<std::vector<std::byte>> ReadSection(std::string_view name);

 private:
  internal::ElfReaderImpls impl_;

  ElfReader(internal::ElfReaderImpls&& impl) : impl_(std::move(impl)) {}
};

}  // namespace pw::elf
