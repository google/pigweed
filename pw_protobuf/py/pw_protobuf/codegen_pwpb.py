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
import enum
import os
import sys
from typing import Dict, Iterable, List, Tuple
from typing import cast

from google.protobuf import descriptor_pb2

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoEnum, ProtoMessage, ProtoMessageField
from pw_protobuf.proto_tree import ProtoNode
from pw_protobuf.proto_tree import build_node_tree

PLUGIN_NAME = 'pw_protobuf'
PLUGIN_VERSION = '0.1.0'

PROTO_H_EXTENSION = '.pwpb.h'
PROTO_CC_EXTENSION = '.pwpb.cc'

PROTOBUF_NAMESPACE = '::pw::protobuf'


class ClassType(enum.Enum):
    """Type of class."""
    MEMORY_ENCODER = 1
    STREAMING_ENCODER = 2
    # MEMORY_DECODER = 3
    STREAMING_DECODER = 4

    def base_class_name(self) -> str:
        """Returns the base class used by this class type."""
        if self is self.STREAMING_ENCODER:
            return 'StreamEncoder'
        if self is self.MEMORY_ENCODER:
            return 'MemoryEncoder'
        if self is self.STREAMING_DECODER:
            return 'StreamDecoder'

        raise ValueError('Unknown class type')

    def codegen_class_name(self) -> str:
        """Returns the base class used by this class type."""
        if self is self.STREAMING_ENCODER:
            return 'StreamEncoder'
        if self is self.MEMORY_ENCODER:
            return 'MemoryEncoder'
        if self is self.STREAMING_DECODER:
            return 'StreamDecoder'

        raise ValueError('Unknown class type')

    def is_encoder(self) -> bool:
        """Returns True if this class type is an encoder."""
        if self is self.STREAMING_ENCODER:
            return True
        if self is self.MEMORY_ENCODER:
            return True
        if self is self.STREAMING_DECODER:
            return False

        raise ValueError('Unknown class type')


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

        # If a class method is referencing its class, the namespace provided
        # must be from the root or it will be empty.
        if type_node == scope:
            scope = self._root

        ancestor = scope.common_ancestor(type_node)
        namespace = type_node.cpp_namespace(ancestor)
        assert namespace
        return namespace


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
        line = 'return {}({}, {});'.format(self._encoder_fn(),
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


class PackedWriteMethod(WriteMethod):
    """A method for a writing a packed repeated field.

    Same as a WriteMethod, but is only generated for repeated fields.
    """
    def should_appear(self) -> bool:
        return self._field.is_repeated()

    def _encoder_fn(self) -> str:
        raise NotImplementedError()


class ReadMethod(ProtoMethod):
    """Base class representing an decoder read method.

    Read methods have following format (for the proto field foo):

        Result<{ctype}> ReadFoo({params...}) {
          Result<uint32_t> field_number = FieldNumber();
          PW_ASSERT(field_number.ok());
          PW_ASSERT(field_number.value() == static_cast<uint32_t>(Fields::FOO));
          return decoder_->Read{type}({params...});
        }

    """
    def name(self) -> str:
        return 'Read{}'.format(self._field.name())

    def return_type(self, from_root: bool = False) -> str:
        return '::pw::Result<{}>'.format(self._result_type())

    def _result_type(self) -> str:
        """The type returned by the deoder function.

        Defined in subclasses.

        e.g. 'uint32_t', 'std::span<std::byte>', etc.
        """
        raise NotImplementedError()

    def body(self) -> List[str]:
        lines: List[str] = []
        lines += ['::pw::Result<uint32_t> field_number = FieldNumber();']
        lines += ['PW_ASSERT(field_number.ok());']
        lines += [
            'PW_ASSERT(field_number.value() == {});'.format(self.field_cast())
        ]
        lines += self._decoder_body()
        return lines

    def _decoder_body(self) -> List[str]:
        """Returns the decoder body part as a list of source code lines."""
        params = ', '.join([pair[1] for pair in self.params()])
        line = 'return {}({});'.format(self._decoder_fn(), params)
        return [line]

    def _decoder_fn(self) -> str:
        """The decoder function to call.

        Defined in subclasses.

        e.g. 'ReadUint32', 'ReadBytes', etc.
        """
        raise NotImplementedError()

    def params(self) -> List[Tuple[str, str]]:
        """Method parameters, can be overriden in subclasses."""
        return []

    def in_class_definition(self) -> bool:
        return True


#
# The following code defines write and read methods for each of the
# complex protobuf types.
#


class SubMessageEncoderMethod(ProtoMethod):
    """Method which returns a sub-message encoder."""
    def name(self) -> str:
        return 'Get{}Encoder'.format(self._field.name())

    def return_type(self, from_root: bool = False) -> str:
        return '{}::StreamEncoder'.format(
            self._relative_type_namespace(from_root))

    def params(self) -> List[Tuple[str, str]]:
        return []

    def body(self) -> List[str]:
        line = 'return {}::StreamEncoder(GetNestedEncoder({}));'.format(
            self._relative_type_namespace(), self.field_cast())
        return [line]

    # Submessage methods are not defined within the class itself because the
    # submessage class may not yet have been defined.
    def in_class_definition(self) -> bool:
        return False


class SubMessageDecoderMethod(ReadMethod):
    """Method which returns a sub-message decoder."""
    def name(self) -> str:
        return 'Get{}Decoder'.format(self._field.name())

    def return_type(self, from_root: bool = False) -> str:
        return '{}::StreamDecoder'.format(
            self._relative_type_namespace(from_root))

    def _decoder_body(self) -> List[str]:
        line = 'return {}::StreamDecoder(GetNestedDecoder());'.format(
            self._relative_type_namespace())
        return [line]

    # Submessage methods are not defined within the class itself because the
    # submessage class may not yet have been defined.
    def in_class_definition(self) -> bool:
        return False


class BytesReaderMethod(ReadMethod):
    """Method which returns a bytes reader."""
    def name(self) -> str:
        return 'Get{}Reader'.format(self._field.name())

    def return_type(self, from_root: bool = False) -> str:
        return '::pw::protobuf::StreamDecoder::BytesReader'

    def _decoder_fn(self) -> str:
        return 'GetBytesReader'


#
# The following code defines write and read methods for each of the
# primitive protobuf types.
#


class DoubleWriteMethod(WriteMethod):
    """Method which writes a proto double value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('double', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteDouble'


class PackedDoubleWriteMethod(PackedWriteMethod):
    """Method which writes a packed list of doubles."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const double>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedDouble'


class DoubleReadMethod(ReadMethod):
    """Method which reads a proto double value."""
    def _result_type(self) -> str:
        return 'double'

    def _decoder_fn(self) -> str:
        return 'ReadDouble'


class FloatWriteMethod(WriteMethod):
    """Method which writes a proto float value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('float', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteFloat'


class PackedFloatWriteMethod(PackedWriteMethod):
    """Method which writes a packed list of floats."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const float>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedFloat'


class FloatReadMethod(ReadMethod):
    """Method which reads a proto float value."""
    def _result_type(self) -> str:
        return 'float'

    def _decoder_fn(self) -> str:
        return 'ReadFloat'


class Int32WriteMethod(WriteMethod):
    """Method which writes a proto int32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteInt32'


class PackedInt32WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of int32."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedInt32'


class Int32ReadMethod(ReadMethod):
    """Method which reads a proto int32 value."""
    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadInt32'


class Sint32WriteMethod(WriteMethod):
    """Method which writes a proto sint32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSint32'


class PackedSint32WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of sint32."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSint32'


class Sint32ReadMethod(ReadMethod):
    """Method which reads a proto sint32 value."""
    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadSint32'


class Sfixed32WriteMethod(WriteMethod):
    """Method which writes a proto sfixed32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSfixed32'


class PackedSfixed32WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of sfixed32."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSfixed32'


class Sfixed32ReadMethod(ReadMethod):
    """Method which reads a proto sfixed32 value."""
    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadSfixed32'


class Int64WriteMethod(WriteMethod):
    """Method which writes a proto int64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteInt64'


class PackedInt64WriteMethod(PackedWriteMethod):
    """Method which writes a proto int64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedInt64'


class Int64ReadMethod(ReadMethod):
    """Method which reads a proto int64 value."""
    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadInt64'


class Sint64WriteMethod(WriteMethod):
    """Method which writes a proto sint64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSint64'


class PackedSint64WriteMethod(PackedWriteMethod):
    """Method which writes a proto sint64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSint64'


class Sint64ReadMethod(ReadMethod):
    """Method which reads a proto sint64 value."""
    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadSint64'


class Sfixed64WriteMethod(WriteMethod):
    """Method which writes a proto sfixed64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('int64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSfixed64'


class PackedSfixed64WriteMethod(PackedWriteMethod):
    """Method which writes a proto sfixed64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const int64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSfixed4'


class Sfixed64ReadMethod(ReadMethod):
    """Method which reads a proto sfixed64 value."""
    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadSfixed64'


class Uint32WriteMethod(WriteMethod):
    """Method which writes a proto uint32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('uint32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteUint32'


class PackedUint32WriteMethod(PackedWriteMethod):
    """Method which writes a proto uint32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const uint32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedUint32'


class Uint32ReadMethod(ReadMethod):
    """Method which reads a proto uint32 value."""
    def _result_type(self) -> str:
        return 'uint32_t'

    def _decoder_fn(self) -> str:
        return 'ReadUint32'


class Fixed32WriteMethod(WriteMethod):
    """Method which writes a proto fixed32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('uint32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteFixed32'


class PackedFixed32WriteMethod(PackedWriteMethod):
    """Method which writes a proto fixed32 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const uint32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedFixed32'


class Fixed32ReadMethod(ReadMethod):
    """Method which reads a proto fixed32 value."""
    def _result_type(self) -> str:
        return 'uint32_t'

    def _decoder_fn(self) -> str:
        return 'ReadFixed32'


class Uint64WriteMethod(WriteMethod):
    """Method which writes a proto uint64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('uint64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteUint64'


class PackedUint64WriteMethod(PackedWriteMethod):
    """Method which writes a proto uint64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const uint64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedUint64'


class Uint64ReadMethod(ReadMethod):
    """Method which reads a proto uint64 value."""
    def _result_type(self) -> str:
        return 'uint64_t'

    def _decoder_fn(self) -> str:
        return 'ReadUint64'


class Fixed64WriteMethod(WriteMethod):
    """Method which writes a proto fixed64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('uint64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteFixed64'


class PackedFixed64WriteMethod(PackedWriteMethod):
    """Method which writes a proto fixed64 value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const uint64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedFixed64'


class Fixed64ReadMethod(ReadMethod):
    """Method which reads a proto fixed64 value."""
    def _result_type(self) -> str:
        return 'uint64_t'

    def _decoder_fn(self) -> str:
        return 'ReadFixed64'


class BoolWriteMethod(WriteMethod):
    """Method which writes a proto bool value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('bool', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteBool'


class BoolReadMethod(ReadMethod):
    """Method which reads a proto bool value."""
    def _result_type(self) -> str:
        return 'bool'

    def _decoder_fn(self) -> str:
        return 'ReadBool'


class BytesWriteMethod(WriteMethod):
    """Method which writes a proto bytes value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<const std::byte>', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteBytes'


class BytesReadMethod(ReadMethod):
    """Method which reads a proto bytes value."""
    def return_type(self, from_root: bool = False) -> str:
        return '::pw::StatusWithSize'

    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<std::byte>', 'out')]

    def _decoder_fn(self) -> str:
        return 'ReadBytes'


class StringLenWriteMethod(WriteMethod):
    """Method which writes a proto string value with length."""
    def params(self) -> List[Tuple[str, str]]:
        return [('const char*', 'value'), ('size_t', 'len')]

    def _encoder_fn(self) -> str:
        return 'WriteString'


class StringWriteMethod(WriteMethod):
    """Method which writes a proto string value."""
    def params(self) -> List[Tuple[str, str]]:
        return [('std::string_view', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteString'


class StringReadMethod(ReadMethod):
    """Method which reads a proto string value."""
    def return_type(self, from_root: bool = False) -> str:
        return '::pw::StatusWithSize'

    def params(self) -> List[Tuple[str, str]]:
        return [('std::span<char>', 'out')]

    def _decoder_fn(self) -> str:
        return 'ReadString'


class EnumWriteMethod(WriteMethod):
    """Method which writes a proto enum value."""
    def params(self) -> List[Tuple[str, str]]:
        return [(self._relative_type_namespace(), 'value')]

    def body(self) -> List[str]:
        line = 'return WriteUint32(' \
            '{}, static_cast<uint32_t>(value));'.format(self.field_cast())
        return [line]

    def in_class_definition(self) -> bool:
        return True

    def _encoder_fn(self) -> str:
        raise NotImplementedError()


class EnumReadMethod(ReadMethod):
    """Method which reads a proto enum value."""
    def _result_type(self):
        return self._relative_type_namespace()

    def _decoder_body(self) -> List[str]:
        lines: List[str] = []
        lines += ['::pw::Result<uint32_t> value = ReadUint32();']
        lines += ['if (!value.ok()) {']
        lines += ['  return value.status();']
        lines += ['}']
        lines += [
            'return static_cast<{}>(value.value());'.format(
                self._result_type())
        ]
        return lines


# Mapping of protobuf field types to their method definitions.
PROTO_FIELD_WRITE_METHODS: Dict[int, List] = {
    descriptor_pb2.FieldDescriptorProto.TYPE_DOUBLE:
    [DoubleWriteMethod, PackedDoubleWriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT:
    [FloatWriteMethod, PackedFloatWriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT32:
    [Int32WriteMethod, PackedInt32WriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT32:
    [Sint32WriteMethod, PackedSint32WriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED32:
    [Sfixed32WriteMethod, PackedSfixed32WriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT64:
    [Int64WriteMethod, PackedInt64WriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT64:
    [Sint64WriteMethod, PackedSint64WriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED64:
    [Sfixed64WriteMethod, PackedSfixed64WriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT32:
    [Uint32WriteMethod, PackedUint32WriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32:
    [Fixed32WriteMethod, PackedFixed32WriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT64:
    [Uint64WriteMethod, PackedUint64WriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64:
    [Fixed64WriteMethod, PackedFixed64WriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_BOOL: [BoolWriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_BYTES: [BytesWriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_STRING: [
        StringLenWriteMethod, StringWriteMethod
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE: [
        SubMessageEncoderMethod
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_ENUM: [EnumWriteMethod],
}

PROTO_FIELD_READ_METHODS: Dict[int, List] = {
    descriptor_pb2.FieldDescriptorProto.TYPE_DOUBLE: [DoubleReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT: [FloatReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT32: [Int32ReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT32: [Sint32ReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED32: [Sfixed32ReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT64: [Int64ReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT64: [Sint64ReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED64: [Sfixed64ReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT32: [Uint32ReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32: [Fixed32ReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT64: [Uint64ReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64: [Fixed64ReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_BOOL: [BoolReadMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_BYTES:
    [BytesReadMethod, BytesReaderMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_STRING:
    [StringReadMethod, BytesReaderMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE:
    [SubMessageDecoderMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_ENUM: [EnumReadMethod],
}


def proto_field_methods(class_type: ClassType, field_type: int) -> List:
    return (PROTO_FIELD_WRITE_METHODS[field_type] if class_type.is_encoder()
            else PROTO_FIELD_READ_METHODS[field_type])


def generate_class_for_message(message: ProtoMessage, root: ProtoNode,
                               output: OutputFile,
                               class_type: ClassType) -> None:
    """Creates a C++ class to encode or decoder a protobuf message."""
    assert message.type() == ProtoNode.Type.MESSAGE

    base_class_name = class_type.base_class_name()
    class_name = class_type.codegen_class_name()

    # Message classes inherit from the base proto message class in codegen.h
    # and use its constructor.
    base_class = f'{PROTOBUF_NAMESPACE}::{base_class_name}'
    output.write_line(
        f'class {message.cpp_namespace(root)}::{class_name} ' \
        f': public {base_class} {{'
    )
    output.write_line(' public:')

    with output.indent():
        # Inherit the constructors from the base class.
        output.write_line(f'using {base_class}::{base_class_name};')

        # Declare a move constructor that takes a base class.
        output.write_line(f'constexpr {class_name}({base_class}&& parent) '
                          f': {base_class}(std::move(parent)) {{}}')

        # Allow MemoryEncoder& to be converted to StreamEncoder&.
        if class_type == ClassType.MEMORY_ENCODER:
            stream_type = (
                f'::{message.cpp_namespace()}::'
                f'{ClassType.STREAMING_ENCODER.codegen_class_name()}')
            output.write_line(
                f'operator {stream_type}&() '
                f' {{ return static_cast<{stream_type}&>('
                f'*static_cast<{PROTOBUF_NAMESPACE}::StreamEncoder*>(this));}}'
            )

        # Generate methods for each of the message's fields.
        for field in message.fields():
            for method_class in proto_field_methods(class_type, field.type()):
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
                                output: OutputFile,
                                class_type: ClassType) -> None:
    """Defines methods for a message class that were previously declared."""
    assert message.type() == ProtoNode.Type.MESSAGE

    for field in message.fields():
        for method_class in proto_field_methods(class_type, field.type()):
            method = method_class(field, message, root)
            if not method.should_appear() or method.in_class_definition():
                continue

            output.write_line()
            class_name = (f'{message.cpp_namespace(root)}::'
                          f'{class_type.codegen_class_name()}')
            method_signature = (
                f'inline {method.return_type(from_root=True)} '
                f'{class_name}::{method.name()}({method.param_string()})')
            output.write_line(f'{method_signature} {{')
            with output.indent():
                for line in method.body():
                    output.write_line(line)
            output.write_line('}')


def generate_code_for_enum(proto_enum: ProtoEnum, root: ProtoNode,
                           output: OutputFile) -> None:
    """Creates a C++ enum for a proto enum."""
    assert proto_enum.type() == ProtoNode.Type.ENUM

    output.write_line(f'enum class {proto_enum.cpp_namespace(root)} {{')
    with output.indent():
        for name, number in proto_enum.values():
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

    # Declare the message's encoder classes.
    output.write_line()
    output.write_line('class StreamEncoder;')
    output.write_line('class MemoryEncoder;')

    # Declare the message's decoder classes.
    output.write_line()
    output.write_line('class StreamDecoder;')

    # Declare the message's enums.
    for child in node.children():
        if child.type() == ProtoNode.Type.ENUM:
            output.write_line()
            generate_code_for_enum(cast(ProtoEnum, child), node, output)

    output.write_line(f'}}  // namespace {namespace}')


def generate_class_wrappers(package: ProtoNode, class_type: ClassType,
                            output: OutputFile):
    # Run through all messages in the file, generating a class for each.
    for node in package:
        if node.type() == ProtoNode.Type.MESSAGE:
            output.write_line()
            generate_class_for_message(cast(ProtoMessage, node), package,
                                       output, class_type)

    # Run a second pass through the classes, this time defining all of the
    # methods which were previously only declared.
    for node in package:
        if node.type() == ProtoNode.Type.MESSAGE:
            define_not_in_class_methods(cast(ProtoMessage, node), package,
                                        output, class_type)


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
    output.write_line('#include <span>')
    output.write_line('#include <string_view>\n')
    output.write_line('#include "pw_assert/assert.h"')
    output.write_line('#include "pw_protobuf/encoder.h"')
    output.write_line('#include "pw_protobuf/stream_decoder.h"')
    output.write_line('#include "pw_result/result.h"')
    output.write_line('#include "pw_status/status.h"')
    output.write_line('#include "pw_status/status_with_size.h"')

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

    generate_class_wrappers(package, ClassType.STREAMING_ENCODER, output)
    generate_class_wrappers(package, ClassType.MEMORY_ENCODER, output)

    generate_class_wrappers(package, ClassType.STREAMING_DECODER, output)

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
