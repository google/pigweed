# Copyright 2021 The Pigweed Authors
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
"""This module generates the code for raw pw_rpc services."""

import os
from typing import Iterable

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoServiceMethod
from pw_protobuf.proto_tree import build_node_tree
from pw_rpc import codegen
from pw_rpc.codegen import CodeGenerator, RPC_NAMESPACE

PROTO_H_EXTENSION = '.pb.h'


def _proto_filename_to_generated_header(proto_file: str) -> str:
    """Returns the generated C++ RPC header name for a .proto file."""
    filename = os.path.splitext(proto_file)[0]
    return f'{filename}.raw_rpc{PROTO_H_EXTENSION}'


def _proto_filename_to_stub_header(proto_file: str) -> str:
    """Returns the generated C++ RPC header name for a .proto file."""
    filename = os.path.splitext(proto_file)[0]
    return f'{filename}.raw_rpc.stub{PROTO_H_EXTENSION}'


class RawCodeGenerator(CodeGenerator):
    """Generates an RPC service and client using the raw buffers API."""
    def name(self) -> str:
        return 'raw'

    def method_union_name(self) -> str:
        return 'RawMethodUnion'

    def includes(self, unused_proto_file_name: str) -> Iterable[str]:
        yield '#include "pw_rpc/raw/internal/method_union.h"'

    def aliases(self) -> None:
        self.line(f'using RawServerWriter = {RPC_NAMESPACE}::RawServerWriter;')
        self.line(f'using RawServerReader = {RPC_NAMESPACE}::RawServerReader;')
        self.line('using RawServerReaderWriter = '
                  f'{RPC_NAMESPACE}::RawServerReaderWriter;')

    def method_descriptor(self, method: ProtoServiceMethod,
                          method_id: int) -> None:
        impl_method = f'&Implementation::{method.name()}'

        self.line(f'{RPC_NAMESPACE}::internal::GetRawMethodFor<{impl_method}, '
                  f'{method.type().cc_enum()}>(')
        self.line(f'    0x{method_id:08x}),  // Hash of "{method.name()}"')

    def client_member_function(self, method: ProtoServiceMethod) -> None:
        self.line('// Raw RPC clients are not yet implemented.')
        self.line(f'void {method.name()}();')

    def client_static_function(self, method: ProtoServiceMethod) -> None:
        self.line('// Raw RPC clients are not yet implemented.')
        self.line(f'static void {method.name()}();')

    def method_info_specialization(self, method: ProtoServiceMethod) -> None:
        pass


class StubGenerator(codegen.StubGenerator):
    def unary_signature(self, method: ProtoServiceMethod, prefix: str) -> str:
        return (f'pw::StatusWithSize {prefix}{method.name()}(ServerContext&, '
                'pw::ConstByteSpan request, pw::ByteSpan response)')

    def unary_stub(self, method: ProtoServiceMethod,
                   output: OutputFile) -> None:
        output.write_line(codegen.STUB_REQUEST_TODO)
        output.write_line('static_cast<void>(request);')
        output.write_line(codegen.STUB_RESPONSE_TODO)
        output.write_line('static_cast<void>(response);')
        output.write_line('return pw::StatusWithSize::Unimplemented();')

    def server_streaming_signature(self, method: ProtoServiceMethod,
                                   prefix: str) -> str:

        return (f'void {prefix}{method.name()}(ServerContext&, '
                'pw::ConstByteSpan request, RawServerWriter& writer)')

    def client_streaming_signature(self, method: ProtoServiceMethod,
                                   prefix: str) -> str:
        return (f'void {prefix}{method.name()}(ServerContext&, '
                'RawServerReader& reader)')

    def bidirectional_streaming_signature(self, method: ProtoServiceMethod,
                                          prefix: str) -> str:
        return (f'void {prefix}{method.name()}(ServerContext&, '
                'RawServerReaderWriter& reader_writer)')


def process_proto_file(proto_file) -> Iterable[OutputFile]:
    """Generates code for a single .proto file."""

    _, package_root = build_node_tree(proto_file)
    output_filename = _proto_filename_to_generated_header(proto_file.name)

    generator = RawCodeGenerator(output_filename)
    codegen.generate_package(proto_file, package_root, generator)

    codegen.package_stubs(package_root, generator.output, StubGenerator())

    return [generator.output]
