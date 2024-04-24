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

#include "pw_tokenizer/detokenize.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>
#include <vector>

#include "pw_bytes/bit.h"
#include "pw_bytes/endian.h"
#include "pw_result/result.h"
#include "pw_tokenizer/base64.h"
#include "pw_tokenizer/internal/decode.h"
#include "pw_tokenizer/nested_tokenization.h"

namespace pw::tokenizer {
namespace {

class NestedMessageDetokenizer {
 public:
  NestedMessageDetokenizer(const Detokenizer& detokenizer)
      : detokenizer_(detokenizer) {}

  void Detokenize(std::string_view chunk) {
    for (char next_char : chunk) {
      Detokenize(next_char);
    }
  }

  bool OutputChangedSinceLastCheck() {
    const bool changed = output_changed_;
    output_changed_ = false;
    return changed;
  }

  void Detokenize(char next_char) {
    switch (state_) {
      case kNonMessage:
        if (next_char == PW_TOKENIZER_NESTED_PREFIX) {
          message_buffer_.push_back(next_char);
          state_ = kMessage;
        } else {
          output_.push_back(next_char);
        }
        break;
      case kMessage:
        if (base64::IsValidChar(next_char)) {
          message_buffer_.push_back(next_char);
        } else {
          HandleEndOfMessage();
          if (next_char == PW_TOKENIZER_NESTED_PREFIX) {
            message_buffer_.push_back(next_char);
          } else {
            output_.push_back(next_char);
            state_ = kNonMessage;
          }
        }
        break;
    }
  }

  std::string Flush() {
    if (state_ == kMessage) {
      HandleEndOfMessage();
      state_ = kNonMessage;
    }
    std::string output(std::move(output_));
    output_.clear();
    return output;
  }

 private:
  void HandleEndOfMessage() {
    if (auto result = detokenizer_.DetokenizeBase64Message(message_buffer_);
        result.ok()) {
      output_ += result.BestString();
      output_changed_ = true;
    } else {
      output_ += message_buffer_;  // Keep the original if it doesn't decode.
    }
    message_buffer_.clear();
  }

  const Detokenizer& detokenizer_;
  std::string output_;
  std::string message_buffer_;

  enum : uint8_t { kNonMessage, kMessage } state_ = kNonMessage;
  bool output_changed_ = false;
};

std::string UnknownTokenMessage(uint32_t value) {
  std::string output(PW_TOKENIZER_ARG_DECODING_ERROR_PREFIX "unknown token ");

  // Output a hexadecimal version of the token.
  for (int shift = 28; shift >= 0; shift -= 4) {
    output.push_back("0123456789abcdef"[(value >> shift) & 0xF]);
  }

  output.append(PW_TOKENIZER_ARG_DECODING_ERROR_SUFFIX);
  return output;
}

// Decoding result with the date removed, for sorting.
using DecodingResult = std::pair<DecodedFormatString, uint32_t>;

// Determines if one result is better than the other if collisions occurred.
// Returns true if lhs is preferred over rhs. This logic should match the
// collision resolution logic in detokenize.py.
bool IsBetterResult(const DecodingResult& lhs, const DecodingResult& rhs) {
  // Favor the result for which decoding succeeded.
  if (lhs.first.ok() != rhs.first.ok()) {
    return lhs.first.ok();
  }

  // Favor the result for which all bytes were decoded.
  if ((lhs.first.remaining_bytes() == 0u) !=
      (rhs.first.remaining_bytes() == 0u)) {
    return lhs.first.remaining_bytes() == 0u;
  }

  // Favor the result with fewer decoding errors.
  if (lhs.first.decoding_errors() != rhs.first.decoding_errors()) {
    return lhs.first.decoding_errors() < rhs.first.decoding_errors();
  }

  // Favor the result that successfully decoded the most arguments.
  if (lhs.first.argument_count() != rhs.first.argument_count()) {
    return lhs.first.argument_count() > rhs.first.argument_count();
  }

  // Favor the result that was removed from the database most recently.
  return lhs.second > rhs.second;
}

// Returns true if all characters in data are printable, space, or if the string
// is empty.
constexpr bool IsPrintableAscii(std::string_view data) {
  // This follows the logic in pw_tokenizer.decode_optionally_tokenized below:
  //
  //   if ''.join(text.split()).isprintable():
  //     return text
  //
  for (int letter : data) {
    if (std::isprint(letter) == 0 && std::isspace(letter) == 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

DetokenizedString::DetokenizedString(
    uint32_t token,
    const span<const TokenizedStringEntry>& entries,
    const span<const std::byte>& arguments)
    : token_(token), has_token_(true) {
  std::vector<DecodingResult> results;

  for (const auto& [format, date_removed] : entries) {
    results.push_back(DecodingResult{
        format.Format(span(reinterpret_cast<const uint8_t*>(arguments.data()),
                           arguments.size())),
        date_removed});
  }

  std::sort(results.begin(), results.end(), IsBetterResult);

  for (auto& result : results) {
    matches_.push_back(std::move(result.first));
  }
}

std::string DetokenizedString::BestString() const {
  return matches_.empty() ? std::string() : matches_[0].value();
}

std::string DetokenizedString::BestStringWithErrors() const {
  if (matches_.empty()) {
    return has_token_ ? UnknownTokenMessage(token_)
                      : PW_TOKENIZER_ARG_DECODING_ERROR("missing token");
  }
  return matches_[0].value_with_errors();
}

Detokenizer::Detokenizer(const TokenDatabase& database) {
  for (const auto& entry : database) {
    database_[entry.token].emplace_back(entry.string, entry.date_removed);
  }
}

Result<Detokenizer> Detokenizer::FromElfSection(
    span<const std::byte> elf_section) {
  size_t index = 0;
  std::unordered_map<uint32_t, std::vector<TokenizedStringEntry>> database;

  while (index + sizeof(_pw_tokenizer_EntryHeader) < elf_section.size()) {
    _pw_tokenizer_EntryHeader header;
    std::memcpy(
        &header, elf_section.data() + index, sizeof(_pw_tokenizer_EntryHeader));
    index += sizeof(_pw_tokenizer_EntryHeader);

    if (header.magic != _PW_TOKENIZER_ENTRY_MAGIC) {
      return Status::DataLoss();
    }

    index += header.domain_length;
    if (index + header.string_length <= elf_section.size()) {
      // TODO(b/326365218): Construct FormatString with string_view to avoid
      // creating a copy here.
      std::string entry(
          reinterpret_cast<const char*>(elf_section.data() + index),
          header.string_length);
      index += header.string_length;
      database[header.token].emplace_back(entry.c_str(),
                                          TokenDatabase::kDateRemovedNever);
    }
  }
  return Detokenizer(std::move(database));
}

DetokenizedString Detokenizer::Detokenize(
    const span<const std::byte>& encoded) const {
  // The token is missing from the encoded data; there is nothing to do.
  if (encoded.empty()) {
    return DetokenizedString();
  }

  uint32_t token = bytes::ReadInOrder<uint32_t>(
      endian::little, encoded.data(), encoded.size());

  const auto result = database_.find(token);

  return DetokenizedString(
      token,
      result == database_.end() ? span<TokenizedStringEntry>()
                                : span(result->second),
      encoded.size() < sizeof(token) ? span<const std::byte>()
                                     : encoded.subspan(sizeof(token)));
}

DetokenizedString Detokenizer::DetokenizeBase64Message(
    std::string_view text) const {
  std::string buffer(text);
  buffer.resize(PrefixedBase64DecodeInPlace(buffer));
  return Detokenize(buffer);
}

std::string Detokenizer::DetokenizeText(std::string_view text,
                                        const unsigned max_passes) const {
  NestedMessageDetokenizer detokenizer(*this);
  detokenizer.Detokenize(text);

  std::string result;
  unsigned pass = 1;

  while (true) {
    result = detokenizer.Flush();
    if (pass >= max_passes || !detokenizer.OutputChangedSinceLastCheck()) {
      break;
    }
    detokenizer.Detokenize(result);
    pass += 1;
  }
  return result;
}

std::string Detokenizer::DecodeOptionallyTokenizedData(
    const ConstByteSpan& optionally_tokenized_data) {
  // Try detokenizing as binary using the best result if available, else use
  // the input data as a string.
  const auto result = Detokenize(optionally_tokenized_data);
  const bool found_matches = !result.matches().empty();
  // Note: unlike pw_tokenizer.proto.decode_optionally_tokenized, this decoding
  // process does not encode and decode UTF8 format, it is sufficient to check
  // if the data is printable ASCII.
  const std::string data =
      found_matches
          ? result.BestString()
          : std::string(
                reinterpret_cast<const char*>(optionally_tokenized_data.data()),
                optionally_tokenized_data.size());

  const bool is_data_printable = IsPrintableAscii(data);
  if (!found_matches && !is_data_printable) {
    // Assume the token is unknown or the data is corrupt.
    std::vector<char> base64_encoding_buffer(
        Base64EncodedBufferSize(optionally_tokenized_data.size()));
    const size_t encoded_length = PrefixedBase64Encode(
        optionally_tokenized_data, span(base64_encoding_buffer));
    return std::string{base64_encoding_buffer.data(), encoded_length};
  }

  // Successfully detokenized, check if the field has more prefixed
  // base64-encoded tokens.
  const std::string field = DetokenizeText(data);
  // If anything detokenized successfully, use that.
  if (field != data) {
    return field;
  }

  // Attempt to determine whether this is an unknown token or plain text.
  // Any string with only printable or whitespace characters is plain text.
  if (found_matches || is_data_printable) {
    return data;
  }

  // Assume this field is tokenized data that could not be decoded.
  std::vector<char> base64_encoding_buffer(
      Base64EncodedBufferSize(optionally_tokenized_data.size()));
  const size_t encoded_length = PrefixedBase64Encode(
      optionally_tokenized_data, span(base64_encoding_buffer));
  return std::string{base64_encoding_buffer.data(), encoded_length};
}

}  // namespace pw::tokenizer
