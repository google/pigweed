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
"""This module defines methods for protobuf message C++ encoder classes."""

import abc
from typing import List, Tuple

import google.protobuf.descriptor_pb2 as descriptor_pb2

from pw_protobuf.proto_structures import ProtoMessageField, ProtoNode


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
        ancestor = scope.common_ancestor(self._field.type_node())
        return self._field.type_node().cpp_namespace(ancestor)


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
        return [('span<const double>', 'values')]

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
        return [('span<const float>', 'values')]

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
        return [('span<const int32_t>', 'values')]

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
        return [('span<const int32_t>', 'values')]

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
        return [('span<const int32_t>', 'values')]

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
        return [('span<const int64_t>', 'values')]

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
        return [('span<const int64_t>', 'values')]

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
        return [('span<const int64_t>', 'values')]

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
        return [('span<const uint32_t>', 'values')]

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
        return [('span<const uint32_t>', 'values')]

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
        return [('span<const uint64_t>', 'values')]

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
        return [('span<const uint64_t>', 'values')]

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
        return [('span<const std::byte>', 'value')]

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
PROTO_FIELD_METHODS = {
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
