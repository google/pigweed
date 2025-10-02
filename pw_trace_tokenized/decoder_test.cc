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

#include "pw_trace_tokenized/decoder.h"

#include "pw_assert/check.h"
#include "pw_bytes/array.h"
#include "pw_log/log.h"
#include "pw_stream/memory_stream.h"
#include "pw_tokenizer/detokenize.h"
#include "pw_trace/trace.h"
#include "pw_unit_test/framework.h"

#define EXPECT_SEQ_EQ(seq1, seq2) \
  EXPECT_TRUE(std::equal(seq1.begin(), seq1.end(), seq2.begin(), seq2.end()))

#undef PW_TRACE_MODULE_NAME
#define PW_TRACE_MODULE_NAME "MyModule"

namespace pw::trace {
namespace {

using namespace std::literals::string_view_literals;

using tokenizer::Detokenizer;

constexpr uint64_t kTicksPerSec = 1000;  // 1 kHz

TEST(TokenizedDecoder, ReadSizePrefixed) {
  // Set up detokenizer
  // Token string: "event_type|flag|module|group|label|<optional DATA_FMT>"
  static constexpr char kTokenDbCsv[] =
      // token (hex!), date, domain ,string
      "11223344,,trace,"
      "PW_TRACE_EVENT_TYPE_ASYNC_STEP|0|MyModule|MyGroup|MyLabel|"
      "MyDataFmt\n";

  pw::Result<Detokenizer> detok = Detokenizer::FromCsv(kTokenDbCsv);
  PW_CHECK_OK(detok);

  // Craft raw input data
  constexpr auto kData = bytes::Array<0x41, 0x42, 0x43, 0x44>();
  constexpr auto kEncoded =
      bytes::Concat(bytes::Array<0x44, 0x33, 0x22, 0x11>(),  // string token
                    bytes::Array<0x96, 0x01>(),              // ticks = 150
                    bytes::Array<0xcd, 0x02>(),              // trace_id = 333
                    kData);
  constexpr auto kEncodedWithPrefix = bytes::Concat(
      std::byte{kEncoded.size()},  // length prefix from transfer_handler.cc
      kEncoded);

  // Set up decoder
  constexpr uint64_t kTimeOffset = 2345;
  TokenizedDecoder decoder(*detok, kTicksPerSec);
  decoder.SetTimeOffset(kTimeOffset);

  // Process input data
  stream::MemoryReader reader(kEncodedWithPrefix);
  auto result = decoder.ReadSizePrefixed(reader);
  PW_TEST_ASSERT_OK(result);

  EXPECT_EQ(result->type, EventType::PW_TRACE_EVENT_TYPE_ASYNC_STEP);
  EXPECT_STREQ(result->flags_str.c_str(), "0");
  EXPECT_STREQ(result->module.c_str(), "MyModule");
  EXPECT_STREQ(result->group.c_str(), "MyGroup");
  EXPECT_STREQ(result->label.c_str(), "MyLabel");
  EXPECT_STREQ(result->data_fmt.c_str(), "MyDataFmt");

  EXPECT_EQ(result->timestamp_usec, kTimeOffset + (150 * kTicksPerSec));
  EXPECT_EQ(result->trace_id, 333u);  // Only "ASYNC" have this
  EXPECT_SEQ_EQ(result->data, kData);

  // Read the same data again, and ensure the timestamp advances.
  stream::MemoryReader reader2(kEncodedWithPrefix);
  result = decoder.ReadSizePrefixed(reader2);
  PW_TEST_ASSERT_OK(result);
  EXPECT_EQ(result->timestamp_usec, kTimeOffset + (2 * 150 * kTicksPerSec));
}

}  // namespace
}  // namespace pw::trace
