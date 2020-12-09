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
"""Tests compiling and importing Python protos on the fly."""

from pathlib import Path
import tempfile
import unittest

from pw_protobuf_compiler import python_protos

PROTO_1 = """\
syntax = "proto3";

package pw.protobuf_compiler.test1;

message SomeMessage {
  uint32 magic_number = 1;
}

message AnotherMessage {
  enum Result {
    FAILED = 0;
    FAILED_MISERABLY = 1;
    I_DONT_WANT_TO_TALK_ABOUT_IT = 2;
  }

  Result result = 1;
  string payload = 2;
}

service PublicService {
  rpc Unary(SomeMessage) returns (AnotherMessage) {}
  rpc ServerStreaming(SomeMessage) returns (stream AnotherMessage) {}
  rpc ClientStreaming(stream SomeMessage) returns (AnotherMessage) {}
  rpc BidiStreaming(stream SomeMessage) returns (stream AnotherMessage) {}
}
"""

PROTO_2 = """\
syntax = "proto2";

package pw.protobuf_compiler.test2;

message Request {
  optional float magic_number = 1;
}

message Response {
}

service Alpha {
  rpc Unary(Request) returns (Response) {}
}

service Bravo {
  rpc BidiStreaming(stream Request) returns (stream Response) {}
}
"""

PROTO_3 = """\
syntax = "proto3";

package pw.protobuf_compiler.test2;

enum Greeting {
  YO = 0;
  HI = 1;
}

message Hello {
  repeated int64 value = 1;
  Greeting hi = 2;
}
"""


class TestCompileAndImport(unittest.TestCase):
    def setUp(self):
        self._proto_dir = tempfile.TemporaryDirectory(prefix='proto_test')
        self._protos = []

        for i, contents in enumerate([PROTO_1, PROTO_2, PROTO_3], 1):
            self._protos.append(Path(self._proto_dir.name, f'test_{i}.proto'))
            self._protos[-1].write_text(contents)

    def tearDown(self):
        self._proto_dir.cleanup()

    def test_compile_to_temp_dir_and_import(self):
        modules = {
            m.DESCRIPTOR.name: m
            for m in python_protos.compile_and_import(self._protos)
        }
        self.assertEqual(3, len(modules))

        # Make sure the protobuf modules contain what we expect them to.
        mod = modules['test_1.proto']
        self.assertEqual(
            4, len(mod.DESCRIPTOR.services_by_name['PublicService'].methods))

        mod = modules['test_2.proto']
        self.assertEqual(mod.Request(magic_number=1.5).magic_number, 1.5)
        self.assertEqual(2, len(mod.DESCRIPTOR.services_by_name))

        mod = modules['test_3.proto']
        self.assertEqual(mod.Hello(value=[123, 456]).value, [123, 456])


class TestProtoLibrary(TestCompileAndImport):
    """Tests the Library class."""
    def setUp(self):
        super().setUp()
        self._library = python_protos.Library(
            python_protos.compile_and_import(self._protos))

    def test_packages_can_access_messages(self):
        msg = self._library.packages.pw.protobuf_compiler.test1.SomeMessage
        self.assertEqual(msg(magic_number=123).magic_number, 123)

    def test_packages_finds_across_modules(self):
        msg = self._library.packages.pw.protobuf_compiler.test2.Request
        self.assertEqual(msg(magic_number=50).magic_number, 50)

        val = self._library.packages.pw.protobuf_compiler.test2.YO
        self.assertEqual(val, 0)

    def test_packages_invalid_name(self):
        with self.assertRaises(AttributeError):
            _ = self._library.packages.nothing

        with self.assertRaises(AttributeError):
            _ = self._library.packages.pw.NOT_HERE

        with self.assertRaises(AttributeError):
            _ = self._library.packages.pw.protobuf_compiler.test1.NotARealMsg

    def test_access_modules_by_package(self):
        test1 = self._library.modules_by_package['pw.protobuf_compiler.test1']
        self.assertEqual(len(test1), 1)
        self.assertEqual(test1[0].AnotherMessage.Result.Value('FAILED'), 0)

        test2 = self._library.modules_by_package['pw.protobuf_compiler.test2']
        self.assertEqual(len(test2), 2)

    def test_access_modules_by_package_unknown(self):
        with self.assertRaises(KeyError):
            _ = self._library.modules_by_package['pw.not_real']

    def test_library_from_strings(self):
        # Replace the package to avoid conflicts with the other proto imports
        new_protos = [
            p.replace('pw.protobuf_compiler', 'proto.library.test')
            for p in [PROTO_1, PROTO_2, PROTO_3]
        ]

        library = python_protos.Library.from_strings(new_protos)

        # Make sure we can safely import the same proto contents multiple times.
        library = python_protos.Library.from_strings(new_protos)

        msg = library.packages.proto.library.test.test2.Request
        self.assertEqual(msg(magic_number=50).magic_number, 50)

        val = library.packages.proto.library.test.test2.YO
        self.assertEqual(val, 0)

    def test_access_nested_packages_by_name(self):
        self.assertIs(self._library.packages['pw.protobuf_compiler.test1'],
                      self._library.packages.pw.protobuf_compiler.test1)
        self.assertIs(self._library.packages.pw['protobuf_compiler.test1'],
                      self._library.packages.pw.protobuf_compiler.test1)
        self.assertIs(self._library.packages.pw.protobuf_compiler['test1'],
                      self._library.packages.pw.protobuf_compiler.test1)

    def test_access_nested_packages_by_name_unknown_package(self):
        with self.assertRaises(KeyError):
            _ = self._library.packages['']

        with self.assertRaises(KeyError):
            _ = self._library.packages['.']

        with self.assertRaises(KeyError):
            _ = self._library.packages['protobuf_compiler.test1']

        with self.assertRaises(KeyError):
            _ = self._library.packages.pw['pw.protobuf_compiler.test1']

        with self.assertRaises(KeyError):
            _ = self._library.packages.pw.protobuf_compiler['not here']


if __name__ == '__main__':
    unittest.main()
