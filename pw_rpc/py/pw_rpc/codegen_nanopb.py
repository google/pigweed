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
"""This module generates the code for nanopb-based pw_rpc services."""

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
PROTO_CC_EXTENSION = '.pb.cc'
NANOPB_H_EXTENSION = '.pb.h'

RPC_NAMESPACE = '::pw::rpc'


def _proto_filename_to_nanopb_header(proto_file: str) -> str:
    """Returns the generated nanopb header name for a .proto file."""
    return os.path.splitext(proto_file)[0] + NANOPB_H_EXTENSION


def _proto_filename_to_generated_header(proto_file: str) -> str:
    """Returns the generated C++ RPC header name for a .proto file."""
    filename = os.path.splitext(proto_file)[0]
    return f'{filename}.rpc{PROTO_H_EXTENSION}'


def _generate_method_descriptor(method: ProtoServiceMethod,
                                output: OutputFile) -> None:
    """Generates a nanopb method descriptor for an RPC method."""

    method_id = pw_rpc.ids.calculate(method.name())
    req_fields = f'{method.request_type().nanopb_name()}_fields'
    res_fields = f'{method.response_type().nanopb_name()}_fields'
    impl_method = f'&Implementation::{method.name()}'

    output.write_line(
        f'{RPC_NAMESPACE}::internal::GetNanopbOrRawMethodFor<{impl_method}>(')
    with output.indent(4):
        output.write_line(f'0x{method_id:08x},  // Hash of "{method.name()}"')
        output.write_line(f'{req_fields},')
        output.write_line(f'{res_fields}),')


def _generate_method_lookup_function(output: OutputFile):
    """Generates a function that gets a Method object from its ID."""
    nanopb_method = f'{RPC_NAMESPACE}::internal::NanopbMethod'

    output.write_line(
        f'static constexpr const {nanopb_method}* NanopbMethodFor(')
    output.write_line('    uint32_t id) {')

    with output.indent():
        output.write_line('for (auto& method : kMethods) {')
        with output.indent():
            output.write_line('if (method.nanopb_method().id() == id) {')
            output.write_line(
                f'  return &static_cast<const {nanopb_method}&>(')
            output.write_line('    method.nanopb_method());')
            output.write_line('}')
        output.write_line('}')

        output.write_line('return nullptr;')

    output.write_line('}')


def _generate_code_for_service(service: ProtoService, root: ProtoNode,
                               output: OutputFile) -> None:
    """Generates a C++ derived class for a nanopb RPC service."""

    output.write_line('namespace generated {')

    base_class = f'{RPC_NAMESPACE}::Service'
    output.write_line('\ntemplate <typename Implementation>')
    output.write_line(
        f'class {service.cpp_namespace(root)} : public {base_class} {{')
    output.write_line(' public:')

    with output.indent():
        output.write_line(
            f'using ServerContext = {RPC_NAMESPACE}::ServerContext;')
        output.write_line('template <typename T>')
        output.write_line(
            f'using ServerWriter = {RPC_NAMESPACE}::ServerWriter<T>;')
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
                          f'{RPC_NAMESPACE}::internal::NanopbMethodUnion,'
                          f' {len(service.methods())}> kMethods = {{')

        with output.indent(4):
            for method in service.methods():
                _generate_method_descriptor(method, output)

        output.write_line('};')

    output.write_line('};')

    output.write_line('\n}  // namespace generated\n')


def _generate_code_for_client_method(method: ProtoServiceMethod,
                                     output: OutputFile) -> None:
    """Outputs client code for a single RPC method."""

    req = method.request_type().nanopb_name()
    res = method.response_type().nanopb_name()
    method_id = pw_rpc.ids.calculate(method.name())

    if method.type() == ProtoServiceMethod.Type.UNARY:
        callback = f'{RPC_NAMESPACE}::UnaryResponseHandler<{res}>'
    elif method.type() == ProtoServiceMethod.Type.SERVER_STREAMING:
        callback = f'{RPC_NAMESPACE}::ServerStreamingResponseHandler<{res}>'
    else:
        raise NotImplementedError(
            'Only unary and server streaming RPCs are currently supported')

    output.write_line()
    output.write_line(f'static NanopbClientCall<\n    {callback}>')
    output.write_line(f'{method.name()}({RPC_NAMESPACE}::Channel& channel,')
    with output.indent(len(method.name()) + 1):
        output.write_line(f'const {req}& request,')
        output.write_line(f'{callback}& callback) {{')

    with output.indent():
        output.write_line(f'NanopbClientCall<{callback}>')
        output.write_line('    call(&channel,')
        with output.indent(9):
            output.write_line('kServiceId,')
            output.write_line(
                f'0x{method_id:08x},  // Hash of "{method.name()}"')
            output.write_line('callback,')
            output.write_line(f'{req}_fields,')
            output.write_line(f'{res}_fields);')
        output.write_line('call.SendRequest(&request);')
        output.write_line('return call;')

    output.write_line('}')


def _generate_code_for_client(service: ProtoService, root: ProtoNode,
                              output: OutputFile) -> None:
    """Outputs client code for an RPC service."""

    output.write_line('namespace nanopb {')

    class_name = f'{service.cpp_namespace(root)}Client'
    output.write_line(f'\nclass {class_name} {{')
    output.write_line(' public:')

    with output.indent():
        output.write_line('template <typename T>')
        output.write_line(
            f'using NanopbClientCall = {RPC_NAMESPACE}::NanopbClientCall<T>;')

        output.write_line('')
        output.write_line(f'{class_name}() = delete;')

        for method in service.methods():
            _generate_code_for_client_method(method, output)

    service_name_hash = pw_rpc.ids.calculate(service.proto_path())
    output.write_line('\n private:')

    with output.indent():
        output.write_line(f'// Hash of "{service.proto_path()}".')
        output.write_line(
            f'static constexpr uint32_t kServiceId = 0x{service_name_hash:08x};'
        )

    output.write_line('};')

    output.write_line('\n}  // namespace nanopb\n')


def generate_code_for_package(file_descriptor_proto, package: ProtoNode,
                              output: OutputFile) -> None:
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
    output.write_line('#include "pw_rpc/internal/nanopb_method_union.h"')
    output.write_line('#include "pw_rpc/nanopb_client_call.h"')
    output.write_line('#include "pw_rpc/server_context.h"')
    output.write_line('#include "pw_rpc/service.h"')

    # Include the corresponding nanopb header file for this proto file, in which
    # the file's messages and enums are generated. All other files imported from
    # the .proto file are #included in there.
    nanopb_header = _proto_filename_to_nanopb_header(
        file_descriptor_proto.name)
    output.write_line(f'#include "{nanopb_header}"\n')

    if package.cpp_namespace():
        file_namespace = package.cpp_namespace()
        if file_namespace.startswith('::'):
            file_namespace = file_namespace[2:]

        output.write_line(f'namespace {file_namespace} {{')

    for node in package:
        if node.type() == ProtoNode.Type.SERVICE:
            _generate_code_for_service(cast(ProtoService, node), package,
                                       output)
            _generate_code_for_client(cast(ProtoService, node), package,
                                      output)

    if package.cpp_namespace():
        output.write_line(f'}}  // namespace {file_namespace}')


def process_proto_file(proto_file) -> Iterable[OutputFile]:
    """Generates code for a single .proto file."""

    _, package_root = build_node_tree(proto_file)
    output_filename = _proto_filename_to_generated_header(proto_file.name)
    output_file = OutputFile(output_filename)
    generate_code_for_package(proto_file, package_root, output_file)

    return [output_file]
