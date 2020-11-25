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
"""This module generates the code for raw pw_rpc services."""

import os
from typing import Iterable

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoNode, ProtoService, ProtoServiceMethod
from pw_protobuf.proto_tree import build_node_tree
from pw_rpc import codegen
from pw_rpc.codegen import RPC_NAMESPACE

PROTO_H_EXTENSION = '.pb.h'


def _proto_filename_to_generated_header(proto_file: str) -> str:
    """Returns the generated C++ RPC header name for a .proto file."""
    filename = os.path.splitext(proto_file)[0]
    return f'{filename}.raw_rpc{PROTO_H_EXTENSION}'


def _proto_filename_to_stub_header(proto_file: str) -> str:
    """Returns the generated C++ RPC header name for a .proto file."""
    filename = os.path.splitext(proto_file)[0]
    return f'{filename}.raw_rpc.stub{PROTO_H_EXTENSION}'


def _generate_method_descriptor(method: ProtoServiceMethod, method_id: int,
                                output: OutputFile) -> None:
    """Generates a method descriptor for a raw RPC method."""

    impl_method = f'&Implementation::{method.name()}'

    output.write_line(
        f'{RPC_NAMESPACE}::internal::GetRawMethodFor<{impl_method}, '
        f'{method.type().cc_enum()}>(')
    output.write_line(f'    0x{method_id:08x}),  // Hash of "{method.name()}"')


def _generate_server_writer_alias(output: OutputFile) -> None:
    output.write_line(
        f'using RawServerWriter = {RPC_NAMESPACE}::RawServerWriter;')


def _generate_code_for_client(unused_service: ProtoService,
                              unused_root: ProtoNode,
                              output: OutputFile) -> None:
    """Outputs client code for an RPC service."""
    output.write_line('// Raw RPC clients are not yet implemented.\n')


def _generate_code_for_service(service: ProtoService, root: ProtoNode,
                               output: OutputFile) -> None:
    """Generates a C++ base class for a raw RPC service."""
    codegen.service_class(service, root, output, _generate_server_writer_alias,
                          'RawMethodUnion', _generate_method_descriptor)


def _generate_code_for_package(proto_file, package: ProtoNode,
                               output: OutputFile) -> None:
    """Generates code for a header file corresponding to a .proto file."""
    includes = lambda *_: ['#include "pw_rpc/internal/raw_method_union.h"']

    codegen.package(proto_file, package, output, includes,
                    _generate_code_for_service, _generate_code_for_client)


def _unary_stub(method: ProtoServiceMethod, output: OutputFile) -> None:
    output.write_line(f'pw::StatusWithSize {method.name()}(ServerContext&, '
                      'pw::ConstByteSpan request, pw::ByteSpan response) {')

    with output.indent():
        output.write_line(codegen.STUB_REQUEST_TODO)
        output.write_line('static_cast<void>(request);')
        output.write_line(codegen.STUB_RESPONSE_TODO)
        output.write_line('static_cast<void>(response);')
        output.write_line('return pw::StatusWithSize::Unimplemented();')

    output.write_line('}')


def _server_streaming_stub(method: ProtoServiceMethod,
                           output: OutputFile) -> None:
    output.write_line(f'void {method.name()}(ServerContext&, '
                      'pw::ConstByteSpan request, RawServerWriter& writer) {')

    with output.indent():
        output.write_line(codegen.STUB_REQUEST_TODO)
        output.write_line('static_cast<void>(request);')
        output.write_line(codegen.STUB_WRITER_TODO)
        output.write_line('static_cast<void>(writer);')

    output.write_line('}')


def process_proto_file(proto_file) -> Iterable[OutputFile]:
    """Generates code for a single .proto file."""

    _, package_root = build_node_tree(proto_file)
    output_filename = _proto_filename_to_generated_header(proto_file.name)
    output_file = OutputFile(output_filename)
    _generate_code_for_package(proto_file, package_root, output_file)

    output_file.write_line()
    codegen.package_stubs(package_root, output_file, _unary_stub,
                          _server_streaming_stub)

    return [output_file]
