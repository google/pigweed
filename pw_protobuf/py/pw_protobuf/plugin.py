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
from argparse import ArgumentParser, Namespace
from pathlib import Path
from shlex import shlex

from google.protobuf.compiler import plugin_pb2

from pw_protobuf import codegen_pwpb, edition_constants, options


def parse_parameter_options(parameter: str) -> Namespace:
    """Parses parameters passed through from protoc.

    These parameters come in via passing `--${NAME}_out` parameters to protoc,
    where protoc-gen-${NAME} is the supplied name of the plugin. At time of
    writing, Blaze uses --pwpb_opt, whereas the script for GN uses --custom_opt.
    """
    parser = ArgumentParser()
    parser.add_argument(
        '-I',
        '--include-path',
        dest='include_paths',
        metavar='DIR',
        action='append',
        default=[],
        type=Path,
        help='Append DIR to options file search path',
    )
    parser.add_argument(
        '--no-legacy-namespace',
        dest='no_legacy_namespace',
        action='store_true',
        help='If set, suppresses `using namespace` declarations, which '
        'disallows use of the legacy non-prefixed namespace',
    )
    parser.add_argument(
        '--exclude-legacy-snake-case-field-name-enums',
        dest='exclude_legacy_snake_case_field_name_enums',
        action='store_true',
        help='Do not generate legacy SNAKE_CASE names for field name enums.',
    )
    parser.add_argument(
        '--options-file',
        dest='options_files',
        metavar='FILE',
        action='append',
        default=[],
        type=Path,
        help='Append FILE to options file list',
    )
    parser.add_argument(
        '--no-oneof-callbacks',
        dest='oneof_callbacks',
        action='store_false',
        help='Generate legacy inline oneof members instead of callbacks',
    )
    parser.add_argument(
        '--no-generic-options-files',
        dest='generic_options_files',
        action='store_false',
        help='If set, only permits the usage of the `.pwpb_options` extension '
        'for options files instead of the generic `.options`',
    )

    # protoc passes the custom arguments in shell quoted form, separated by
    # commas. Use shlex to split them, correctly handling quoted sections, with
    # equivalent options to IFS=","
    lex = shlex(parameter)
    lex.whitespace_split = True
    lex.whitespace = ','
    lex.commenters = ''
    args = list(lex)

    return parser.parse_args(args)


def process_proto_request(
    req: plugin_pb2.CodeGeneratorRequest, res: plugin_pb2.CodeGeneratorResponse
) -> bool:
    """Handles a protoc CodeGeneratorRequest message.

    Generates code for the files in the request and writes the output to the
    specified CodeGeneratorResponse message.

    Args:
      req: A CodeGeneratorRequest for a proto compilation.
      res: A CodeGeneratorResponse to populate with the plugin's output.
    """

    success = True

    args = parse_parameter_options(req.parameter)
    for proto_file in req.proto_file:
        proto_options = options.load_options(
            args.include_paths,
            Path(proto_file.name),
            args.options_files,
            allow_generic_options_extension=args.generic_options_files,
        )

        codegen_options = codegen_pwpb.GeneratorOptions(
            oneof_callbacks=args.oneof_callbacks,
            suppress_legacy_namespace=args.no_legacy_namespace,
            exclude_legacy_snake_case_field_name_enums=(
                args.exclude_legacy_snake_case_field_name_enums
            ),
        )

        output_files = codegen_pwpb.process_proto_file(
            proto_file,
            proto_options,
            codegen_options,
        )

        if output_files is not None:
            for output_file in output_files:
                fd = res.file.add()
                fd.name = output_file.name()
                fd.content = output_file.content()
        else:
            success = False

    return success


def main() -> int:
    """Protobuf compiler plugin entrypoint.

    Reads a CodeGeneratorRequest proto from stdin and writes a
    CodeGeneratorResponse to stdout.
    """
    data = sys.stdin.buffer.read()
    request = plugin_pb2.CodeGeneratorRequest.FromString(data)
    response = plugin_pb2.CodeGeneratorResponse()

    # Declare that this plugin supports optional fields in proto3.
    response.supported_features |= (  # type: ignore[attr-defined]
        response.FEATURE_PROTO3_OPTIONAL
    )  # type: ignore[attr-defined]

    response.supported_features |= edition_constants.FEATURE_SUPPORTS_EDITIONS

    if hasattr(response, 'minimum_edition'):
        response.minimum_edition = (  # type: ignore[attr-defined]
            edition_constants.Edition.EDITION_PROTO2.value
        )
        response.maximum_edition = (  # type: ignore[attr-defined]
            edition_constants.Edition.EDITION_2023.value
        )

    if not process_proto_request(request, response):
        print('pwpb failed to generate protobuf code', file=sys.stderr)
        return 1

    sys.stdout.buffer.write(response.SerializeToString())
    return 0


if __name__ == '__main__':
    sys.exit(main())
