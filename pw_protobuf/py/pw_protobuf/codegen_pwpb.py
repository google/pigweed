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
"""This module defines the generated code for pw_protobuf C++ classes."""

import abc
from datetime import datetime
import os
import sys
from typing import Dict, Iterable, List, Tuple
from typing import cast

import google.protobuf.descriptor_pb2 as descriptor_pb2

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoEnum, ProtoMessage, ProtoMessageField
from pw_protobuf.proto_tree import ProtoNode
from pw_protobuf.proto_tree import build_node_tree

PLUGIN_NAME = 'pw_protobuf'
PLUGIN_VERSION = '0.1.0'

PROTO_H_EXTENSION = '.pwpb.h'
PROTO_CC_EXTENSION = '.pwpb.cc'

PROTOBUF_NAMESPACE = 'pw::protobuf'
BASE_PROTO_CLASS = 'ProtoMessageEncoder'


# protoc captures stdout, so we need to printf debug to stderr.
def debug_print(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


class ProtoMethod(abc.ABC):
    """Base class for a C++ method for a field in a protobuf message."""
    def __init__(
        self,
        field: ProtoMessageField,
        scope: ProtoNode,
        root: ProtoNode,
    ):
        """Creates an instance of a method.

        Args:
          field: the ProtoMessageField to which the method belongs.
          scope: the ProtoNode namespace in which the method is being defined.
        """
        self._field: ProtoMessageField = field
        self._scope: ProtoNode = scope
        self._root: ProtoNode = root

    @abc.abstractmethod
    def name(self) -> str:
        """Returns the name of the method, e.g. DoSomething."""

    @abc.abstractmethod
    def params(self) -> List[Tuple[str, str]]:
        """Returns the parameters of the method as a list of (type, name) pairs.

        e.g.
        [('int', 'foo'), ('const char*', 'bar')]
        """

    @abc.abstractmethod
    def body(self) -> List[str]:
        """Returns the method body as a list of source code lines.

        e.g.
        [
          'int baz = bar[foo];',
          'return (baz ^ foo) >> 3;'
        ]
        """

    @abc.abstractmethod
    def return_type(self, from_root: bool = False) -> str:
        """Returns the return type of the method, e.g. int.

        For non-primitive return types, the from_root argument determines
        whether the namespace should be relative to the message's scope
        (default) or the root scope.
        """

    @abc.abstractmethod
    def in_class_definition(self) -> bool:
        """Determines where the method should be defined.

        Returns True if the method definition should be inlined in its class
        definition, or False if it should be declared in the class and defined
        later.
        """

    def should_appear(self) -> bool:  # pylint: disable=no-self-use
        """Whether the method should be generated."""
        return True

    def param_string(self) -> str:
        return ', '.join([f'{type} {name}' for type, name in self.params()])

    def field_cast(self) -> str:
        return 'static_cast<uint32_t>(Fields::{})'.format(
            self._field.enum_name())

    def _relative_type_namespace(self, from_root: bool = False) -> str:
        """Returns relative namespace between method's scope and field type."""
        scope = self._root if from_root else self._scope
        type_node = self._field.type_node()
        assert type_node is not None
        ancestor = scope.common_ancestor(type_node)
        namespace = type_node.cpp_namespace(ancestor)
        assert namespace is not None
        return namespace


class SubMessageMethod(ProtoMethod):
    """Method which returns a sub-message encoder."""
    def name(self) -> str:
        return 'Get{}Encoder'.format(self._field.name())

    def return_type(self, from_root: bool = False) -> str:
        return '{}::Encoder'.format(self._relative_type_namespace(from_root))

    def params(self) -> List[Tuple[str, str]]:
        return []

    def body(self) -> List[str]:
        line = 'return {}::Encoder(encoder_, {});'.format(
            self._relative_type_namespace(), self.field_cast())
        return [line]

    # Submessage methods are not defined within the class itself because the
    # submessage class may not yet have been defined.
    def in_class_definition(self) -> bool:
        return False


class WriteMethod(ProtoMethod):
    """Base class representing an encoder write method.

    Write methods have following format (for the proto field foo):

        Status WriteFoo({params...}) {
          return encoder_->Write{type}(kFoo, {params...});
        }

    """
    def name(self) -> str:
        return 'Write{}'.format(self._field.name())

    def return_type(self, from_root: bool = False) -> str:
        return '::pw::Status'

    def body(self) -> List[str]:
        params = ', '.join([pair[1] for pair in self.params()])
        line = 'return encoder_->{}({}, {});'.format(self._encoder_fn(),
                                                     self.field_cast(), params)
        return [line]

    def params(self) -> List[Tuple[str, str]]:
        """Method parameters, defined in subclasses."""
        raise NotImplementedError()

    def in_class_definition(self) -> bool:
        return True

    def _encoder_fn(self) -> str:
        """The encoder function to call.

        Defined in subclasses.

        e.g. 'WriteUint32', 'WriteBytes', etc.
        """
        raise NotImplementedError()


class PackedMethod(WriteMethod):
    """A method for a packed repeated field.

    Same as a WriteMethod, but is only generated for repeated fields.
    """
    def should_appear(self) -> bool:
        return self._field.is_repeated()

    def _encoder_fn(self) -> str:
        raise NotImplementedError()


#
# The following code defines write methods for each of the
# primitive protobuf types.
#


class DoubleMethod(WriteMethod):
    """Method which writes a proto double value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('double', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteDouble'


class PackedDoubleMethod(PackedMethod):
    """Method which writes a packed list of doubles."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const double>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedDouble'


class FloatMethod(WriteMethod):
    """Method which writes a proto float value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('float', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteFloat'


class PackedFloatMethod(PackedMethod):
    """Method which writes a packed list of floats."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const float>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedFloat'


class Int32Method(WriteMethod):
    """Method which writes a proto int32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteInt32'


class PackedInt32Method(PackedMethod):
    """Method which writes a packed list of int32."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedInt32'


class Sint32Method(WriteMethod):
    """Method which writes a proto sint32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSint32'


class PackedSint32Method(PackedMethod):
    """Method which writes a packed list of sint32."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSint32'


class Sfixed32Method(WriteMethod):
    """Method which writes a proto sfixed32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSfixed32'


class PackedSfixed32Method(PackedMethod):
    """Method which writes a packed list of sfixed32."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSfixed32'


class Int64Method(WriteMethod):
    """Method which writes a proto int64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteInt64'


class PackedInt64Method(PackedMethod):
    """Method which writes a proto int64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedInt64'


class Sint64Method(WriteMethod):
    """Method which writes a proto sint64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSint64'


class PackedSint64Method(PackedMethod):
    """Method which writes a proto sint64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSint64'


class Sfixed64Method(WriteMethod):
    """Method which writes a proto sfixed64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSfixed64'


class PackedSfixed64Method(PackedMethod):
    """Method which writes a proto sfixed64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSfixed4'


class Uint32Method(WriteMethod):
    """Method which writes a proto uint32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('uint32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteUint32'


class PackedUint32Method(PackedMethod):
    """Method which writes a proto uint32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const uint32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedUint32'


class Fixed32Method(WriteMethod):
    """Method which writes a proto fixed32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('uint32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteFixed32'


class PackedFixed32Method(PackedMethod):
    """Method which writes a proto fixed32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const uint32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedFixed32'


class Uint64Method(WriteMethod):
    """Method which writes a proto uint64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('uint64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteUint64'


class PackedUint64Method(PackedMethod):
    """Method which writes a proto uint64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const uint64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedUint64'


class Fixed64Method(WriteMethod):
    """Method which writes a proto fixed64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('uint64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteFixed64'


class PackedFixed64Method(PackedMethod):
    """Method which writes a proto fixed64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const uint64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedFixed64'


class BoolMethod(WriteMethod):
    """Method which writes a proto bool value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('bool', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteBool'


class BytesMethod(WriteMethod):
    """Method which writes a proto bytes value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const std::byte>', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteBytes'


class StringLenMethod(WriteMethod):
    """Method which writes a proto string value with length."""
    def params(self) -> List[Tuple[str, str]]:
        return [('const char*', 'value'), ('size_t', 'len')]

    def _encoder_fn(self) -> str:
        return 'WriteString'


class StringMethod(WriteMethod):
    """Method which writes a proto string value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('const char*', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteString'


class EnumMethod(WriteMethod):
    """Method which writes a proto enum value."""
    def params(self) -> List[Tuple[str, str]]:
        return [(self._relative_type_namespace(), 'value')]

    def body(self) -> List[str]:
        line = 'return encoder_->WriteUint32(' \
            '{}, static_cast<uint32_t>(value));'.format(self.field_cast())
        return [line]

    def in_class_definition(self) -> bool:
        return True

    def _encoder_fn(self) -> str:
        raise NotImplementedError()


# Mapping of protobuf field types to their method definitions.
PROTO_FIELD_METHODS: Dict[int, List] = {
    descriptor_pb2.FieldDescriptorProto.TYPE_DOUBLE:
    [DoubleMethod, PackedDoubleMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT:
    [FloatMethod, PackedFloatMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT32:
    [Int32Method, PackedInt32Method],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT32:
    [Sint32Method, PackedSint32Method],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED32:
    [Sfixed32Method, PackedSfixed32Method],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT64:
    [Int64Method, PackedInt64Method],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT64:
    [Sint64Method, PackedSint64Method],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED64:
    [Sfixed64Method, PackedSfixed64Method],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT32:
    [Uint32Method, PackedUint32Method],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32:
    [Fixed32Method, PackedFixed32Method],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT64:
    [Uint64Method, PackedUint64Method],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64:
    [Fixed64Method, PackedFixed64Method],
    descriptor_pb2.FieldDescriptorProto.TYPE_BOOL: [BoolMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_BYTES: [BytesMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_STRING: [
        StringLenMethod, StringMethod
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE: [SubMessageMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_ENUM: [EnumMethod],
}


def generate_code_for_message(message: ProtoMessage, root: ProtoNode,
                              output: OutputFile) -> None:
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


def define_not_in_class_methods(message: ProtoMessage, root: ProtoNode,
                                output: OutputFile) -> None:
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


def generate_code_for_enum(enum: ProtoEnum, root: ProtoNode,
                           output: OutputFile) -> None:
    """Creates a C++ enum for a proto enum."""
    assert enum.type() == ProtoNode.Type.ENUM

    output.write_line(f'enum class {enum.cpp_namespace(root)} {{')
    with output.indent():
        for name, number in enum.values():
            output.write_line(f'{name} = {number},')
    output.write_line('};')


def forward_declare(node: ProtoMessage, root: ProtoNode,
                    output: OutputFile) -> None:
    """Generates code forward-declaring entities in a message's namespace."""
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
            generate_code_for_enum(cast(ProtoEnum, child), node, output)

    output.write_line(f'}}  // namespace {namespace}')


def _proto_filename_to_generated_header(proto_file: str) -> str:
    """Returns the generated C++ header name for a .proto file."""
    return os.path.splitext(proto_file)[0] + PROTO_H_EXTENSION


def generate_code_for_package(file_descriptor_proto, package: ProtoNode,
                              output: OutputFile) -> None:
    """Generates code for a single .pb.h file corresponding to a .proto file."""

    assert package.type() == ProtoNode.Type.PACKAGE

    output.write_line(f'// {os.path.basename(output.name())} automatically '
                      f'generated by {PLUGIN_NAME} {PLUGIN_VERSION}')
    output.write_line(f'// on {datetime.now()}')
    output.write_line('#pragma once\n')
    output.write_line('#include <cstddef>')
    output.write_line('#include <cstdint>')
    output.write_line('#include <span>\n')
    output.write_line('#include "pw_protobuf/codegen.h"')

    for imported_file in file_descriptor_proto.dependency:
        generated_header = _proto_filename_to_generated_header(imported_file)
        output.write_line(f'#include "{generated_header}"')

    if package.cpp_namespace():
        file_namespace = package.cpp_namespace()
        if file_namespace.startswith('::'):
            file_namespace = file_namespace[2:]

        output.write_line(f'\nnamespace {file_namespace} {{')

    for node in package:
        if node.type() == ProtoNode.Type.MESSAGE:
            forward_declare(cast(ProtoMessage, node), package, output)

    # Define all top-level enums.
    for node in package.children():
        if node.type() == ProtoNode.Type.ENUM:
            output.write_line()
            generate_code_for_enum(cast(ProtoEnum, node), package, output)

    # Run through all messages in the file, generating a class for each.
    for node in package:
        if node.type() == ProtoNode.Type.MESSAGE:
            output.write_line()
            generate_code_for_message(cast(ProtoMessage, node), package,
                                      output)

    # Run a second pass through the classes, this time defining all of the
    # methods which were previously only declared.
    for node in package:
        if node.type() == ProtoNode.Type.MESSAGE:
            define_not_in_class_methods(cast(ProtoMessage, node), package,
                                        output)

    if package.cpp_namespace():
        output.write_line(f'\n}}  // namespace {package.cpp_namespace()}')


def process_proto_file(proto_file) -> Iterable[OutputFile]:
    """Generates code for a single .proto file."""

    # Two passes are made through the file. The first builds the tree of all
    # message/enum nodes, then the second creates the fields in each. This is
    # done as non-primitive fields need pointers to their types, which requires
    # the entire tree to have been parsed into memory.
    _, package_root = build_node_tree(proto_file)

    output_filename = _proto_filename_to_generated_header(proto_file.name)
    output_file = OutputFile(output_filename)
    generate_code_for_package(proto_file, package_root, output_file)

    return [output_file]
