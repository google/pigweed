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
from typing import Iterable

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoNode, ProtoServiceMethod
from pw_protobuf.proto_tree import build_node_tree
import pw_rpc.ids

PLUGIN_NAME = 'pw_rpc_codegen'
PLUGIN_VERSION = '0.1.0'

PROTO_H_EXTENSION = '.pb.h'
PROTO_CC_EXTENSION = '.pb.cc'
NANOPB_H_EXTENSION = '.pb.h'

RPC_NAMESPACE = '::pw::rpc'


def _invoker_name(method: ProtoServiceMethod) -> str:
    """Name for the function that invokes the user-defined service method.

    The generated class uses a different name for the methods than the derived
    class. If it used the same name, the invocation would appear to the compiler
    as a recursive function call if the derived class failed to define a method.
    """
    return 'Invoke_' + method.name()


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

    method_class = f'{RPC_NAMESPACE}::internal::Method'

    if method.type() == ProtoServiceMethod.Type.UNARY:
        func = f'{method_class}::Unary<{_invoker_name(method)}>'
    elif method.type() == ProtoServiceMethod.Type.SERVER_STREAMING:
        func = f'{method_class}::ServerStreaming<{_invoker_name(method)}>'
    else:
        raise NotImplementedError(
            'Only unary and server streaming RPCs are currently supported')

    method_id = pw_rpc.ids.calculate(method.name())
    req_fields = f'{method.request_type().nanopb_name()}_fields'
    res_fields = f'{method.response_type().nanopb_name()}_fields'

    output.write_line(f'{func}(')
    with output.indent(4):
        output.write_line(f'0x{method_id:08x},  // Hash of "{method.name()}"')
        output.write_line(f'{req_fields},')
        output.write_line(f'{res_fields}),')


def _generate_code_for_method(method: ProtoServiceMethod,
                              output: OutputFile) -> None:
    """Generates the function singature of a nanopb RPC method."""

    req_type = method.request_type().nanopb_name()
    res_type = method.response_type().nanopb_name()
    implementation_cast = 'static_cast<Implementation&>(call.service())'

    output.write_line()

    if method.type() == ProtoServiceMethod.Type.UNARY:
        output.write_line(f'static ::pw::Status {_invoker_name(method)}(')
        with output.indent(4):
            output.write_line('::pw::rpc::internal::ServerCall& call,')
            output.write_line(f'const {req_type}& request,')
            output.write_line(f'{res_type}& response) {{')
        with output.indent():
            output.write_line(f'return {implementation_cast}')
            output.write_line(
                f'    .{method.name()}(call.context(), request, response);')
        output.write_line('}')
    elif method.type() == ProtoServiceMethod.Type.SERVER_STREAMING:
        output.write_line(f'static void {_invoker_name(method)}(')
        with output.indent(4):
            output.write_line('::pw::rpc::internal::ServerCall& call,')
            output.write_line(f'const {req_type}& request,')
            output.write_line(f'ServerWriter<{res_type}>& writer) {{')
        with output.indent():
            output.write_line(implementation_cast)
            output.write_line(
                f'    .{method.name()}(call.context(), request, writer);')
        output.write_line('}')
    else:
        raise NotImplementedError(
            'Only unary and server streaming RPCs are currently supported')


def _generate_method_lookup_function(service: ProtoNode, output: OutputFile):
    """Generates a function that gets the Method from a function pointer."""
    output.write_line('template <auto impl_method>')
    output.write_line(
        'static constexpr const ::pw::rpc::internal::Method* MethodFor() {')

    with output.indent():
        for i, method in enumerate(service.methods()):
            output.write_line(
                'if constexpr (std::is_same_v<decltype(impl_method), '
                f'decltype(&Implementation::{method.name()})>) {{')

            with output.indent():
                output.write_line(
                    'if constexpr ('
                    f'impl_method == &Implementation::{method.name()}) {{')
                output.write_line(f'  return &std::get<{i}>(kMethods);')
                output.write_line('}')

            output.write_line('}')

        output.write_line('return nullptr;')

    output.write_line('}')


def _generate_code_for_service(service: ProtoNode, root: ProtoNode,
                               output: OutputFile) -> None:
    """Generates a C++ derived class for a nanopb RPC service."""

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

    service_name_hash = pw_rpc.ids.calculate(service.proto_path())
    output.write_line('\n private:')

    with output.indent():
        output.write_line(f'// Hash of "{service.proto_path()}".')
        output.write_line(
            f'static constexpr uint32_t kServiceId = 0x{service_name_hash:08x};'
        )

        for method in service.methods():
            _generate_code_for_method(method, output)

        output.write_line()

        # Generate the method table
        output.write_line(
            f'static constexpr std::array<{RPC_NAMESPACE}::internal::Method,'
            f' {len(service.methods())}> kMethods = {{')

        with output.indent(4):
            for method in service.methods():
                _generate_method_descriptor(method, output)

        output.write_line('};\n')

        _generate_method_lookup_function(service, output)

        output.write_line()
        output.write_line('template <auto>')
        output.write_line(
            'friend class ::pw::rpc::internal::ServiceMethodTraits;')

    output.write_line('};')


def generate_code_for_package(file_descriptor_proto, package: ProtoNode,
                              output: OutputFile) -> None:
    """Generates code for a header file corresponding to a .proto file."""

    assert package.type() == ProtoNode.Type.PACKAGE

    output.write_line(f'// {os.path.basename(output.name())} automatically '
                      f'generated by {PLUGIN_NAME} {PLUGIN_VERSION}')
    output.write_line(f'// on {datetime.now()}')
    output.write_line('// clang-format off')
    output.write_line('#pragma once\n')
    output.write_line('#include <array>')
    output.write_line('#include <cstddef>')
    output.write_line('#include <cstdint>')
    output.write_line('#include <type_traits>\n')
    output.write_line('#include "pw_rpc/internal/method.h"')
    output.write_line('#include "pw_rpc/server_context.h"')
    output.write_line('#include "pw_rpc/service.h"')

    # Include the corresponding nanopb header file for this proto file, in which
    # the file's messages and enums are generated. All other files imported from
    # the .proto file are #included in there.
    nanopb_header = _proto_filename_to_nanopb_header(
        file_descriptor_proto.name)
    output.write_line(f'#include "{nanopb_header}"\n')

    output.write_line('namespace pw::rpc::internal {\n')
    output.write_line('template <auto>')
    output.write_line('class ServiceMethodTraits;')
    output.write_line('\n}  // namespace pw::rpc::internal\n')

    if package.cpp_namespace():
        file_namespace = package.cpp_namespace()
        if file_namespace.startswith('::'):
            file_namespace = file_namespace[2:]

        output.write_line(f'namespace {file_namespace} {{')

    output.write_line('namespace generated {')

    for node in package:
        if node.type() == ProtoNode.Type.SERVICE:
            _generate_code_for_service(node, package, output)

    output.write_line('\n}  // namespace generated')

    if package.cpp_namespace():
        output.write_line(f'}}  // namespace {file_namespace}')


def process_proto_file(proto_file) -> Iterable[OutputFile]:
    """Generates code for a single .proto file."""

    _, package_root = build_node_tree(proto_file)
    output_filename = _proto_filename_to_generated_header(proto_file.name)
    output_file = OutputFile(output_filename)
    generate_code_for_package(proto_file, package_root, output_file)

    return [output_file]
