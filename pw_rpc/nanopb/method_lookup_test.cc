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
#include "pw_rpc/nanopb_test_method_context.h"
#include "pw_rpc/raw_test_method_context.h"
#include "pw_rpc_test_protos/test.rpc.pb.h"

namespace pw::rpc {
namespace {

class MixedService1 : public test::generated::TestService<MixedService1> {
 public:
  StatusWithSize TestRpc(ServerContext&, ConstByteSpan, ByteSpan) {
    return StatusWithSize(123);
  }

  void TestStreamRpc(ServerContext&,
                     const pw_rpc_test_TestRequest&,
                     ServerWriter<pw_rpc_test_TestStreamResponse>&) {
    called_streaming_method = true;
  }

  bool called_streaming_method = false;
};

class MixedService2 : public test::generated::TestService<MixedService2> {
 public:
  Status TestRpc(ServerContext&,
                 const pw_rpc_test_TestRequest&,
                 pw_rpc_test_TestResponse&) {
    return Status::Unauthenticated();
  }

  void TestStreamRpc(ServerContext&, ConstByteSpan, RawServerWriter&) {
    called_streaming_method = true;
  }

  bool called_streaming_method = false;
};

TEST(MixedService1, CallRawMethod) {
  PW_RAW_TEST_METHOD_CONTEXT(MixedService1, TestRpc) context;
  StatusWithSize sws = context.call({});
  EXPECT_TRUE(sws.ok());
  EXPECT_EQ(123u, sws.size());
}

TEST(MixedService1, CallNanopbMethod) {
  PW_NANOPB_TEST_METHOD_CONTEXT(MixedService1, TestStreamRpc) context;
  ASSERT_FALSE(context.service().called_streaming_method);
  context.call({});
  EXPECT_TRUE(context.service().called_streaming_method);
}

TEST(MixedService2, CallNanopbMethod) {
  PW_NANOPB_TEST_METHOD_CONTEXT(MixedService2, TestRpc) context;
  Status status = context.call({});
  EXPECT_EQ(Status::Unauthenticated(), status);
}

TEST(MixedService2, CallRawMethod) {
  PW_RAW_TEST_METHOD_CONTEXT(MixedService2, TestStreamRpc) context;
  ASSERT_FALSE(context.service().called_streaming_method);
  context.call({});
  EXPECT_TRUE(context.service().called_streaming_method);
}

}  // namespace
}  // namespace pw::rpc
