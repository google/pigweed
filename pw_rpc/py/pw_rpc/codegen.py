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
"""Common RPC codegen utilities."""

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoService

import pw_rpc.ids


def method_lookup_table(service: ProtoService, output: OutputFile) -> None:
    """Generates array of method IDs for looking up methods at compile time."""
    output.write_line('static constexpr std::array<uint32_t, '
                      f'{len(service.methods())}> kMethodIds = {{')

    with output.indent(4):
        for method in service.methods():
            method_id = pw_rpc.ids.calculate(method.name())
            output.write_line(
                f'0x{method_id:08x},  // Hash of {method.name()}')

    output.write_line('};\n')
