// Copyright 2025 The Pigweed Authors
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

#include "pw_assert/check.h"
#include "pw_perf_test/perf_test.h"
#include "pw_tokenizer/detokenize.h"

namespace pw::tokenizer {
namespace {

constexpr char kDataWithArguments[] =
    "TOKENS\0\0"
    "\x09\x00\x00\x00"
    "\0\0\0\0"
    "\x00\x00\x00\x00----"
    "\x0A\x0B\x0C\x0D----"
    "\x0E\x0F\x00\x01----"
    "\xAA\xAA\xAA\xAA----"
    "\xBB\xBB\xBB\xBB----"
    "\xCC\xCC\xCC\xCC----"
    "\xDD\xDD\xDD\xDD----"
    "\xEE\xEE\xEE\xEE----"
    "\xFF\xFF\xFF\xFF----"
    "\0"
    "Use the %s, %s.\0"
    "Now there are %d of %s!\0"
    "%c!\0"    // AA
    "%hhu!\0"  // BB
    "%hu!\0"   // CC
    "%u!\0"    // DD
    "%lu!\0"   // EE
    "%llu!";   // FF
constexpr TokenDatabase kDatabase = TokenDatabase::Create<kDataWithArguments>();

void DetokenizeText(perf_test::State& state,
                    std::string_view text,
                    std::string_view expected) {
  Detokenizer detokenizer(kDatabase);

  std::string result = detokenizer.DetokenizeText(text);

  while (state.KeepRunning()) {
    result = detokenizer.DetokenizeText(text);
  }

  PW_CHECK(result == expected);
}

PW_PERF_TEST(DetokenizeText, DetokenizeText, "$qqqqqvwB", "~!");

PW_PERF_TEST(DeteoknizeTextWithOnePaddingChar,
             DetokenizeText,
             "$Dg8AAYABBHRoZW0=",
             "Now there are 64 of them!");

PW_PERF_TEST(DeteoknizeTextWithTwoPaddingChars,
             DetokenizeText,
             "$Dg8AAQQEdGhlbQ==",
             "Now there are 2 of them!");

PW_PERF_TEST(DetokenizeTextTwoMessages,
             DetokenizeText,
             "What the $qqqqqvwB, $Dg8AAQQEdGhlbQ==",
             "What the ~!, Now there are 2 of them!");

}  // namespace
}  // namespace pw::tokenizer
