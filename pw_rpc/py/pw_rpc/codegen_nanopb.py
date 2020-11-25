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

import os
from typing import Iterable, Iterator

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoNode, ProtoService, ProtoServiceMethod
from pw_protobuf.proto_tree import build_node_tree
from pw_rpc import codegen
from pw_rpc.codegen import RPC_NAMESPACE
import pw_rpc.ids

PROTO_H_EXTENSION = '.pb.h'
PROTO_CC_EXTENSION = '.pb.cc'
NANOPB_H_EXTENSION = '.pb.h'


def _proto_filename_to_nanopb_header(proto_file: str) -> str:
    """Returns the generated nanopb header name for a .proto file."""
    return os.path.splitext(proto_file)[0] + NANOPB_H_EXTENSION


def _proto_filename_to_generated_header(proto_file: str) -> str:
    """Returns the generated C++ RPC header name for a .proto file."""
    filename = os.path.splitext(proto_file)[0]
    return f'{filename}.rpc{PROTO_H_EXTENSION}'


def _generate_method_descriptor(method: ProtoServiceMethod, method_id: int,
                                output: OutputFile) -> None:
    """Generates a nanopb method descriptor for an RPC method."""

    req_fields = f'{method.request_type().nanopb_name()}_fields'
    res_fields = f'{method.response_type().nanopb_name()}_fields'
    impl_method = f'&Implementation::{method.name()}'

    output.write_line(
        f'{RPC_NAMESPACE}::internal::GetNanopbOrRawMethodFor<{impl_method}, '
        f'{method.type().cc_enum()}>(')
    with output.indent(4):
        output.write_line(f'0x{method_id:08x},  // Hash of "{method.name()}"')
        output.write_line(f'{req_fields},')
        output.write_line(f'{res_fields}),')


def _generate_server_writer_alias(output: OutputFile) -> None:
    output.write_line('template <typename T>')
    output.write_line('using ServerWriter = ::pw::rpc::ServerWriter<T>;')


def _generate_code_for_service(service: ProtoService, root: ProtoNode,
                               output: OutputFile) -> None:
    """Generates a C++ derived class for a nanopb RPC service."""
    codegen.service_class(service, root, output, _generate_server_writer_alias,
                          'NanopbMethodUnion', _generate_method_descriptor)


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


def includes(proto_file, unused_package: ProtoNode) -> Iterator[str]:
    yield '#include "pw_rpc/internal/nanopb_method_union.h"'
    yield '#include "pw_rpc/nanopb_client_call.h"'

    # Include the corresponding nanopb header file for this proto file, in which
    # the file's messages and enums are generated. All other files imported from
    # the .proto file are #included in there.
    nanopb_header = _proto_filename_to_nanopb_header(proto_file.name)
    yield f'#include "{nanopb_header}"'


def _generate_code_for_package(proto_file, package: ProtoNode,
                               output: OutputFile) -> None:
    """Generates code for a header file corresponding to a .proto file."""

    codegen.package(proto_file, package, output, includes,
                    _generate_code_for_service, _generate_code_for_client)


def _unary_stub(method: ProtoServiceMethod, output: OutputFile) -> None:
    output.write_line(f'pw::Status {method.name()}(ServerContext&, '
                      f'const {method.request_type().nanopb_name()}& request, '
                      f'{method.response_type().nanopb_name()}& response) {{')

    with output.indent():
        output.write_line(codegen.STUB_REQUEST_TODO)
        output.write_line('static_cast<void>(request);')
        output.write_line(codegen.STUB_RESPONSE_TODO)
        output.write_line('static_cast<void>(response);')
        output.write_line('return pw::Status::Unimplemented();')

    output.write_line('}')


def _server_streaming_stub(method: ProtoServiceMethod,
                           output: OutputFile) -> None:
    output.write_line(
        f'void {method.name()}(ServerContext&, '
        f'const {method.request_type().nanopb_name()}& request, '
        f'ServerWriter<{method.response_type().nanopb_name()}>& writer) {{')

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
