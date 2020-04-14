#!/usr/bin/env python3
# Copyright 2019 The Pigweed Authors
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

import os
import sys

from typing import List

import google.protobuf.compiler.plugin_pb2 as plugin_pb2
import google.protobuf.descriptor_pb2 as descriptor_pb2

from pw_protobuf.methods import PROTO_FIELD_METHODS
from pw_protobuf.proto_structures import ProtoEnum, ProtoMessage
from pw_protobuf.proto_structures import ProtoMessageField, ProtoNode
from pw_protobuf.proto_structures import ProtoPackage

PLUGIN_NAME = 'pw_protobuf'
PLUGIN_VERSION = '0.1.0'

PROTOBUF_NAMESPACE = 'pw::protobuf'
BASE_PROTO_CLASS = 'ProtoMessageEncoder'


# protoc captures stdout, so we need to printf debug to stderr.
def debug_print(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


class OutputFile:
    """A buffer to which data is written.

    Example:

    ```
    output = Output("hello.c")
    output.write_line('int main(void) {')
    with output.indent():
        output.write_line('printf("Hello, world");')
        output.write_line('return 0;')
    output.write_line('}')

    print(output.content())
    ```

    Produces:
    ```
    int main(void) {
      printf("Hello, world");
      return 0;
    }
    ```
    """

    INDENT_WIDTH = 2

    def __init__(self, filename: str):
        self._filename: str = filename
        self._content: List[str] = []
        self._indentation: int = 0

    def write_line(self, line: str = '') -> None:
        if line:
            self._content.append(' ' * self._indentation)
            self._content.append(line)
        self._content.append('\n')

    def indent(self) -> 'OutputFile._IndentationContext':
        """Increases the indentation level of the output."""
        return self._IndentationContext(self)

    def name(self) -> str:
        return self._filename

    def content(self) -> str:
        return ''.join(self._content)

    class _IndentationContext:
        """Context that increases the output's indentation when it is active."""
        def __init__(self, output: 'OutputFile'):
            self._output = output

        def __enter__(self):
            self._output._indentation += OutputFile.INDENT_WIDTH

        def __exit__(self, typ, value, traceback):
            self._output._indentation -= OutputFile.INDENT_WIDTH


def generate_code_for_message(
        message: ProtoNode,
        root: ProtoNode,
        output: OutputFile,
) -> None:
    """Creates a C++ class for a protobuf message."""
    assert message.type() == ProtoNode.Type.MESSAGE

    # Message classes inherit from the base proto message class in codegen.h
    # and use its constructor.
    base_class = f'{PROTOBUF_NAMESPACE}::{BASE_PROTO_CLASS}'
    output.write_line(
        f'class {message.cpp_namespace(root)}::Encoder : public {base_class} {{'
    )
    output.write_line(' public:')

    with output.indent():
        output.write_line(f'using {BASE_PROTO_CLASS}::{BASE_PROTO_CLASS};')

        # Generate methods for each of the message's fields.
        for field in message.fields():
            for method_class in PROTO_FIELD_METHODS[field.type()]:
                method = method_class(field, message, root)
                if not method.should_appear():
                    continue

                output.write_line()
                method_signature = (
                    f'{method.return_type()} '
                    f'{method.name()}({method.param_string()})')

                if not method.in_class_definition():
                    # Method will be defined outside of the class at the end of
                    # the file.
                    output.write_line(f'{method_signature};')
                    continue

                output.write_line(f'{method_signature} {{')
                with output.indent():
                    for line in method.body():
                        output.write_line(line)
                output.write_line('}')

    output.write_line('};')


def define_not_in_class_methods(
        message: ProtoNode,
        root: ProtoNode,
        output: OutputFile,
) -> None:
    """Defines methods for a message class that were previously declared."""
    assert message.type() == ProtoNode.Type.MESSAGE

    for field in message.fields():
        for method_class in PROTO_FIELD_METHODS[field.type()]:
            method = method_class(field, message, root)
            if not method.should_appear() or method.in_class_definition():
                continue

            output.write_line()
            class_name = f'{message.cpp_namespace(root)}::Encoder'
            method_signature = (
                f'inline {method.return_type(from_root=True)} '
                f'{class_name}::{method.name()}({method.param_string()})')
            output.write_line(f'{method_signature} {{')
            with output.indent():
                for line in method.body():
                    output.write_line(line)
            output.write_line('}')


def generate_code_for_enum(
        enum: ProtoNode,
        root: ProtoNode,
        output: OutputFile,
) -> None:
    """Creates a C++ enum for a proto enum."""
    assert enum.type() == ProtoNode.Type.ENUM

    output.write_line(f'enum class {enum.cpp_namespace(root)} {{')
    with output.indent():
        for name, number in enum.values():
            output.write_line(f'{name} = {number},')
    output.write_line('};')


def forward_declare(
        node: ProtoNode,
        root: ProtoNode,
        output: OutputFile,
) -> None:
    """Generates code forward-declaring entities in a message's namespace."""
    if node.type() != ProtoNode.Type.MESSAGE:
        return

    namespace = node.cpp_namespace(root)
    output.write_line()
    output.write_line(f'namespace {namespace} {{')

    # Define an enum defining each of the message's fields and their numbers.
    output.write_line('enum class Fields {')
    with output.indent():
        for field in node.fields():
            output.write_line(f'{field.enum_name()} = {field.number()},')
    output.write_line('};')

    # Declare the message's encoder class and all of its enums.
    output.write_line()
    output.write_line('class Encoder;')
    for child in node.children():
        if child.type() == ProtoNode.Type.ENUM:
            output.write_line()
            generate_code_for_enum(child, node, output)

    output.write_line(f'}}  // namespace {namespace}')


# TODO(frolv): Right now, this plugin assumes that package is synonymous with
# .proto file. This will need to be updated to handle compiling multiple files.
def generate_code_for_package(package: ProtoNode, output: OutputFile) -> None:
    """Generates code for a single .pb.h file corresponding to a .proto file."""

    assert package.type() == ProtoNode.Type.PACKAGE

    output.write_line(f'// {os.path.basename(output.name())} automatically '
                      f'generated by {PLUGIN_NAME} {PLUGIN_VERSION}')
    output.write_line('#pragma once\n')
    output.write_line('#include <cstddef>')
    output.write_line('#include <cstdint>\n')
    output.write_line('#include "pw_protobuf/codegen.h"')

    if package.cpp_namespace():
        output.write_line(f'\nnamespace {package.cpp_namespace()} {{')

    for node in package:
        forward_declare(node, package, output)

    # Define all top-level enums.
    for node in package.children():
        if node.type() == ProtoNode.Type.ENUM:
            output.write_line()
            generate_code_for_enum(node, package, output)

    # Run through all messages in the file, generating a class for each.
    for node in package:
        if node.type() == ProtoNode.Type.MESSAGE:
            output.write_line()
            generate_code_for_message(node, package, output)

    # Run a second pass through the classes, this time defining all of the
    # methods which were previously only declared.
    for node in package:
        if node.type() == ProtoNode.Type.MESSAGE:
            define_not_in_class_methods(node, package, output)

    if package.cpp_namespace():
        output.write_line(f'\n}}  // namespace {package.cpp_namespace()}')


def add_enum_fields(enum: ProtoNode, proto_enum) -> None:
    """Adds fields from a protobuf enum descriptor to an enum node."""
    assert enum.type() == ProtoNode.Type.ENUM
    for value in proto_enum.value:
        enum.add_value(value.name, value.number)


def add_message_fields(
        root: ProtoNode,
        message: ProtoNode,
        proto_message,
) -> None:
    """Adds fields from a protobuf message descriptor to a message node."""
    assert message.type() == ProtoNode.Type.MESSAGE

    for field in proto_message.field:
        if field.type_name:
            # The "type_name" member contains the global .proto path of the
            # field's type object, for example ".pw.protobuf.test.KeyValuePair".
            # Since only a single proto file is currently supported, the root
            # node has the value of the file's package ("pw.protobuf.test").
            # This must be stripped from the path to find the desired node
            # within the tree.
            #
            # TODO(frolv): Once multiple files are supported, the root node
            # should refer to the global namespace, and this should no longer
            # be needed.
            path = field.type_name
            if path[0] == '.':
                path = path[1:]

            if path.startswith(root.name()):
                relative_path = path[len(root.name()):].lstrip('.')
            else:
                relative_path = path

            type_node = root.find(relative_path)
        else:
            type_node = None

        repeated = \
            field.label == descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED
        message.add_field(
            ProtoMessageField(
                field.name,
                field.number,
                field.type,
                type_node,
                repeated,
            ))


def populate_fields(proto_file, root: ProtoNode) -> None:
    """Traverses a proto file, adding all message and enum fields to a tree."""
    def populate_message(node, message):
        """Recursively populates nested messages and enums."""
        add_message_fields(root, node, message)

        for enum in message.enum_type:
            add_enum_fields(node.find(enum.name), enum)
        for msg in message.nested_type:
            populate_message(node.find(msg.name), msg)

    # Iterate through the proto file, populating top-level enums and messages.
    for enum in proto_file.enum_type:
        add_enum_fields(root.find(enum.name), enum)
    for message in proto_file.message_type:
        populate_message(root.find(message.name), message)


def build_hierarchy(proto_file):
    """Creates a ProtoNode hierarchy from a proto file descriptor."""

    root = ProtoPackage(proto_file.package)

    def build_message_subtree(proto_message):
        node = ProtoMessage(proto_message.name)
        for enum in proto_message.enum_type:
            node.add_child(ProtoEnum(enum.name))
        for submessage in proto_message.nested_type:
            node.add_child(build_message_subtree(submessage))

        return node

    for enum in proto_file.enum_type:
        root.add_child(ProtoEnum(enum.name))

    for message in proto_file.message_type:
        root.add_child(build_message_subtree(message))

    return root


def process_proto_file(proto_file):
    """Generates code for a single .proto file."""

    # Two passes are made through the file. The first builds the tree of all
    # message/enum nodes, then the second creates the fields in each. This is
    # done as non-primitive fields need pointers to their types, which requires
    # the entire tree to have been parsed into memory.
    root = build_hierarchy(proto_file)
    populate_fields(proto_file, root)

    output_filename = os.path.splitext(proto_file.name)[0] + '.pwpb.h'
    output_file = OutputFile(output_filename)
    generate_code_for_package(root, output_file)

    return output_file


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
        # TODO(frolv): Proto files are currently processed individually. Support
        # for multiple files with cross-dependencies should be added.
        output_file = process_proto_file(proto_file)
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
    sys.stdout.buffer.write(response.SerializeToString())
    return 0


if __name__ == '__main__':
    sys.exit(main())
