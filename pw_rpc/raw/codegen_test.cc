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

#include "gtest/gtest.h"
#include "pw_protobuf/decoder.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc_test_protos/test.pwpb.h"
#include "pw_rpc_test_protos/test.raw_rpc.pb.h"

namespace pw::rpc {
namespace test {

class TestService final : public generated::TestService<TestService> {
 public:
  StatusWithSize TestRpc(ServerContext&,
                         ConstByteSpan request,
                         ByteSpan response) {
    int64_t integer;
    uint32_t status_code;
    protobuf::Decoder decoder(request);

    while (decoder.Next().ok()) {
      switch (static_cast<TestRequest::Fields>(decoder.FieldNumber())) {
        case TestRequest::Fields::INTEGER:
          decoder.ReadInt64(&integer);
          break;
        case TestRequest::Fields::STATUS_CODE:
          decoder.ReadUint32(&status_code);
          break;
      }
    }

    protobuf::NestedEncoder encoder(response);
    TestResponse::Encoder test_response(&encoder);
    test_response.WriteValue(integer + 1);

    return StatusWithSize(static_cast<Status::Code>(status_code),
                          encoder.Encode().value().size());
  }

  void TestStreamRpc(ServerContext&, ConstByteSpan, RawServerWriter&) {}
};

}  // namespace test

namespace {

TEST(RawCodegen, CompilesProperly) {
  test::TestService service;
  EXPECT_EQ(service.id(), internal::Hash("pw.rpc.test.TestService"));
  EXPECT_STREQ(service.name(), "TestService");
}

}  // namespace
}  // namespace pw::rpc
