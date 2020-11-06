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
#pragma once

#include "pw_log/log.h"
#include "pw_unit_test/internal/rpc_event_handler.h"
#include "pw_unit_test_proto/unit_test.pwpb.h"
#include "pw_unit_test_proto/unit_test.raw_rpc.pb.h"

namespace pw::unit_test {

class UnitTestService final : public generated::UnitTest<UnitTestService> {
 public:
  UnitTestService() : handler_(*this), verbose_(false) {}

  void Run(ServerContext& ctx, ConstByteSpan request, RawServerWriter& writer);

 private:
  friend class internal::RpcEventHandler;

  // TODO(frolv): This function essentially performs what a pw_protobuf RPC
  // method would do. Once that API is implemented, this service should be
  // migrated to it.
  template <typename WriteFunction>
  void WriteEvent(WriteFunction event_writer) {
    protobuf::NestedEncoder<2, 3> encoder(writer_.PayloadBuffer());
    Event::Encoder event(&encoder);
    event_writer(event);
    if (Result<ConstByteSpan> result = encoder.Encode(); result.ok()) {
      writer_.Write(result.value());
    }
  }

  void WriteTestRunStart();
  void WriteTestRunEnd(const RunTestsSummary& summary);
  void WriteTestCaseStart(const TestCase& test_case);
  void WriteTestCaseEnd(TestResult result);
  void WriteTestCaseDisabled(const TestCase& test_case);
  void WriteTestCaseExpectation(const TestExpectation& expectation);

  internal::RpcEventHandler handler_;
  RawServerWriter writer_;
  bool verbose_;
};

}  // namespace pw::unit_test
