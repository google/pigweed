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
"""pw_protobuf compiler plugin.

This file implements a protobuf compiler plugin which generates C++ headers for
protobuf messages in the pw_protobuf format.
"""

import sys

from google.protobuf.compiler import plugin_pb2

from pw_protobuf import codegen_pwpb


def process_proto_request(req: plugin_pb2.CodeGeneratorRequest,
                          res: plugin_pb2.CodeGeneratorResponse) -> None:
    """Handles a protoc CodeGeneratorRequest message.

    Generates code for the files in the request and writes the output to the
    specified CodeGeneratorResponse message.

    Args:
      req: A CodeGeneratorRequest for a proto compilation.
      res: A CodeGeneratorResponse to populate with the plugin's output.
    """
    for proto_file in req.proto_file:
        output_files = codegen_pwpb.process_proto_file(proto_file)
        for output_file in output_files:
            fd = res.file.add()
            fd.name = output_file.name()
            fd.content = output_file.content()


def main() -> int:
    """Protobuf compiler plugin entrypoint.

    Reads a CodeGeneratorRequest proto from stdin and writes a
    CodeGeneratorResponse to stdout.
    """
    data = sys.stdin.buffer.read()
    request = plugin_pb2.CodeGeneratorRequest.FromString(data)
    response = plugin_pb2.CodeGeneratorResponse()
    process_proto_request(request, response)

    # Declare that this plugin supports optional fields in proto3.
    response.supported_features |= (  # type: ignore[attr-defined]
        response.FEATURE_PROTO3_OPTIONAL)  # type: ignore[attr-defined]

    sys.stdout.buffer.write(response.SerializeToString())
    return 0


if __name__ == '__main__':
    sys.exit(main())
