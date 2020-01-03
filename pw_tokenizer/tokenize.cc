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

// This file defines the functions that encode tokenized logs at runtime. These
// are the only pw_tokenizer functions present in a binary that tokenizes
// strings. All other tokenizing code is resolved at compile time.

#include "pw_tokenizer/tokenize.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstring>

#include "pw_varint/varint.h"

namespace pw::tokenizer {
namespace {

// Store metadata about this compilation's string tokenization in the ELF.
//
// The tokenizer metadata will not go into the on-device executable binary code.
// This metadata will be present in the ELF file's .tokenizer_info section, from
// which the host-side tooling (Python, Java, etc.) can understand how to decode
// tokenized strings for the given binary. Only attributes that affect the
// decoding process are recorded.
//
// Tokenizer metadata is stored in an array of key-value pairs. Each Metadata
// object is 32 bytes: a 24-byte string and an 8-byte value. Metadata structs
// may be parsed in Python with the struct format '24s<Q'.
PW_PACKED(struct) Metadata {
  char name[24];   // name of the metadata field
  uint64_t value;  // value of the field
};

static_assert(sizeof(Metadata) == 32);

// Store tokenization metadata in its own section.
constexpr Metadata metadata[] PW_KEEP_IN_SECTION(".tokenzier_info") = {
    {"hash_length_bytes", PW_TOKENIZER_CFG_HASH_LENGTH},
    {"sizeof_long", sizeof(long)},            // %l conversion specifier
    {"sizeof_intmax_t", sizeof(intmax_t)},    // %j conversion specifier
    {"sizeof_size_t", sizeof(size_t)},        // %z conversion specifier
    {"sizeof_ptrdiff_t", sizeof(ptrdiff_t)},  // %t conversion specifier
};

// Declare the types as an enum for convenience.
enum class ArgType : uint8_t {
  kInt = PW_TOKENIZER_ARG_TYPE_INT,
  kInt64 = PW_TOKENIZER_ARG_TYPE_INT64,
  kDouble = PW_TOKENIZER_ARG_TYPE_DOUBLE,
  kString = PW_TOKENIZER_ARG_TYPE_STRING,
};

// Just to be safe, make sure these values are what we expect them to be.
static_assert(0b00u == static_cast<uint8_t>(ArgType::kInt));
static_assert(0b01u == static_cast<uint8_t>(ArgType::kInt64));
static_assert(0b10u == static_cast<uint8_t>(ArgType::kDouble));
static_assert(0b11u == static_cast<uint8_t>(ArgType::kString));

// Buffer for encoding a tokenized string and arguments.
struct EncodedMessage {
  pw_TokenizerStringToken token;
  std::array<uint8_t, PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES> args;
};

static_assert(offsetof(EncodedMessage, args) == sizeof(EncodedMessage::token),
              "EncodedMessage should not have padding bytes between members");

size_t EncodeInt(int value, const span<uint8_t>& output) {
  return varint::Encode(value, pw::as_writable_bytes(output));
}

size_t EncodeInt64(int64_t value, const span<uint8_t>& output) {
  return varint::Encode(value, pw::as_writable_bytes(output));
}

size_t EncodeFloat(float value, const span<uint8_t>& output) {
  if (output.size() < sizeof(value)) {
    return 0;
  }
  std::memcpy(output.data(), &value, sizeof(value));
  return sizeof(value);
}

size_t EncodeString(const char* string, const span<uint8_t>& output) {
  // The top bit of the status byte indicates if the string was truncated.
  static constexpr size_t kMaxStringLength = 0x7Fu;

  if (output.empty()) {  // At least one byte is needed for the status/size.
    return 0;
  }

  if (string == nullptr) {
    string = "NULL";
  }

  // Subtract 1 to save room for the status byte.
  const size_t max_bytes = std::min(output.size(), kMaxStringLength) - 1;

  // Scan the string to find out how many bytes to copy.
  size_t bytes_to_copy = 0;
  uint8_t overflow_bit = 0;

  while (string[bytes_to_copy] != '\0') {
    if (bytes_to_copy == max_bytes) {
      overflow_bit = '\x80';
      break;
    }
    bytes_to_copy += 1;
  }

  output[0] = bytes_to_copy | overflow_bit;
  std::memcpy(output.data() + 1, string, bytes_to_copy);

  return bytes_to_copy + 1;  // include the status byte in the total
}

size_t EncodeArgs(pw_TokenizerArgTypes types,
                  va_list args,
                  span<uint8_t> output) {
  size_t arg_count = types & PW_TOKENIZER_TYPE_COUNT_MASK;
  types >>= PW_TOKENIZER_TYPE_COUNT_SIZE_BITS;

  size_t encoded_bytes = 0;
  while (arg_count != 0u) {
    // How many bytes were encoded; 0 indicates that there wasn't enough space.
    size_t argument_bytes = 0;

    switch (static_cast<ArgType>(types & 0b11u)) {
      case ArgType::kInt:
        argument_bytes = EncodeInt(va_arg(args, int), output);
        break;
      case ArgType::kInt64:
        argument_bytes = EncodeInt64(va_arg(args, int64_t), output);
        break;
      case ArgType::kDouble:
        argument_bytes =
            EncodeFloat(static_cast<float>(va_arg(args, double)), output);
        break;
      case ArgType::kString:
        argument_bytes = EncodeString(va_arg(args, const char*), output);
        break;
    }

    // If zero bytes were encoded, the encoding buffer is full.
    if (argument_bytes == 0u) {
      break;
    }

    output = output.subspan(argument_bytes);
    encoded_bytes += argument_bytes;

    arg_count -= 1;
    types >>= 2;  // each argument type is encoded in two bits
  }

  return encoded_bytes;
}

}  // namespace

extern "C" {

void pw_TokenizeToBuffer(void* buffer,
                         size_t* buffer_size_bytes,
                         pw_TokenizerStringToken token,
                         pw_TokenizerArgTypes types,
                         ...) {
  if (*buffer_size_bytes < sizeof(token)) {
    *buffer_size_bytes = 0;
    return;
  }

  std::memcpy(buffer, &token, sizeof(token));

  va_list args;
  va_start(args, types);
  const size_t encoded_bytes =
      EncodeArgs(types,
                 args,
                 span(static_cast<uint8_t*>(buffer) + sizeof(token),
                      *buffer_size_bytes - sizeof(token)));
  va_end(args);

  *buffer_size_bytes = sizeof(token) + encoded_bytes;
}

void pw_TokenizeToCallback(void (*callback)(const uint8_t* encoded_message,
                                            size_t size_bytes),
                           pw_TokenizerStringToken token,
                           pw_TokenizerArgTypes types,
                           ...) {
  EncodedMessage encoded;
  encoded.token = token;

  va_list args;
  va_start(args, types);
  const size_t encoded_bytes = EncodeArgs(types, args, encoded.args);
  va_end(args);

  callback(reinterpret_cast<const uint8_t*>(&encoded),
           sizeof(encoded.token) + encoded_bytes);
}

#if PW_TOKENIZER_CFG_ENABLE_TOKENIZE_TO_GLOBAL_HANDLER

void pw_TokenizeToGlobalHandler(pw_TokenizerStringToken token,
                                pw_TokenizerArgTypes types,
                                ...) {
  EncodedMessage encoded;
  encoded.token = token;

  va_list args;
  va_start(args, types);
  const size_t encoded_bytes = EncodeArgs(types, args, encoded.args);
  va_end(args);

  pw_TokenizerHandleEncodedMessage(reinterpret_cast<const uint8_t*>(&encoded),
                                   sizeof(encoded.token) + encoded_bytes);
}

#endif  // PW_TOKENIZER_CFG_ENABLE_TOKENIZE_TO_GLOBAL_HANDLER

#if PW_TOKENIZER_CFG_ENABLE_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD

void pw_TokenizeToGlobalHandlerWithPayload(const pw_TokenizerPayload payload,
                                           pw_TokenizerStringToken token,
                                           pw_TokenizerArgTypes types,
                                           ...) {
  EncodedMessage encoded;
  encoded.token = token;

  va_list args;
  va_start(args, types);
  const size_t encoded_bytes = EncodeArgs(types, args, encoded.args);
  va_end(args);

  pw_TokenizerHandleEncodedMessageWithPayload(
      payload,
      reinterpret_cast<const uint8_t*>(&encoded),
      sizeof(encoded.token) + encoded_bytes);
}

#endif  // PW_TOKENIZER_CFG_ENABLE_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD

}  // extern "C"

}  // namespace pw::tokenizer
