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
#pragma once

#include "pw_bytes/span.h"
#include "pw_protobuf/decoder.h"
#include "pw_protobuf/stream_decoder.h"
#include "pw_result/result.h"
#include "pw_status/try.h"
#include "pw_string/string.h"

namespace pw::protobuf {

namespace internal {

Status AdvanceToField(Decoder& decoder, uint32_t field_number);
Status AdvanceToField(StreamDecoder& decoder, uint32_t field_number);

}  // namespace internal

/// @submodule{pw_protobuf,find}

template <typename T, auto kReadFn>
class Finder {
 public:
  constexpr Finder(ConstByteSpan message, uint32_t field_number)
      : decoder_(message), field_number_(field_number) {}

  Result<T> Next() {
    T output;
    PW_TRY(internal::AdvanceToField(decoder_, field_number_));
    PW_TRY((decoder_.*kReadFn)(&output));
    return output;
  }

 private:
  Decoder decoder_;
  uint32_t field_number_;
};

template <typename T, auto kReadFn>
class StreamFinder {
 public:
  constexpr StreamFinder(stream::Reader& reader, uint32_t field_number)
      : decoder_(reader), field_number_(field_number) {}

  Result<T> Next() {
    PW_TRY(internal::AdvanceToField(decoder_, field_number_));
    Result<T> result = (decoder_.*kReadFn)();

    // The StreamDecoder returns a NOT_FOUND if trying to read the wrong type
    // for a field. Remap this to FAILED_PRECONDITION for consistency with the
    // non-stream Find.
    return result.status().IsNotFound()
               ? Result<T>(Status::FailedPrecondition())
               : result;
  }

 private:
  StreamDecoder decoder_;
  uint32_t field_number_;
};

template <typename T>
class EnumFinder : private Finder<uint32_t, &Decoder::ReadUint32> {
 public:
  using Finder::Finder;

  Result<T> Next() {
    Result<uint32_t> result = Finder::Next();
    if (!result.ok()) {
      return result.status();
    }
    return static_cast<T>(result.value());
  }
};

template <typename T>
class EnumStreamFinder
    : private StreamFinder<uint32_t, &StreamDecoder::ReadUint32> {
 public:
  using StreamFinder::StreamFinder;

  Result<T> Next() {
    Result<uint32_t> result = StreamFinder::Next();
    if (!result.ok()) {
      return result.status();
    }
    return static_cast<T>(result.value());
  }
};

/// @}

namespace internal {
template <typename T, auto kReadFn>
Result<T> Find(ConstByteSpan message, uint32_t field_number) {
  Finder<T, kReadFn> finder(message, field_number);
  return finder.Next();
}

template <typename T, auto kReadFn>
Result<T> Find(stream::Reader& reader, uint32_t field_number) {
  StreamFinder<T, kReadFn> finder(reader, field_number);
  return finder.Next();
}

}  // namespace internal

/// @submodule{pw_protobuf,find}

/// @brief Scans a serialized protobuf message for a `uint32` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<uint32_t> FindUint32(ConstByteSpan message,
                                   uint32_t field_number) {
  return internal::Find<uint32_t, &Decoder::ReadUint32>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<uint32_t> FindUint32(ConstByteSpan message, T field) {
  return FindUint32(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for a `uint32` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<uint32_t> FindUint32(stream::Reader& message_stream,
                                   uint32_t field_number) {
  return internal::Find<uint32_t, &StreamDecoder::ReadUint32>(message_stream,
                                                              field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<uint32_t> FindUint32(stream::Reader& message_stream, T field) {
  return FindUint32(message_stream, static_cast<uint32_t>(field));
}

using Uint32Finder = Finder<uint32_t, &Decoder::ReadUint32>;
using Uint32StreamFinder = StreamFinder<uint32_t, &StreamDecoder::ReadUint32>;

/// @brief Scans a serialized protobuf message for an `int32` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int32_t> FindInt32(ConstByteSpan message, uint32_t field_number) {
  return internal::Find<int32_t, &Decoder::ReadInt32>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int32_t> FindInt32(ConstByteSpan message, T field) {
  return FindInt32(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for an `int32` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int32_t> FindInt32(stream::Reader& message_stream,
                                 uint32_t field_number) {
  return internal::Find<int32_t, &StreamDecoder::ReadInt32>(message_stream,
                                                            field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int32_t> FindInt32(stream::Reader& message_stream, T field) {
  return FindInt32(message_stream, static_cast<uint32_t>(field));
}

using Int32Finder = Finder<int32_t, &Decoder::ReadInt32>;
using Int32StreamFinder = StreamFinder<int32_t, &StreamDecoder::ReadInt32>;

/// @brief Scans a serialized protobuf message for an `sint32` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int32_t> FindSint32(ConstByteSpan message,
                                  uint32_t field_number) {
  return internal::Find<int32_t, &Decoder::ReadSint32>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int32_t> FindSint32(ConstByteSpan message, T field) {
  return FindSint32(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for an `sint32` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int32_t> FindSint32(stream::Reader& message_stream,
                                  uint32_t field_number) {
  return internal::Find<int32_t, &StreamDecoder::ReadSint32>(message_stream,
                                                             field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int32_t> FindSint32(stream::Reader& message_stream, T field) {
  return FindSint32(message_stream, static_cast<uint32_t>(field));
}

using Sint32Finder = Finder<int32_t, &Decoder::ReadSint32>;
using Sint32StreamFinder = StreamFinder<int32_t, &StreamDecoder::ReadSint32>;

/// @brief Scans a serialized protobuf message for a `uint64` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<uint64_t> FindUint64(ConstByteSpan message,
                                   uint32_t field_number) {
  return internal::Find<uint64_t, &Decoder::ReadUint64>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<uint64_t> FindUint64(ConstByteSpan message, T field) {
  return FindUint64(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for a `uint64` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<uint64_t> FindUint64(stream::Reader& message_stream,
                                   uint32_t field_number) {
  return internal::Find<uint64_t, &StreamDecoder::ReadUint64>(message_stream,
                                                              field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<uint64_t> FindUint64(stream::Reader& message_stream, T field) {
  return FindUint64(message_stream, static_cast<uint32_t>(field));
}

using Uint64Finder = Finder<uint64_t, &Decoder::ReadUint64>;
using Uint64StreamFinder = StreamFinder<uint64_t, &StreamDecoder::ReadUint64>;

/// @brief Scans a serialized protobuf message for an `int64` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int64_t> FindInt64(ConstByteSpan message, uint32_t field_number) {
  return internal::Find<int64_t, &Decoder::ReadInt64>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int64_t> FindInt64(ConstByteSpan message, T field) {
  return FindInt64(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for an `int64` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int64_t> FindInt64(stream::Reader& message_stream,
                                 uint32_t field_number) {
  return internal::Find<int64_t, &StreamDecoder::ReadInt64>(message_stream,
                                                            field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int64_t> FindInt64(stream::Reader& message_stream, T field) {
  return FindInt64(message_stream, static_cast<uint32_t>(field));
}

using Int64Finder = Finder<int64_t, &Decoder::ReadInt64>;
using Int64StreamFinder = StreamFinder<int64_t, &StreamDecoder::ReadInt64>;

/// @brief Scans a serialized protobuf message for an `sint64` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int64_t> FindSint64(ConstByteSpan message,
                                  uint32_t field_number) {
  return internal::Find<int64_t, &Decoder::ReadSint64>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int64_t> FindSint64(ConstByteSpan message, T field) {
  return FindSint64(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for an `sint64` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int64_t> FindSint64(stream::Reader& message_stream,
                                  uint32_t field_number) {
  return internal::Find<int64_t, &StreamDecoder::ReadSint64>(message_stream,
                                                             field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int64_t> FindSint64(stream::Reader& message_stream, T field) {
  return FindSint64(message_stream, static_cast<uint32_t>(field));
}

using Sint64Finder = Finder<int64_t, &Decoder::ReadSint64>;
using Sint64StreamFinder = StreamFinder<int64_t, &StreamDecoder::ReadSint64>;

/// @brief Scans a serialized protobuf message for a `bool` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<bool> FindBool(ConstByteSpan message, uint32_t field_number) {
  return internal::Find<bool, &Decoder::ReadBool>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<bool> FindBool(ConstByteSpan message, T field) {
  return FindBool(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for a `bool` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<bool> FindBool(stream::Reader& message_stream,
                             uint32_t field_number) {
  return internal::Find<bool, &StreamDecoder::ReadBool>(message_stream,
                                                        field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<bool> FindBool(stream::Reader& message_stream, T field) {
  return FindBool(message_stream, static_cast<uint32_t>(field));
}

using BoolFinder = Finder<bool, &Decoder::ReadBool>;
using BoolStreamFinder = StreamFinder<bool, &StreamDecoder::ReadBool>;

/// @brief Scans a serialized protobuf message for a `fixed32` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<uint32_t> FindFixed32(ConstByteSpan message,
                                    uint32_t field_number) {
  return internal::Find<uint32_t, &Decoder::ReadFixed32>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<uint32_t> FindFixed32(ConstByteSpan message, T field) {
  return FindFixed32(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for a `fixed32` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<uint32_t> FindFixed32(stream::Reader& message_stream,
                                    uint32_t field_number) {
  return internal::Find<uint32_t, &StreamDecoder::ReadFixed32>(message_stream,
                                                               field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<uint32_t> FindFixed32(stream::Reader& message_stream, T field) {
  return FindFixed32(message_stream, static_cast<uint32_t>(field));
}

using Fixed32Finder = Finder<uint32_t, &Decoder::ReadFixed32>;
using Fixed32StreamFinder = StreamFinder<uint32_t, &StreamDecoder::ReadFixed32>;

/// @brief Scans a serialized protobuf message for a `fixed64` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<uint64_t> FindFixed64(ConstByteSpan message,
                                    uint32_t field_number) {
  return internal::Find<uint64_t, &Decoder::ReadFixed64>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<uint64_t> FindFixed64(ConstByteSpan message, T field) {
  return FindFixed64(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for a `fixed64` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<uint64_t> FindFixed64(stream::Reader& message_stream,
                                    uint32_t field_number) {
  return internal::Find<uint64_t, &StreamDecoder::ReadFixed64>(message_stream,
                                                               field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<uint64_t> FindFixed64(stream::Reader& message_stream, T field) {
  return FindFixed64(message_stream, static_cast<uint32_t>(field));
}

using Fixed64Finder = Finder<uint64_t, &Decoder::ReadFixed64>;
using Fixed64StreamFinder = StreamFinder<uint64_t, &StreamDecoder::ReadFixed64>;

/// @brief Scans a serialized protobuf message for an `sfixed32` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int32_t> FindSfixed32(ConstByteSpan message,
                                    uint32_t field_number) {
  return internal::Find<int32_t, &Decoder::ReadSfixed32>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int32_t> FindSfixed32(ConstByteSpan message, T field) {
  return FindSfixed32(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for an `sfixed32` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int32_t> FindSfixed32(stream::Reader& message_stream,
                                    uint32_t field_number) {
  return internal::Find<int32_t, &StreamDecoder::ReadSfixed32>(message_stream,
                                                               field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int32_t> FindSfixed32(stream::Reader& message_stream, T field) {
  return FindSfixed32(message_stream, static_cast<uint32_t>(field));
}

using Sfixed32Finder = Finder<int32_t, &Decoder::ReadSfixed32>;
using Sfixed32StreamFinder =
    StreamFinder<int32_t, &StreamDecoder::ReadSfixed32>;

/// @brief Scans a serialized protobuf message for an `sfixed64` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int64_t> FindSfixed64(ConstByteSpan message,
                                    uint32_t field_number) {
  return internal::Find<int64_t, &Decoder::ReadSfixed64>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int64_t> FindSfixed64(ConstByteSpan message, T field) {
  return FindSfixed64(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for an `sfixed64` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<int64_t> FindSfixed64(stream::Reader& message_stream,
                                    uint32_t field_number) {
  return internal::Find<int64_t, &StreamDecoder::ReadSfixed64>(message_stream,
                                                               field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<int64_t> FindSfixed64(stream::Reader& message_stream, T field) {
  return FindSfixed64(message_stream, static_cast<uint32_t>(field));
}

using Sfixed64Finder = Finder<int64_t, &Decoder::ReadSfixed64>;
using Sfixed64StreamFinder =
    StreamFinder<int64_t, &StreamDecoder::ReadSfixed64>;

/// @brief Scans a serialized protobuf message for a `float` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<float> FindFloat(ConstByteSpan message, uint32_t field_number) {
  return internal::Find<float, &Decoder::ReadFloat>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<float> FindFloat(ConstByteSpan message, T field) {
  return FindFloat(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for a `float` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<float> FindFloat(stream::Reader& message_stream,
                               uint32_t field_number) {
  return internal::Find<float, &StreamDecoder::ReadFloat>(message_stream,
                                                          field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<float> FindFloat(stream::Reader& message_stream, T field) {
  return FindFloat(message_stream, static_cast<uint32_t>(field));
}

using FloatFinder = Finder<float, &Decoder::ReadFloat>;
using FloatStreamFinder = StreamFinder<float, &StreamDecoder::ReadFloat>;

/// @brief Scans a serialized protobuf message for a `double` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<double> FindDouble(ConstByteSpan message, uint32_t field_number) {
  return internal::Find<double, &Decoder::ReadDouble>(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<double> FindDouble(ConstByteSpan message, T field) {
  return FindDouble(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for a `double` field.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<double> FindDouble(stream::Reader& message_stream,
                                 uint32_t field_number) {
  return internal::Find<double, &StreamDecoder::ReadDouble>(message_stream,
                                                            field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<double> FindDouble(stream::Reader& message_stream, T field) {
  return FindDouble(message_stream, static_cast<uint32_t>(field));
}

using DoubleFinder = Finder<double, &Decoder::ReadDouble>;
using DoubleStreamFinder = StreamFinder<double, &StreamDecoder::ReadDouble>;

/// @brief Scans a serialized protobuf message for a `string` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @note The returned string is NOT null-terminated.
///
/// @returns @Result{a subspan of the buffer containing the string field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<std::string_view> FindString(ConstByteSpan message,
                                           uint32_t field_number) {
  return internal::Find<std::string_view, &Decoder::ReadString>(message,
                                                                field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<std::string_view> FindString(ConstByteSpan message, T field) {
  return FindString(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for a `string`field, copying its
/// data into the provided buffer.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
/// @param out The buffer to which to write the string.
///
/// @note The returned string is NOT null-terminated.
///
/// @returns
/// * @OK: Returns the size of the copied data.
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline StatusWithSize FindString(stream::Reader& message_stream,
                                 uint32_t field_number,
                                 span<char> out) {
  StreamDecoder decoder(message_stream);
  Status status = internal::AdvanceToField(decoder, field_number);
  if (!status.ok()) {
    return StatusWithSize(status, 0);
  }
  StatusWithSize sws = decoder.ReadString(out);

  // The StreamDecoder returns a NOT_FOUND if trying to read the wrong type for
  // a field. Remap this to FAILED_PRECONDITION for consistency with the
  // non-stream Find.
  return sws.status().IsNotFound() ? StatusWithSize::FailedPrecondition() : sws;
}

/// @brief Scans a serialized protobuf message for a `string`field, copying its
/// data into the provided buffer.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
/// @param out String to which to write the found value.
///
/// @returns
/// * @OK: Returns the size of the copied data.
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline StatusWithSize FindString(stream::Reader& message_stream,
                                 uint32_t field_number,
                                 InlineString<>& out) {
  StatusWithSize sws;

  out.resize_and_overwrite([&](char* data, size_t size) {
    sws = FindString(message_stream, field_number, span(data, size));
    return sws.size();
  });

  return sws;
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline StatusWithSize FindString(stream::Reader& message_stream,
                                 T field,
                                 span<char> out) {
  return FindString(message_stream, static_cast<uint32_t>(field), out);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline StatusWithSize FindString(stream::Reader& message_stream,
                                 T field,
                                 InlineString<>& out) {
  return FindString(message_stream, static_cast<uint32_t>(field), out);
}

using StringFinder = Finder<std::string_view, &Decoder::ReadString>;

/// @brief Scans a serialized protobuf message for a `bytes` field.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the subspan of the buffer containing the bytes field}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<ConstByteSpan> FindBytes(ConstByteSpan message,
                                       uint32_t field_number) {
  return internal::Find<ConstByteSpan, &Decoder::ReadBytes>(message,
                                                            field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<ConstByteSpan> FindBytes(ConstByteSpan message, T field) {
  return FindBytes(message, static_cast<uint32_t>(field));
}

/// @brief Scans a serialized protobuf message for a `bytes` field, copying its
/// data into the provided buffer.
///
/// @param message_stream The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns
/// * @OK: Returns the size of the copied data.
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline StatusWithSize FindBytes(stream::Reader& message_stream,
                                uint32_t field_number,
                                ByteSpan out) {
  StreamDecoder decoder(message_stream);
  Status status = internal::AdvanceToField(decoder, field_number);
  if (!status.ok()) {
    return StatusWithSize(status, 0);
  }
  StatusWithSize sws = decoder.ReadBytes(out);

  // The StreamDecoder returns a NOT_FOUND if trying to read the wrong type for
  // a field. Remap this to FAILED_PRECONDITION for consistency with the
  // non-stream Find.
  return sws.status().IsNotFound() ? StatusWithSize::FailedPrecondition() : sws;
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline StatusWithSize FindBytes(stream::Reader& message_stream,
                                T field,
                                ByteSpan out) {
  return FindBytes(message_stream, static_cast<uint32_t>(field), out);
}

using BytesFinder = Finder<ConstByteSpan, &Decoder::ReadBytes>;

/// @brief Scans a serialized protobuf message for a submessage.
///
/// @param message The serialized message to search.
/// @param field_number Protobuf field number of the field.
///
/// @returns @Result{the subspan of the buffer containing the submessage}
/// * @NOT_FOUND: The field is not present.
/// * @DATA_LOSS: The serialized message is not a valid protobuf.
/// * @FAILED_PRECONDITION: The field exists, but is not the correct type.
inline Result<ConstByteSpan> FindSubmessage(ConstByteSpan message,
                                            uint32_t field_number) {
  // On the wire, a submessage is identical to bytes. This function exists only
  // to clarify users' intent.
  return FindBytes(message, field_number);
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
inline Result<ConstByteSpan> FindSubmessage(ConstByteSpan message, T field) {
  return FindSubmessage(message, static_cast<uint32_t>(field));
}

/// Returns a span containing the raw bytes of the value.
inline Result<ConstByteSpan> FindRaw(ConstByteSpan message,
                                     uint32_t field_number) {
  Decoder decoder(message);
  PW_TRY(internal::AdvanceToField(decoder, field_number));
  return decoder.RawFieldBytes();
}

template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
Result<ConstByteSpan> FindRaw(ConstByteSpan message, T field) {
  return FindRaw(message, static_cast<uint32_t>(field));
}

/// @}

}  // namespace pw::protobuf
