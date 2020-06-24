#!/usr/bin/env python3
# Copyright 2020 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Tests the generated pw_rpc code."""

from pathlib import Path
import os
import subprocess
import tempfile
import unittest

TEST_PROTO_FILE = b"""\
syntax = "proto3";

package pw.rpc.test;

message TestRequest {
  float integer = 1;
}

message TestResponse {
  int32 value = 1;
}

message TestStreamResponse {
  bytes chunk = 1;
}

message Empty {}

service TestService {
  rpc TestRpc(TestRequest) returns (TestResponse) {}
  rpc TestStreamRpc(Empty) returns (stream TestStreamResponse) {}
}
"""

EXPECTED_NANOPB_CODE = """\
#pragma once

#include <cstddef>
#include <cstdint>

#include "pw_rpc/internal/service.h"
#include "test.pb.h"


namespace pw::rpc::test {

class TestService : public ::pw::rpc::internal::Service {
 public:
  using ServerContext = ::pw::rpc::ServerContext;
  template <typename T> using ServerWriter = ::pw::rpc::ServerWriter<T>;

  constexpr TestService() : ::pw::rpc::internal::Service(kServiceId, kMethods) {}

  TestService(const TestService&) = delete;
  TestService& operator=(const TestService&) = delete;

  static ::pw::Status TestRpc(
      ServerContext& ctx,
      const pw_rpc_test_TestRequest& request,
      pw_rpc_test_TestResponse& response);

  static ::pw::Status TestStreamRpc(
      ServerContext& ctx,
      const pw_rpc_test_Empty& request,
      ServerWriter<pw_rpc_test_TestStreamResponse>& writer);

 private:
  // 65599 hash of "TestService".
  static constexpr uint32_t kServiceId = 0x105b6ac8;
  static constexpr char* kServiceName = "TestService";

  static constexpr std::array<::pw::rpc::internal::Method, 2> kMethods = {
      ::pw::rpc::internal::Method::Unary<TestRpc>(
          0xbc924054,  // 65599 hash of "TestRpc"
          pw_rpc_test_TestRequest_fields,
          pw_rpc_test_TestResponse_fields),
      ::pw::rpc::internal::Method::ServerStreaming<TestStreamRpc>(
          0xd97a28fa,  // 65599 hash of "TestStreamRpc"
          pw_rpc_test_Empty_fields,
          pw_rpc_test_TestStreamResponse_fields),
  };
};

}  // namespace pw::rpc::test
"""


class TestNanopbCodegen(unittest.TestCase):
    """Test case for nanopb code generation."""
    def setUp(self):
        self._output_dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        self._output_dir.cleanup()

    def test_nanopb_codegen(self):
        root = Path(os.getenv('PW_ROOT'))
        proto_dir = root / 'pw_rpc' / 'pw_rpc_test_protos'
        proto_file = proto_dir / 'test.proto'

        venv_bin = 'Scripts' if os.name == 'nt' else 'bin'
        plugin = root / '.python3-env' / venv_bin / 'pw_rpc_codegen'

        command = (
            'protoc',
            f'-I{proto_dir}',
            proto_file,
            '--plugin',
            f'protoc-gen-custom={plugin}',
            '--custom_out',
            self._output_dir.name,
        )

        subprocess.run(command)

        generated_files = os.listdir(self._output_dir.name)
        self.assertEqual(len(generated_files), 1)
        self.assertEqual(generated_files[0], 'test_rpc.pb.h')

        # Read the generated file, ignoring its preamble.
        generated_code = Path(self._output_dir.name,
                              generated_files[0]).read_text()
        generated_code = generated_code[generated_code.index('#pragma'):]

        self.assertEqual(generated_code, EXPECTED_NANOPB_CODE)
