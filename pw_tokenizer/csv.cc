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

#include "pw_tokenizer_private/csv.h"

#include "pw_log/log.h"

namespace pw::tokenizer::internal {
namespace {

constexpr char kSeparator = ',';
[[nodiscard]] constexpr bool IsLineEnd(char ch) {
  return ch == '\r' || ch == '\n';
}

}  // namespace

std::optional<std::vector<std::string>> CsvParser::ParseCharacterOrEof(
    int val) {
  const char ch = static_cast<char>(val);
  switch (state_) {
    case kNewEntry:
      if (ch == '"') {
        state_ = kQuotedEntry;
      } else if (IsLineEnd(ch)) {
        if (line_.size() > 1) {  // Ignore empty lines
          return FinishLine();
        }
      } else if (ch == kSeparator) {
        line_.emplace_back();  // Append the empty entry, start the next
      } else {
        state_ = kUnquotedEntry;
        line_.back().push_back(ch);
      }
      break;
    case kUnquotedEntry:
      if (val == kEndOfFile || IsLineEnd(ch)) {
        return FinishLine();
      }
      if (ch == kSeparator) {
        state_ = kNewEntry;
        line_.emplace_back();
      } else {
        line_.back().push_back(ch);
      }
      break;
    case kQuotedEntry:
      if (val == kEndOfFile) {
        PW_LOG_WARN("Unexpected end-of-file in quoted entry; ignoring line");
      } else if (ch == '"') {
        state_ = kQuotedEntryQuote;
      } else {
        line_.back().push_back(ch);
      }
      break;
    case kQuotedEntryQuote:
      if (ch == '"') {
        state_ = kQuotedEntry;
        line_.back().push_back('"');
      } else if (val == kEndOfFile || IsLineEnd(ch)) {
        return FinishLine();
      } else if (ch == kSeparator) {
        state_ = kNewEntry;
        line_.emplace_back();
      } else {
        PW_LOG_WARN(
            "Unexpected character '%c' after quoted entry; expected ',' or a "
            "line ending; skipping line",
            ch);
        state_ = kError;
        line_.clear();
        line_.emplace_back();
      }
      break;
    case kError:
      if (IsLineEnd(ch)) {  // Skip chars until end-of-line
        state_ = kNewEntry;
      }
      break;
  }
  return std::nullopt;
}

std::optional<std::vector<std::string>> CsvParser::FinishLine() {
  state_ = kNewEntry;
  std::optional<std::vector<std::string>> completed_line = std::move(line_);
  line_.clear();
  line_.emplace_back();
  return completed_line;
}

}  // namespace pw::tokenizer::internal
