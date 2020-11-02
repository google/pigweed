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

from datetime import datetime
import os
from typing import Iterable, cast

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoNode, ProtoService, ProtoServiceMethod
from pw_protobuf.proto_tree import build_node_tree
import pw_rpc.ids

PLUGIN_NAME = 'pw_rpc_codegen'
PLUGIN_VERSION = '0.1.0'

PROTO_H_EXTENSION = '.pb.h'

RPC_NAMESPACE = '::pw::rpc'


def _proto_filename_to_generated_header(proto_file: str) -> str:
    """Returns the generated C++ RPC header name for a .proto file."""
    filename = os.path.splitext(proto_file)[0]
    return f'{filename}.raw_rpc{PROTO_H_EXTENSION}'


def _generate_method_descriptor(method: ProtoServiceMethod,
                                output: OutputFile) -> None:
    """Generates a method descriptor for a raw RPC method."""

    method_id = pw_rpc.ids.calculate(method.name())
    impl_method = f'&Implementation::{method.name()}'

    output.write_line(
        f'{RPC_NAMESPACE}::internal::GetRawMethodFor<{impl_method}>(')
    output.write_line(f'    0x{method_id:08x}),  // Hash of "{method.name()}"')


def _generate_method_lookup_function(output: OutputFile):
    """Generates a function that gets a Method object from its ID."""
    raw_method = f'{RPC_NAMESPACE}::internal::RawMethod'

    output.write_line(f'static constexpr const {raw_method}* RawMethodFor(')
    output.write_line('    uint32_t id) {')

    with output.indent():
        output.write_line('for (auto& method : kMethods) {')
        with output.indent():
            output.write_line('if (method.raw_method().id() == id) {')
            output.write_line(f'  return &static_cast<const {raw_method}&>(')
            output.write_line('    method.raw_method());')
            output.write_line('}')
        output.write_line('}')

        output.write_line('return nullptr;')

    output.write_line('}')


def _generate_code_for_service(service: ProtoService, root: ProtoNode,
                               output: OutputFile) -> None:
    """Generates a C++ base class for a raw RPC service."""

    base_class = f'{RPC_NAMESPACE}::Service'
    output.write_line('\ntemplate <typename Implementation>')
    output.write_line(
        f'class {service.cpp_namespace(root)} : public {base_class} {{')
    output.write_line(' public:')

    with output.indent():
        output.write_line(
            f'using ServerContext = {RPC_NAMESPACE}::ServerContext;')
        output.write_line(
            f'using RawServerWriter = {RPC_NAMESPACE}::RawServerWriter;')
        output.write_line()

        output.write_line(f'constexpr {service.name()}()')
        output.write_line(f'    : {base_class}(kServiceId, kMethods) {{}}')

        output.write_line()
        output.write_line(
            f'{service.name()}(const {service.name()}&) = delete;')
        output.write_line(f'{service.name()}& operator='
                          f'(const {service.name()}&) = delete;')

        output.write_line()
        output.write_line(f'static constexpr const char* name() '
                          f'{{ return "{service.name()}"; }}')

        output.write_line()
        output.write_line(
            '// Used by ServiceMethodTraits to identify a base service.')
        output.write_line(
            'constexpr void _PwRpcInternalGeneratedBase() const {}')

        output.write_line()
        _generate_method_lookup_function(output)

    service_name_hash = pw_rpc.ids.calculate(service.proto_path())
    output.write_line('\n private:')

    with output.indent():
        output.write_line(f'// Hash of "{service.proto_path()}".')
        output.write_line(
            f'static constexpr uint32_t kServiceId = 0x{service_name_hash:08x};'
        )

        output.write_line()

        # Generate the method table
        output.write_line('static constexpr std::array<'
                          f'{RPC_NAMESPACE}::internal::RawMethodUnion,'
                          f' {len(service.methods())}> kMethods = {{')

        with output.indent(4):
            for method in service.methods():
                _generate_method_descriptor(method, output)

        output.write_line('};')

    output.write_line('};')


def _generate_code_for_package(package: ProtoNode, output: OutputFile) -> None:
    """Generates code for a header file corresponding to a .proto file."""
    assert package.type() == ProtoNode.Type.PACKAGE

    output.write_line(f'// {os.path.basename(output.name())} automatically '
                      f'generated by {PLUGIN_NAME} {PLUGIN_VERSION}')
    output.write_line(f'// on {datetime.now().isoformat()}')
    output.write_line('// clang-format off')
    output.write_line('#pragma once\n')
    output.write_line('#include <array>')
    output.write_line('#include <cstddef>')
    output.write_line('#include <cstdint>')
    output.write_line('#include <type_traits>\n')
    output.write_line('#include "pw_rpc/internal/raw_method_union.h"')
    output.write_line('#include "pw_rpc/server_context.h"')
    output.write_line('#include "pw_rpc/service.h"\n')

    if package.cpp_namespace():
        file_namespace = package.cpp_namespace()
        if file_namespace.startswith('::'):
            file_namespace = file_namespace[2:]

        output.write_line(f'namespace {file_namespace} {{')

    output.write_line('namespace generated {')

    for node in package:
        if node.type() == ProtoNode.Type.SERVICE:
            _generate_code_for_service(cast(ProtoService, node), package,
                                       output)

    output.write_line('\n}  // namespace generated')

    if package.cpp_namespace():
        output.write_line(f'}}  // namespace {file_namespace}')


def process_proto_file(proto_file) -> Iterable[OutputFile]:
    """Generates code for a single .proto file."""

    _, package_root = build_node_tree(proto_file)
    output_filename = _proto_filename_to_generated_header(proto_file.name)
    output_file = OutputFile(output_filename)
    _generate_code_for_package(package_root, output_file)

    return [output_file]
