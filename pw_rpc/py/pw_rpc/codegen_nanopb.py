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
"""This module generates the code for nanopb-based pw_rpc services."""

import os
from typing import Iterable, NamedTuple, Optional

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoServiceMethod
from pw_protobuf.proto_tree import build_node_tree
from pw_rpc import codegen
from pw_rpc.codegen import CodeGenerator, RPC_NAMESPACE
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


class NanopbCodeGenerator(CodeGenerator):
    """Generates an RPC service and client using the Nanopb API."""
    def name(self) -> str:
        return 'nanopb'

    def method_union_name(self) -> str:
        return 'NanopbMethodUnion'

    def includes(self, proto_file_name: str) -> Iterable[str]:
        yield '#include "pw_rpc/nanopb/internal/method_union.h"'
        yield '#include "pw_rpc/nanopb/client_call.h"'

        # Include the corresponding nanopb header file for this proto file, in
        # which the file's messages and enums are generated. All other files
        # imported from the .proto file are #included in there.
        nanopb_header = _proto_filename_to_nanopb_header(proto_file_name)
        yield f'#include "{nanopb_header}"'

    def aliases(self) -> None:
        self.line('template <typename Response>')
        self.line('using ServerWriter = '
                  f'{RPC_NAMESPACE}::NanopbServerWriter<Response>;')
        self.line('template <typename Request, typename Response>')
        self.line('using ServerReader = '
                  f'{RPC_NAMESPACE}::NanopbServerReader<Request, Response>;')
        self.line('template <typename Request, typename Response>')
        self.line(
            'using ServerReaderWriter = '
            f'{RPC_NAMESPACE}::NanopbServerReaderWriter<Request, Response>;')

    def method_descriptor(self, method: ProtoServiceMethod,
                          method_id: int) -> None:
        req_fields = f'{method.request_type().nanopb_name()}_fields'
        res_fields = f'{method.response_type().nanopb_name()}_fields'
        impl_method = f'&Implementation::{method.name()}'

        self.line(f'{RPC_NAMESPACE}::internal::'
                  f'GetNanopbOrRawMethodFor<{impl_method}, '
                  f'{method.type().cc_enum()}, '
                  f'{method.request_type().nanopb_name()}, '
                  f'{method.response_type().nanopb_name()}>(')
        with self.indent(4):
            self.line(f'0x{method_id:08x},  // Hash of "{method.name()}"')
            self.line(f'{req_fields},')
            self.line(f'{res_fields}),')

    def client_member_function(self, method: ProtoServiceMethod) -> None:
        """Outputs client code for a single RPC method."""

        if method.type() in (ProtoServiceMethod.Type.CLIENT_STREAMING,
                             ProtoServiceMethod.Type.BIDIRECTIONAL_STREAMING):
            self.line('// Nanopb RPC clients for '
                      f'{method.type().name.lower().replace("_", " ")} '
                      'methods are not yet supported.')
            self.line('// See pwbug/428 (http://bugs.pigweed.dev/428).')
            # TODO(pwbug/428): Support client & bidirectional streaming clients.
            return

        req = method.request_type().nanopb_name()
        res = method.response_type().nanopb_name()
        method_id = pw_rpc.ids.calculate(method.name())

        callbacks, functions, moved_functions = _client_functions(method)

        call_alias = f'{method.name()}Call'

        moved_functions = list(f'std::move({function.name})'
                               for function in functions)

        self.line(f'using {call_alias} = {RPC_NAMESPACE}::NanopbClientCall<')
        self.line(f'    {callbacks}<{res}>>;')
        self.line()

        # TODO(frolv): Deprecate this channel-based API.
        # ======== Deprecated API ========
        self.line('// This function is DEPRECATED. Use pw_rpc::nanopb::'
                  f'{method.service().name()}::{method.name()}() instead.')
        self.line(f'static {call_alias} {method.name()}(')
        with self.indent(4):
            self.line(f'{RPC_NAMESPACE}::Channel& channel,')
            self.line(f'const {req}& request,')

            # Write out each of the callback functions for the method type.
            for i, function in enumerate(functions):
                if i == len(functions) - 1:
                    self.line(f'{function}) {{')
                else:
                    self.line(f'{function},')

        with self.indent():
            self.line(f'{call_alias} call(&channel,')
            with self.indent(len(call_alias) + 6):
                self.line('kServiceId,')
                self.line(f'0x{method_id:08x},  // Hash of "{method.name()}"')
                self.line(f'{callbacks}({", ".join(moved_functions)}),')
                self.line(f'{req}_fields,')
                self.line(f'{res}_fields);')
            self.line('call.SendRequest(&request);')
            self.line('return call;')

        self.line('}')
        self.line()

        # ======== End deprecated API ========

        self.line(f'{call_alias} {method.name()}(')
        with self.indent(4):
            self.line(f'const {req}& request,')

            # Write out each of the callback functions for the method type.
            for i, function in enumerate(functions):
                if i == len(functions) - 1:
                    self.line(f'{function}) {{')
                else:
                    self.line(f'{function},')

        with self.indent():
            self.line()
            self.line(f'{call_alias} call(&client(),')
            with self.indent(len(call_alias) + 6):
                self.line('channel_id(),')
                self.line('kServiceId,')
                self.line(f'0x{method_id:08x},  // Hash of "{method.name()}"')
                self.line(f'{callbacks}({", ".join(moved_functions)}),')
                self.line(f'{req}_fields,')
                self.line(f'{res}_fields);')

            # Unary and server streaming RPCs send initial request immediately.
            if method.type() in (ProtoServiceMethod.Type.UNARY,
                                 ProtoServiceMethod.Type.SERVER_STREAMING):
                self.line()
                self.line('if (::pw::Status status = '
                          'call.SendRequest(&request); !status.ok()) {')
                with self.indent():
                    self.line('call.callbacks().InvokeRpcError(status);')
                self.line('}')
                self.line()

            self.line('return call;')

        self.line('}')
        self.line()

    def client_static_function(self, method: ProtoServiceMethod) -> None:
        if method.type() in (ProtoServiceMethod.Type.CLIENT_STREAMING,
                             ProtoServiceMethod.Type.BIDIRECTIONAL_STREAMING):
            self.line('// Nanopb RPC clients for '
                      f'{method.type().name.lower().replace("_", " ")} '
                      'methods are not yet supported.')
            self.line('// See pwbug/428 (http://bugs.pigweed.dev/428).')
            self.line(f'static void {method.name()}();')
            # TODO(pwbug/428): Support client & bidirectional streaming clients.
            return

        req = method.request_type().nanopb_name()

        _, functions, moved_functions = _client_functions(method)

        self.line(f'using {method.name()}Call = Client::{method.name()}Call;')
        self.line()

        self.line(f'static {method.name()}Call {method.name()}(')

        with self.indent(4):
            self.line(f'{RPC_NAMESPACE}::Client& client,')
            self.line('uint32_t channel_id,')
            self.line(f'const {req}& request,')

            # Write out each of the callback functions for the method type.
            for i, function in enumerate(functions):
                if i == len(functions) - 1:
                    self.line(f'{function}) {{')
                else:
                    self.line(f'{function},')

        with self.indent():
            self.line(f'return Client(client, channel_id).{method.name()}(')
            self.line(f'    request, {", ".join(moved_functions)});')

        self.line('}')

    def method_info_specialization(self, method: ProtoServiceMethod) -> None:
        self.line()
        self.line(f'using Request = {method.request_type().nanopb_name()};')
        self.line(f'using Response = {method.response_type().nanopb_name()};')
        self.line()
        self.line(f'static constexpr {RPC_NAMESPACE}::internal::'
                  'NanopbMethodSerde serde() {')

        with self.indent():
            self.line('return {'
                      f'{method.request_type().nanopb_name()}_fields, '
                      f'{method.response_type().nanopb_name()}_fields}};')

        self.line('}')


def _client_functions(method: ProtoServiceMethod) -> tuple:
    res = method.response_type().nanopb_name()

    rpc_error = _CallbackFunction(
        'void(::pw::Status)',
        'on_rpc_error',
        'nullptr',
    )

    if method.type() == ProtoServiceMethod.Type.UNARY:
        callbacks = f'{RPC_NAMESPACE}::internal::UnaryCallbacks'
        functions = [
            _CallbackFunction(f'void(const {res}&, ::pw::Status)',
                              'on_response'),
            rpc_error,
        ]
    elif method.type() == ProtoServiceMethod.Type.SERVER_STREAMING:
        callbacks = f'{RPC_NAMESPACE}::internal::ServerStreamingCallbacks'
        functions = [
            _CallbackFunction(f'void(const {res}&)', 'on_response'),
            _CallbackFunction('void(::pw::Status)', 'on_stream_end'),
            rpc_error,
        ]
    else:
        raise NotImplementedError

    moved_functions = list(f'std::move({function.name})'
                           for function in functions)

    return callbacks, functions, moved_functions


class _CallbackFunction(NamedTuple):
    """Represents a callback function parameter in a client RPC call."""

    function_type: str
    name: str
    default_value: Optional[str] = None

    def __str__(self):
        param = f'::pw::Function<{self.function_type}> {self.name}'
        if self.default_value:
            param += f' = {self.default_value}'
        return param


class StubGenerator(codegen.StubGenerator):
    """Generates Nanopb RPC stubs."""
    def unary_signature(self, method: ProtoServiceMethod, prefix: str) -> str:
        return (f'::pw::Status {prefix}{method.name()}(ServerContext&, '
                f'const {method.request_type().nanopb_name()}& request, '
                f'{method.response_type().nanopb_name()}& response)')

    def unary_stub(self, method: ProtoServiceMethod,
                   output: OutputFile) -> None:
        output.write_line(codegen.STUB_REQUEST_TODO)
        output.write_line('static_cast<void>(request);')
        output.write_line(codegen.STUB_RESPONSE_TODO)
        output.write_line('static_cast<void>(response);')
        output.write_line('return ::pw::Status::Unimplemented();')

    def server_streaming_signature(self, method: ProtoServiceMethod,
                                   prefix: str) -> str:
        return (
            f'void {prefix}{method.name()}(ServerContext&, '
            f'const {method.request_type().nanopb_name()}& request, '
            f'ServerWriter<{method.response_type().nanopb_name()}>& writer)')

    def client_streaming_signature(self, method: ProtoServiceMethod,
                                   prefix: str) -> str:
        return (f'void {prefix}{method.name()}(ServerContext&, '
                f'ServerReader<{method.request_type().nanopb_name()}, '
                f'{method.response_type().nanopb_name()}>& reader)')

    def bidirectional_streaming_signature(self, method: ProtoServiceMethod,
                                          prefix: str) -> str:
        return (f'void {prefix}{method.name()}(ServerContext&, '
                f'ServerReaderWriter<{method.request_type().nanopb_name()}, '
                f'{method.response_type().nanopb_name()}>& reader_writer)')


def process_proto_file(proto_file) -> Iterable[OutputFile]:
    """Generates code for a single .proto file."""

    _, package_root = build_node_tree(proto_file)
    output_filename = _proto_filename_to_generated_header(proto_file.name)
    generator = NanopbCodeGenerator(output_filename)
    codegen.generate_package(proto_file, package_root, generator)

    codegen.package_stubs(package_root, generator.output, StubGenerator())

    return [generator.output]
