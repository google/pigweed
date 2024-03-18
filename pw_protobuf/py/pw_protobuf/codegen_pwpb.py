# Copyright 2023 The Pigweed Authors
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
import enum

# Type ignore here for graphlib-backport on Python 3.8
from graphlib import CycleError, TopologicalSorter  # type: ignore
from itertools import takewhile
import os
import sys
from typing import Dict, Iterable, Optional, Type
from typing import cast

from google.protobuf import descriptor_pb2

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoEnum, ProtoMessage, ProtoMessageField
from pw_protobuf.proto_tree import ProtoNode
from pw_protobuf.proto_tree import build_node_tree
from pw_protobuf.proto_tree import EXTERNAL_SYMBOL_WORKAROUND_NAMESPACE

PLUGIN_NAME = 'pw_protobuf'
PLUGIN_VERSION = '0.1.0'

PROTO_H_EXTENSION = '.pwpb.h'
PROTO_CC_EXTENSION = '.pwpb.cc'

PROTOBUF_NAMESPACE = '::pw::protobuf'
_INTERNAL_NAMESPACE = '::pw::protobuf::internal'


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


class ProtoMember(abc.ABC):
    """Base class for a C++ class member for a field in a protobuf message."""

    def __init__(
        self, field: ProtoMessageField, scope: ProtoNode, root: ProtoNode
    ):
        """Creates an instance of a class member.

        Args:
          field: the ProtoMessageField to which the method belongs.
          scope: the ProtoNode namespace in which the method is being defined.
        """
        self._field: ProtoMessageField = field
        self._scope: ProtoNode = scope
        self._root: ProtoNode = root

    @abc.abstractmethod
    def name(self) -> str:
        """Returns the name of the member, e.g. DoSomething."""

    @abc.abstractmethod
    def should_appear(self) -> bool:  # pylint: disable=no-self-use
        """Whether the member should be generated."""

    def field_cast(self) -> str:
        return 'static_cast<uint32_t>(Fields::{})'.format(
            self._field.enum_name()
        )

    def _relative_type_namespace(self, from_root: bool = False) -> str:
        """Returns relative namespace between member's scope and field type."""
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


class ProtoMethod(ProtoMember):
    """Base class for a C++ method for a field in a protobuf message."""

    def __init__(
        self,
        field: ProtoMessageField,
        scope: ProtoNode,
        root: ProtoNode,
        base_class: str,
    ):
        super().__init__(field, scope, root)
        self._base_class: str = base_class

    @abc.abstractmethod
    def params(self) -> list[tuple[str, str]]:
        """Returns the parameters of the method as a list of (type, name) pairs.

        e.g.
        [('int', 'foo'), ('const char*', 'bar')]
        """

    @abc.abstractmethod
    def body(self) -> list[str]:
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

    def body(self) -> list[str]:
        params = ', '.join([pair[1] for pair in self.params()])
        line = 'return {}::{}({}, {});'.format(
            self._base_class, self._encoder_fn(), self.field_cast(), params
        )
        return [line]

    def params(self) -> list[tuple[str, str]]:
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
          PW_ASSERT(field_number.value() ==
                    static_cast<uint32_t>(Fields::kFoo));
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

        e.g. 'uint32_t', 'pw::span<std::byte>', etc.
        """
        raise NotImplementedError()

    def body(self) -> list[str]:
        lines: list[str] = []
        lines += ['::pw::Result<uint32_t> field_number = FieldNumber();']
        lines += ['PW_ASSERT(field_number.ok());']
        lines += [
            'PW_ASSERT(field_number.value() == {});'.format(self.field_cast())
        ]
        lines += self._decoder_body()
        return lines

    def _decoder_body(self) -> list[str]:
        """Returns the decoder body part as a list of source code lines."""
        params = ', '.join([pair[1] for pair in self.params()])
        line = 'return {}::{}({});'.format(
            self._base_class, self._decoder_fn(), params
        )
        return [line]

    def _decoder_fn(self) -> str:
        """The decoder function to call.

        Defined in subclasses.

        e.g. 'ReadUint32', 'ReadBytes', etc.
        """
        raise NotImplementedError()

    def params(self) -> list[tuple[str, str]]:
        """Method parameters, can be overriden in subclasses."""
        return []

    def in_class_definition(self) -> bool:
        return True


class PackedReadMethod(ReadMethod):
    """A method for a reading a packed repeated field.

    Same as ReadMethod, but is only generated for repeated fields.
    """

    def should_appear(self) -> bool:
        return self._field.is_repeated()

    def return_type(self, from_root: bool = False) -> str:
        return '::pw::StatusWithSize'

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<{}>'.format(self._result_type()), 'out')]


class PackedReadVectorMethod(ReadMethod):
    """A method for a reading a packed repeated field.

    An alternative to ReadMethod for repeated fields that appends values into
    a pw::Vector.
    """

    def should_appear(self) -> bool:
        return self._field.is_repeated()

    def return_type(self, from_root: bool = False) -> str:
        return '::pw::Status'

    def params(self) -> list[tuple[str, str]]:
        return [('::pw::Vector<{}>&'.format(self._result_type()), 'out')]


class FindMethod(ReadMethod):
    def name(self) -> str:
        return 'Find{}'.format(self._field.name())

    def params(self) -> list[tuple[str, str]]:
        return [('::pw::ConstByteSpan', 'message')]

    def body(self) -> list[str]:
        lines: list[str] = []
        lines += [
            f'return {PROTOBUF_NAMESPACE}::{self._find_fn()}'
            f'(message, {self.field_cast()});'
        ]
        return lines

    def _find_fn(self) -> str:
        """The find function to call.

        Defined in subclasses.

        e.g. 'FindUint32', 'FindBytes', etc.
        """
        raise NotImplementedError()


class FindStreamMethod(FindMethod):
    def name(self) -> str:
        return 'Find{}'.format(self._field.name())

    def params(self) -> list[tuple[str, str]]:
        return [('::pw::stream::Reader&', 'message_stream')]

    def body(self) -> list[str]:
        lines: list[str] = []
        lines += [
            f'return {PROTOBUF_NAMESPACE}::{self._find_fn()}'
            f'(message_stream, {self.field_cast()});'
        ]
        return lines


class MessageProperty(ProtoMember):
    """Base class for a C++ property for a field in a protobuf message."""

    def name(self) -> str:
        return self._field.field_name()

    def should_appear(self) -> bool:
        return True

    @abc.abstractmethod
    def type_name(self, from_root: bool = False) -> str:
        """Returns the type of the property, e.g. uint32_t."""

    @abc.abstractmethod
    def wire_type(self) -> str:
        """Returns the wire type of the property, e.g. kVarint."""

    def varint_decode_type(self) -> str:
        """Returns the varint decoding type of the property, e.g. kZigZag.

        Defined in subclasses that return kVarint for wire_type().
        """
        raise NotImplementedError()

    def is_string(self) -> bool:  # pylint: disable=no-self-use
        """True if this field is a string field (as opposed to bytes)."""
        return False

    @staticmethod
    def repeated_field_container(type_name: str, max_size: str) -> str:
        """Returns the container type used for repeated fields.

        Defaults to ::pw::Vector<type, max_size>. String fields use
        ::pw::InlineString<max_size> instead.
        """
        return f'::pw::Vector<{type_name}, {max_size}>'

    def use_callback(self) -> bool:  # pylint: disable=no-self-use
        """Returns whether the decoder should use a callback."""
        options = self._field.options()
        assert options is not None
        return options.use_callback or (
            self._field.is_repeated() and self.max_size() == 0
        )

    def is_optional(self) -> bool:
        """Returns whether the decoder should use std::optional."""
        return (
            self._field.is_optional()
            and self.max_size() == 0
            and self.wire_type() != 'kDelimited'
        )

    def is_repeated(self) -> bool:
        return self._field.is_repeated()

    def max_size(self) -> int:
        """Returns the maximum size of the field."""
        if self._field.is_repeated():
            options = self._field.options()
            assert options is not None
            return options.max_count

        return 0

    def is_fixed_size(self) -> bool:
        """Returns whether the decoder should use a fixed sized field."""
        if self._field.is_repeated():
            options = self._field.options()
            assert options is not None
            return options.fixed_count

        return False

    def sub_table(self) -> str:  # pylint: disable=no-self-use
        return '{}'

    def struct_member_type(self, from_root: bool = False) -> str:
        """Returns the structure member type."""
        if self.use_callback():
            return (
                f'{PROTOBUF_NAMESPACE}::Callback<StreamEncoder, StreamDecoder>'
            )

        # Optional fields are wrapped in std::optional
        if self.is_optional():
            return 'std::optional<{}>'.format(self.type_name(from_root))

        # Non-repeated fields have a member of just the type name.
        max_size = self.max_size()
        if max_size == 0:
            return self.type_name(from_root)

        # Fixed size fields use std::array.
        if self.is_fixed_size():
            return 'std::array<{}, {}>'.format(
                self.type_name(from_root), self.max_size_constant_name()
            )

        # Otherwise prefer pw::Vector for repeated fields.
        return self.repeated_field_container(
            self.type_name(from_root), self.max_size_constant_name()
        )

    def max_size_constant_name(self) -> str:
        return f'k{self._field.name()}MaxSize'

    def _varint_type_table_entry(self) -> str:
        if self.wire_type() == 'kVarint':
            return '{}::VarintType::{}'.format(
                _INTERNAL_NAMESPACE, self.varint_decode_type()
            )

        return f'static_cast<{_INTERNAL_NAMESPACE}::VarintType>(0)'

    def _wire_type_table_entry(self) -> str:
        return '{}::WireType::{}'.format(PROTOBUF_NAMESPACE, self.wire_type())

    def _elem_size_table_entry(self) -> str:
        return 'sizeof({})'.format(self.type_name())

    def _bool_attr(self, attr: str) -> str:
        """C++ string for a bool argument that includes the argument name."""
        return f'/*{attr}=*/{bool(getattr(self, attr)())}'.lower()

    def table_entry(self) -> list[str]:
        """Table entry."""
        return [
            self.field_cast(),
            self._wire_type_table_entry(),
            self._elem_size_table_entry(),
            self._varint_type_table_entry(),
            self._bool_attr('is_string'),
            self._bool_attr('is_fixed_size'),
            self._bool_attr('is_repeated'),
            self._bool_attr('is_optional'),
            self._bool_attr('use_callback'),
            'offsetof(Message, {})'.format(self.name()),
            'sizeof(Message::{})'.format(self.name()),
            self.sub_table(),
        ]

    @abc.abstractmethod
    def _size_fn(self) -> str:
        """Returns the name of the field size function."""

    def _size_length(self) -> Optional[str]:  # pylint: disable=no-self-use
        """Returns the length to add to the maximum encoded size."""
        return None

    def max_encoded_size(self) -> str:
        """Returns a constant expression for field's maximum encoded size."""
        size_call = '{}::{}({})'.format(
            PROTOBUF_NAMESPACE, self._size_fn(), self.field_cast()
        )

        size_length: Optional[str] = self._size_length()
        if size_length is None:
            return size_call

        return f'{size_call} + {size_length}'

    def include_in_scratch_size(self) -> bool:  # pylint: disable=no-self-use
        """Returns whether the field contributes to the scratch buffer size."""
        return False


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
            self._relative_type_namespace(from_root)
        )

    def params(self) -> list[tuple[str, str]]:
        return []

    def body(self) -> list[str]:
        line = 'return {}::StreamEncoder({}::GetNestedEncoder({}));'.format(
            self._relative_type_namespace(), self._base_class, self.field_cast()
        )
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
            self._relative_type_namespace(from_root)
        )

    def _decoder_body(self) -> list[str]:
        line = 'return {}::StreamDecoder(GetNestedDecoder());'.format(
            self._relative_type_namespace()
        )
        return [line]

    # Submessage methods are not defined within the class itself because the
    # submessage class may not yet have been defined.
    def in_class_definition(self) -> bool:
        return False


class SubMessageFindMethod(FindMethod):
    """Method which reads a proto submessage."""

    def _result_type(self) -> str:
        return '::pw::ConstByteSpan'

    def _find_fn(self) -> str:
        return 'FindBytes'


class SubMessageProperty(MessageProperty):
    """Property which contains a sub-message."""

    def _dependency_removed(self) -> bool:
        """Returns true if the message dependency was removed to break a cycle.

        Proto allows cycles between messages, but C++ doesn't allow cycles
        between class references. So when we're forced to break one, the
        struct member is replaced with a callback.
        """
        type_node = self._field.type_node()
        assert type_node is not None
        return type_node in cast(ProtoMessage, self._scope).dependency_cycles()

    def _elem_size_table_entry(self) -> str:
        # Since messages can't be repeated (as we couldn't set callbacks),
        # only field size is used. Set elem_size to 0 so space can be saved by
        # not using more than 4 bits for it.
        return '0'

    def type_name(self, from_root: bool = False) -> str:
        return '{}::Message'.format(self._relative_type_namespace(from_root))

    def use_callback(self) -> bool:
        # Always use a callback for a message dependency removed to break a
        # cycle, and for repeated fields, since in both cases there's no way
        # to handle the size of nested field.
        options = self._field.options()
        assert options is not None
        return (
            options.use_callback
            or self._dependency_removed()
            or self._field.is_repeated()
        )

    def wire_type(self) -> str:
        return 'kDelimited'

    def sub_table(self) -> str:
        if self.use_callback():
            return 'nullptr'

        return '&{}::kMessageFields'.format(self._relative_type_namespace())

    def _size_fn(self) -> str:
        # This uses the WithoutValue method to ensure that the maximum length
        # of the delimited field size varint is used. This is because the nested
        # message might include callbacks and be longer than we expect, and to
        # account for scratch overhead when used with MemoryEncoder.
        return 'SizeOfDelimitedFieldWithoutValue'

    def _size_length(self) -> Optional[str]:
        if self.use_callback():
            return None

        return '{}::kMaxEncodedSizeBytes'.format(
            self._relative_type_namespace()
        )

    def include_in_scratch_size(self) -> bool:
        return True


class BytesReaderMethod(ReadMethod):
    """Method which returns a bytes reader."""

    def name(self) -> str:
        return 'Get{}Reader'.format(self._field.name())

    def return_type(self, from_root: bool = False) -> str:
        return f'{PROTOBUF_NAMESPACE}::StreamDecoder::BytesReader'

    def _decoder_fn(self) -> str:
        return 'GetBytesReader'


#
# The following code defines write and read methods for each of the
# primitive protobuf types.
#


class DoubleWriteMethod(WriteMethod):
    """Method which writes a proto double value."""

    def params(self) -> list[tuple[str, str]]:
        return [('double', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteDouble'


class PackedDoubleWriteMethod(PackedWriteMethod):
    """Method which writes a packed list of doubles."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const double>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedDouble'


class PackedDoubleWriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of doubles."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<double>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedDouble'


class DoubleReadMethod(ReadMethod):
    """Method which reads a proto double value."""

    def _result_type(self) -> str:
        return 'double'

    def _decoder_fn(self) -> str:
        return 'ReadDouble'


class PackedDoubleReadMethod(PackedReadMethod):
    """Method which reads packed double values."""

    def _result_type(self) -> str:
        return 'double'

    def _decoder_fn(self) -> str:
        return 'ReadPackedDouble'


class PackedDoubleReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed double values."""

    def _result_type(self) -> str:
        return 'double'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedDouble'


class DoubleFindMethod(FindMethod):
    """Method which reads a proto double value."""

    def _result_type(self) -> str:
        return 'double'

    def _find_fn(self) -> str:
        return 'FindDouble'


class DoubleFindStreamMethod(FindStreamMethod):
    """Method which reads a proto double value."""

    def _result_type(self) -> str:
        return 'double'

    def _find_fn(self) -> str:
        return 'FindDouble'


class DoubleProperty(MessageProperty):
    """Property which holds a proto double value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'double'

    def wire_type(self) -> str:
        return 'kFixed64'

    def _size_fn(self) -> str:
        return 'SizeOfFieldDouble'


class FloatWriteMethod(WriteMethod):
    """Method which writes a proto float value."""

    def params(self) -> list[tuple[str, str]]:
        return [('float', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteFloat'


class PackedFloatWriteMethod(PackedWriteMethod):
    """Method which writes a packed list of floats."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const float>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedFloat'


class PackedFloatWriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of floats."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<float>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedFloat'


class FloatReadMethod(ReadMethod):
    """Method which reads a proto float value."""

    def _result_type(self) -> str:
        return 'float'

    def _decoder_fn(self) -> str:
        return 'ReadFloat'


class PackedFloatReadMethod(PackedReadMethod):
    """Method which reads packed float values."""

    def _result_type(self) -> str:
        return 'float'

    def _decoder_fn(self) -> str:
        return 'ReadPackedFloat'


class PackedFloatReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed float values."""

    def _result_type(self) -> str:
        return 'float'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedFloat'


class FloatFindMethod(FindMethod):
    """Method which reads a proto float value."""

    def _result_type(self) -> str:
        return 'float'

    def _find_fn(self) -> str:
        return 'FindFloat'


class FloatFindStreamMethod(FindStreamMethod):
    """Method which reads a proto float value."""

    def _result_type(self) -> str:
        return 'float'

    def _find_fn(self) -> str:
        return 'FindFloat'


class FloatProperty(MessageProperty):
    """Property which holds a proto float value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'float'

    def wire_type(self) -> str:
        return 'kFixed32'

    def _size_fn(self) -> str:
        return 'SizeOfFieldFloat'


class Int32WriteMethod(WriteMethod):
    """Method which writes a proto int32 value."""

    def params(self) -> list[tuple[str, str]]:
        return [('int32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteInt32'


class PackedInt32WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of int32."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const int32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedInt32'


class PackedInt32WriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of int32."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<int32_t>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedInt32'


class Int32ReadMethod(ReadMethod):
    """Method which reads a proto int32 value."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadInt32'


class PackedInt32ReadMethod(PackedReadMethod):
    """Method which reads packed int32 values."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadPackedInt32'


class PackedInt32ReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed int32 values."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedInt32'


class Int32FindMethod(FindMethod):
    """Method which reads a proto int32 value."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _find_fn(self) -> str:
        return 'FindInt32'


class Int32FindStreamMethod(FindStreamMethod):
    """Method which reads a proto int32 value."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _find_fn(self) -> str:
        return 'FindInt32'


class Int32Property(MessageProperty):
    """Property which holds a proto int32 value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'int32_t'

    def wire_type(self) -> str:
        return 'kVarint'

    def varint_decode_type(self) -> str:
        return 'kNormal'

    def _size_fn(self) -> str:
        return 'SizeOfFieldInt32'


class Sint32WriteMethod(WriteMethod):
    """Method which writes a proto sint32 value."""

    def params(self) -> list[tuple[str, str]]:
        return [('int32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSint32'


class PackedSint32WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of sint32."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const int32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSint32'


class PackedSint32WriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of sint32."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<int32_t>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedSint32'


class Sint32ReadMethod(ReadMethod):
    """Method which reads a proto sint32 value."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadSint32'


class PackedSint32ReadMethod(PackedReadMethod):
    """Method which reads packed sint32 values."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadPackedSint32'


class PackedSint32ReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed sint32 values."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedSint32'


class Sint32FindMethod(FindMethod):
    """Method which reads a proto sint32 value."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _find_fn(self) -> str:
        return 'FindSint32'


class Sint32FindStreamMethod(FindStreamMethod):
    """Method which reads a proto sint32 value."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _find_fn(self) -> str:
        return 'FindSint32'


class Sint32Property(MessageProperty):
    """Property which holds a proto sint32 value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'int32_t'

    def wire_type(self) -> str:
        return 'kVarint'

    def varint_decode_type(self) -> str:
        return 'kZigZag'

    def _size_fn(self) -> str:
        return 'SizeOfFieldSint32'


class Sfixed32WriteMethod(WriteMethod):
    """Method which writes a proto sfixed32 value."""

    def params(self) -> list[tuple[str, str]]:
        return [('int32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSfixed32'


class PackedSfixed32WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of sfixed32."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const int32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSfixed32'


class PackedSfixed32WriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of sfixed32."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<int32_t>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedSfixed32'


class Sfixed32ReadMethod(ReadMethod):
    """Method which reads a proto sfixed32 value."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadSfixed32'


class PackedSfixed32ReadMethod(PackedReadMethod):
    """Method which reads packed sfixed32 values."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadPackedSfixed32'


class PackedSfixed32ReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed sfixed32 values."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedSfixed32'


class Sfixed32FindMethod(FindMethod):
    """Method which reads a proto sfixed32 value."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _find_fn(self) -> str:
        return 'FindSfixed32'


class Sfixed32FindStreamMethod(FindStreamMethod):
    """Method which reads a proto sfixed32 value."""

    def _result_type(self) -> str:
        return 'int32_t'

    def _find_fn(self) -> str:
        return 'FindSfixed32'


class Sfixed32Property(MessageProperty):
    """Property which holds a proto sfixed32 value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'int32_t'

    def wire_type(self) -> str:
        return 'kFixed32'

    def _size_fn(self) -> str:
        return 'SizeOfFieldSfixed32'


class Int64WriteMethod(WriteMethod):
    """Method which writes a proto int64 value."""

    def params(self) -> list[tuple[str, str]]:
        return [('int64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteInt64'


class PackedInt64WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of int64."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const int64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedInt64'


class PackedInt64WriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of int64."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<int64_t>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedInt64'


class Int64ReadMethod(ReadMethod):
    """Method which reads a proto int64 value."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadInt64'


class PackedInt64ReadMethod(PackedReadMethod):
    """Method which reads packed int64 values."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadPackedInt64'


class PackedInt64ReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed int64 values."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedInt64'


class Int64FindMethod(FindMethod):
    """Method which reads a proto int64 value."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _find_fn(self) -> str:
        return 'FindInt64'


class Int64FindStreamMethod(FindStreamMethod):
    """Method which reads a proto int64 value."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _find_fn(self) -> str:
        return 'FindInt64'


class Int64Property(MessageProperty):
    """Property which holds a proto int64 value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'int64_t'

    def wire_type(self) -> str:
        return 'kVarint'

    def varint_decode_type(self) -> str:
        return 'kNormal'

    def _size_fn(self) -> str:
        return 'SizeOfFieldInt64'


class Sint64WriteMethod(WriteMethod):
    """Method which writes a proto sint64 value."""

    def params(self) -> list[tuple[str, str]]:
        return [('int64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSint64'


class PackedSint64WriteMethod(PackedWriteMethod):
    """Method which writes a packst list of sint64."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const int64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSint64'


class PackedSint64WriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of sint64."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<int64_t>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedSint64'


class Sint64ReadMethod(ReadMethod):
    """Method which reads a proto sint64 value."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadSint64'


class PackedSint64ReadMethod(PackedReadMethod):
    """Method which reads packed sint64 values."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadPackedSint64'


class PackedSint64ReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed sint64 values."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedSint64'


class Sint64FindMethod(FindMethod):
    """Method which reads a proto sint64 value."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _find_fn(self) -> str:
        return 'FindSint64'


class Sint64FindStreamMethod(FindStreamMethod):
    """Method which reads a proto sint64 value."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _find_fn(self) -> str:
        return 'FindSint64'


class Sint64Property(MessageProperty):
    """Property which holds a proto sint64 value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'int64_t'

    def wire_type(self) -> str:
        return 'kVarint'

    def varint_decode_type(self) -> str:
        return 'kZigZag'

    def _size_fn(self) -> str:
        return 'SizeOfFieldSint64'


class Sfixed64WriteMethod(WriteMethod):
    """Method which writes a proto sfixed64 value."""

    def params(self) -> list[tuple[str, str]]:
        return [('int64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteSfixed64'


class PackedSfixed64WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of sfixed64."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const int64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedSfixed4'


class PackedSfixed64WriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of sfixed64."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<int64_t>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedSfixed4'


class Sfixed64ReadMethod(ReadMethod):
    """Method which reads a proto sfixed64 value."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadSfixed64'


class PackedSfixed64ReadMethod(PackedReadMethod):
    """Method which reads packed sfixed64 values."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadPackedSfixed64'


class PackedSfixed64ReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed sfixed64 values."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedSfixed64'


class Sfixed64FindMethod(FindMethod):
    """Method which reads a proto sfixed64 value."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _find_fn(self) -> str:
        return 'FindSfixed64'


class Sfixed64FindStreamMethod(FindStreamMethod):
    """Method which reads a proto sfixed64 value."""

    def _result_type(self) -> str:
        return 'int64_t'

    def _find_fn(self) -> str:
        return 'FindSfixed64'


class Sfixed64Property(MessageProperty):
    """Property which holds a proto sfixed64 value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'int64_t'

    def wire_type(self) -> str:
        return 'kFixed64'

    def _size_fn(self) -> str:
        return 'SizeOfFieldSfixed64'


class Uint32WriteMethod(WriteMethod):
    """Method which writes a proto uint32 value."""

    def params(self) -> list[tuple[str, str]]:
        return [('uint32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteUint32'


class PackedUint32WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of uint32."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const uint32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedUint32'


class PackedUint32WriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of uint32."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<uint32_t>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedUint32'


class Uint32ReadMethod(ReadMethod):
    """Method which reads a proto uint32 value."""

    def _result_type(self) -> str:
        return 'uint32_t'

    def _decoder_fn(self) -> str:
        return 'ReadUint32'


class PackedUint32ReadMethod(PackedReadMethod):
    """Method which reads packed uint32 values."""

    def _result_type(self) -> str:
        return 'uint32_t'

    def _decoder_fn(self) -> str:
        return 'ReadPackedUint32'


class PackedUint32ReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed uint32 values."""

    def _result_type(self) -> str:
        return 'uint32_t'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedUint32'


class Uint32FindMethod(FindMethod):
    """Method which finds a proto uint32 value."""

    def _result_type(self) -> str:
        return 'uint32_t'

    def _find_fn(self) -> str:
        return 'FindUint32'


class Uint32FindStreamMethod(FindStreamMethod):
    """Method which finds a proto uint32 value."""

    def _result_type(self) -> str:
        return 'uint32_t'

    def _find_fn(self) -> str:
        return 'FindUint32'


class Uint32Property(MessageProperty):
    """Property which holds a proto uint32 value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'uint32_t'

    def wire_type(self) -> str:
        return 'kVarint'

    def varint_decode_type(self) -> str:
        return 'kUnsigned'

    def _size_fn(self) -> str:
        return 'SizeOfFieldUint32'


class Fixed32WriteMethod(WriteMethod):
    """Method which writes a proto fixed32 value."""

    def params(self) -> list[tuple[str, str]]:
        return [('uint32_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteFixed32'


class PackedFixed32WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of fixed32."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const uint32_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedFixed32'


class PackedFixed32WriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of fixed32."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<uint32_t>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedFixed32'


class Fixed32ReadMethod(ReadMethod):
    """Method which reads a proto fixed32 value."""

    def _result_type(self) -> str:
        return 'uint32_t'

    def _decoder_fn(self) -> str:
        return 'ReadFixed32'


class PackedFixed32ReadMethod(PackedReadMethod):
    """Method which reads packed fixed32 values."""

    def _result_type(self) -> str:
        return 'uint32_t'

    def _decoder_fn(self) -> str:
        return 'ReadPackedFixed32'


class PackedFixed32ReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed fixed32 values."""

    def _result_type(self) -> str:
        return 'uint32_t'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedFixed32'


class Fixed32FindMethod(FindMethod):
    """Method which finds a proto fixed32 value."""

    def _result_type(self) -> str:
        return 'uint32_t'

    def _find_fn(self) -> str:
        return 'FindFixed32'


class Fixed32FindStreamMethod(FindStreamMethod):
    """Method which finds a proto fixed32 value."""

    def _result_type(self) -> str:
        return 'uint32_t'

    def _find_fn(self) -> str:
        return 'FindFixed32'


class Fixed32Property(MessageProperty):
    """Property which holds a proto fixed32 value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'uint32_t'

    def wire_type(self) -> str:
        return 'kFixed32'

    def _size_fn(self) -> str:
        return 'SizeOfFieldFixed32'


class Uint64WriteMethod(WriteMethod):
    """Method which writes a proto uint64 value."""

    def params(self) -> list[tuple[str, str]]:
        return [('uint64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteUint64'


class PackedUint64WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of uint64."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const uint64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedUint64'


class PackedUint64WriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of uint64."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<uint64_t>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedUint64'


class Uint64ReadMethod(ReadMethod):
    """Method which reads a proto uint64 value."""

    def _result_type(self) -> str:
        return 'uint64_t'

    def _decoder_fn(self) -> str:
        return 'ReadUint64'


class PackedUint64ReadMethod(PackedReadMethod):
    """Method which reads packed uint64 values."""

    def _result_type(self) -> str:
        return 'uint64_t'

    def _decoder_fn(self) -> str:
        return 'ReadPackedUint64'


class PackedUint64ReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed uint64 values."""

    def _result_type(self) -> str:
        return 'uint64_t'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedUint64'


class Uint64FindMethod(FindMethod):
    """Method which finds a proto uint64 value."""

    def _result_type(self) -> str:
        return 'uint64_t'

    def _find_fn(self) -> str:
        return 'FindUint64'


class Uint64FindStreamMethod(FindStreamMethod):
    """Method which finds a proto uint64 value."""

    def _result_type(self) -> str:
        return 'uint64_t'

    def _find_fn(self) -> str:
        return 'FindUint64'


class Uint64Property(MessageProperty):
    """Property which holds a proto uint64 value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'uint64_t'

    def wire_type(self) -> str:
        return 'kVarint'

    def varint_decode_type(self) -> str:
        return 'kUnsigned'

    def _size_fn(self) -> str:
        return 'SizeOfFieldUint64'


class Fixed64WriteMethod(WriteMethod):
    """Method which writes a proto fixed64 value."""

    def params(self) -> list[tuple[str, str]]:
        return [('uint64_t', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteFixed64'


class PackedFixed64WriteMethod(PackedWriteMethod):
    """Method which writes a packed list of fixed64."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const uint64_t>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedFixed64'


class PackedFixed64WriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed list of fixed64."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<uint64_t>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedFixed64'


class Fixed64ReadMethod(ReadMethod):
    """Method which reads a proto fixed64 value."""

    def _result_type(self) -> str:
        return 'uint64_t'

    def _decoder_fn(self) -> str:
        return 'ReadFixed64'


class PackedFixed64ReadMethod(PackedReadMethod):
    """Method which reads packed fixed64 values."""

    def _result_type(self) -> str:
        return 'uint64_t'

    def _decoder_fn(self) -> str:
        return 'ReadPackedFixed64'


class PackedFixed64ReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed fixed64 values."""

    def _result_type(self) -> str:
        return 'uint64_t'

    def _decoder_fn(self) -> str:
        return 'ReadRepeatedFixed64'


class Fixed64FindMethod(FindMethod):
    """Method which finds a proto fixed64 value."""

    def _result_type(self) -> str:
        return 'uint64_t'

    def _find_fn(self) -> str:
        return 'FindFixed64'


class Fixed64FindStreamMethod(FindStreamMethod):
    """Method which finds a proto fixed64 value."""

    def _result_type(self) -> str:
        return 'uint64_t'

    def _find_fn(self) -> str:
        return 'FindFixed64'


class Fixed64Property(MessageProperty):
    """Property which holds a proto fixed64 value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'uint64_t'

    def wire_type(self) -> str:
        return 'kFixed64'

    def _size_fn(self) -> str:
        return 'SizeOfFieldFixed64'


class BoolWriteMethod(WriteMethod):
    """Method which writes a proto bool value."""

    def params(self) -> list[tuple[str, str]]:
        return [('bool', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteBool'


class PackedBoolWriteMethod(PackedWriteMethod):
    """Method which writes a packed list of bools."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const bool>', 'values')]

    def _encoder_fn(self) -> str:
        return 'WritePackedBool'


class PackedBoolWriteVectorMethod(PackedWriteMethod):
    """Method which writes a packed vector of bools."""

    def params(self) -> list[tuple[str, str]]:
        return [('const ::pw::Vector<bool>&', 'values')]

    def _encoder_fn(self) -> str:
        return 'WriteRepeatedBool'


class BoolReadMethod(ReadMethod):
    """Method which reads a proto bool value."""

    def _result_type(self) -> str:
        return 'bool'

    def _decoder_fn(self) -> str:
        return 'ReadBool'


class PackedBoolReadMethod(PackedReadMethod):
    """Method which reads packed bool values."""

    def _result_type(self) -> str:
        return 'bool'

    def _decoder_fn(self) -> str:
        return 'ReadPackedBool'


class BoolFindMethod(FindMethod):
    """Method which finds a proto bool value."""

    def _result_type(self) -> str:
        return 'bool'

    def _find_fn(self) -> str:
        return 'FindBool'


class BoolFindStreamMethod(FindStreamMethod):
    """Method which finds a proto bool value."""

    def _result_type(self) -> str:
        return 'bool'

    def _find_fn(self) -> str:
        return 'FindBool'


class BoolProperty(MessageProperty):
    """Property which holds a proto bool value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'bool'

    def wire_type(self) -> str:
        return 'kVarint'

    def varint_decode_type(self) -> str:
        return 'kUnsigned'

    def _size_fn(self) -> str:
        return 'SizeOfFieldBool'


class BytesWriteMethod(WriteMethod):
    """Method which writes a proto bytes value."""

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<const std::byte>', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteBytes'


class BytesReadMethod(ReadMethod):
    """Method which reads a proto bytes value."""

    def return_type(self, from_root: bool = False) -> str:
        return '::pw::StatusWithSize'

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<std::byte>', 'out')]

    def _decoder_fn(self) -> str:
        return 'ReadBytes'


class BytesFindMethod(FindMethod):
    """Method which reads a proto bytes value."""

    def _result_type(self) -> str:
        return '::pw::ConstByteSpan'

    def _find_fn(self) -> str:
        return 'FindBytes'


class BytesFindStreamMethod(FindStreamMethod):
    """Method which reads a proto bytes value."""

    def return_type(self, from_root: bool = False) -> str:
        return '::pw::StatusWithSize'

    def params(self) -> list[tuple[str, str]]:
        return [
            ('::pw::stream::Reader&', 'message_stream'),
            ('::pw::ByteSpan', 'out'),
        ]

    def body(self) -> list[str]:
        lines: list[str] = []
        lines += [
            f'return {PROTOBUF_NAMESPACE}::{self._find_fn()}'
            f'(message_stream, {self.field_cast()}, out);'
        ]
        return lines

    def _find_fn(self) -> str:
        return 'FindBytes'


class BytesProperty(MessageProperty):
    """Property which holds a proto bytes value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'std::byte'

    def use_callback(self) -> bool:
        return self.max_size() == 0

    def max_size(self) -> int:
        if not self._field.is_repeated():
            options = self._field.options()
            assert options is not None
            return options.max_size

        return 0

    def is_fixed_size(self) -> bool:
        if not self._field.is_repeated():
            options = self._field.options()
            assert options is not None
            return options.fixed_size

        return False

    def wire_type(self) -> str:
        return 'kDelimited'

    def _size_fn(self) -> str:
        # This uses the WithoutValue method to ensure that the maximum length
        # of the delimited field size varint is used. This accounts for scratch
        # overhead when used with MemoryEncoder.
        return 'SizeOfDelimitedFieldWithoutValue'

    def _size_length(self) -> Optional[str]:
        if self.use_callback():
            return None
        return self.max_size_constant_name()


class StringLenWriteMethod(WriteMethod):
    """Method which writes a proto string value with length."""

    def params(self) -> list[tuple[str, str]]:
        return [('const char*', 'value'), ('size_t', 'len')]

    def _encoder_fn(self) -> str:
        return 'WriteString'


class StringWriteMethod(WriteMethod):
    """Method which writes a proto string value."""

    def params(self) -> list[tuple[str, str]]:
        return [('std::string_view', 'value')]

    def _encoder_fn(self) -> str:
        return 'WriteString'


class StringReadMethod(ReadMethod):
    """Method which reads a proto string value."""

    def return_type(self, from_root: bool = False) -> str:
        return '::pw::StatusWithSize'

    def params(self) -> list[tuple[str, str]]:
        return [('pw::span<char>', 'out')]

    def _decoder_fn(self) -> str:
        return 'ReadString'


class StringFindMethod(FindMethod):
    """Method which reads a proto string value."""

    def _result_type(self) -> str:
        return 'std::string_view'

    def _find_fn(self) -> str:
        return 'FindString'


class StringFindStreamMethod(FindStreamMethod):
    """Method which reads a proto string value."""

    def return_type(self, from_root: bool = False) -> str:
        return '::pw::StatusWithSize'

    def params(self) -> list[tuple[str, str]]:
        return [
            ('::pw::stream::Reader&', 'message_stream'),
            ('::pw::span<char>', 'out'),
        ]

    def body(self) -> list[str]:
        lines: list[str] = []
        lines += [
            f'return {PROTOBUF_NAMESPACE}::{self._find_fn()}'
            f'(message_stream, {self.field_cast()}, out);'
        ]
        return lines

    def _find_fn(self) -> str:
        return 'FindString'


class StringFindStreamMethodInlineString(FindStreamMethod):
    """Method which reads a proto string value to an InlineString."""

    def return_type(self, from_root: bool = False) -> str:
        return '::pw::StatusWithSize'

    def params(self) -> list[tuple[str, str]]:
        return [
            ('::pw::stream::Reader&', 'message_stream'),
            ('::pw::InlineString<>&', 'out'),
        ]

    def body(self) -> list[str]:
        lines: list[str] = []
        lines += [
            f'return {PROTOBUF_NAMESPACE}::{self._find_fn()}'
            f'(message_stream, {self.field_cast()}, out);'
        ]
        return lines

    def _find_fn(self) -> str:
        return 'FindString'


class StringProperty(MessageProperty):
    """Property which holds a proto string value."""

    def type_name(self, from_root: bool = False) -> str:
        return 'char'

    def use_callback(self) -> bool:
        return self.max_size() == 0

    def max_size(self) -> int:
        if not self._field.is_repeated():
            options = self._field.options()
            assert options is not None
            return options.max_size

        return 0

    def is_fixed_size(self) -> bool:
        return False

    def wire_type(self) -> str:
        return 'kDelimited'

    def is_string(self) -> bool:
        return True

    @staticmethod
    def repeated_field_container(type_name: str, max_size: str) -> str:
        return f'::pw::InlineBasicString<{type_name}, {max_size}>'

    def _size_fn(self) -> str:
        # This uses the WithoutValue method to ensure that the maximum length
        # of the delimited field size varint is used. This accounts for scratch
        # overhead when used with MemoryEncoder.
        return 'SizeOfDelimitedFieldWithoutValue'

    def _size_length(self) -> Optional[str]:
        if self.use_callback():
            return None
        return self.max_size_constant_name()


class EnumWriteMethod(WriteMethod):
    """Method which writes a proto enum value."""

    def params(self) -> list[tuple[str, str]]:
        return [(self._relative_type_namespace(), 'value')]

    def body(self) -> list[str]:
        line = (
            'return {}::WriteUint32({}, '
            'static_cast<uint32_t>(value));'.format(
                self._base_class, self.field_cast()
            )
        )
        return [line]

    def in_class_definition(self) -> bool:
        return True

    def _encoder_fn(self) -> str:
        raise NotImplementedError()


class PackedEnumWriteMethod(PackedWriteMethod):
    """Method which writes a packed list of enum."""

    def params(self) -> list[tuple[str, str]]:
        return [
            (
                'pw::span<const {}>'.format(self._relative_type_namespace()),
                'values',
            )
        ]

    def body(self) -> list[str]:
        value_param = self.params()[0][1]
        line = (
            f'return {self._base_class}::WritePackedUint32('
            f'{self.field_cast()}, pw::span(reinterpret_cast<const uint32_t*>('
            f'{value_param}.data()), {value_param}.size()));'
        )
        return [line]

    def in_class_definition(self) -> bool:
        return True

    def _encoder_fn(self) -> str:
        raise NotImplementedError()


class PackedEnumWriteVectorMethod(PackedEnumWriteMethod):
    """Method which writes a packed vector of enum."""

    def params(self) -> list[tuple[str, str]]:
        return [
            (
                'const ::pw::Vector<{}>&'.format(
                    self._relative_type_namespace()
                ),
                'values',
            )
        ]


class EnumReadMethod(ReadMethod):
    """Method which reads a proto enum value."""

    def _result_type(self):
        return self._relative_type_namespace()

    def _decoder_body(self) -> list[str]:
        lines: list[str] = []
        lines += ['::pw::Result<uint32_t> value = ReadUint32();']
        lines += ['if (!value.ok()) {']
        lines += ['  return value.status();']
        lines += ['}']

        lines += [f'return static_cast<{self._result_type()}>(value.value());']
        return lines


class PackedEnumReadMethod(PackedReadMethod):
    """Method which reads packed enum values."""

    def _result_type(self):
        return self._relative_type_namespace()

    def _decoder_body(self) -> list[str]:
        value_param = self.params()[0][1]
        return [
            f'return ReadPackedUint32('
            f'pw::span(reinterpret_cast<uint32_t*>({value_param}.data()), '
            f'{value_param}.size()));'
        ]


class PackedEnumReadVectorMethod(PackedReadVectorMethod):
    """Method which reads packed enum values."""

    def _result_type(self):
        return self._relative_type_namespace()

    def _decoder_body(self) -> list[str]:
        value_param = self.params()[0][1]
        return [
            f'return ReadRepeatedUint32('
            f'*reinterpret_cast<pw::Vector<uint32_t>*>(&{value_param}));'
        ]


class EnumFindMethod(FindMethod):
    """Method which finds a proto enum value."""

    def _result_type(self) -> str:
        return self._relative_type_namespace()

    def body(self) -> list[str]:
        lines: list[str] = []
        lines += [
            '::pw::Result<uint32_t> result = '
            f'{PROTOBUF_NAMESPACE}::{self._find_fn()}'
            f'(message, {self.field_cast()});',
            'if (!result.ok()) {',
            '  return result.status();',
            '}',
            f'return static_cast<{self._result_type()}>(result.value());',
        ]
        return lines

    def _find_fn(self) -> str:
        return 'FindUint32'


class EnumFindStreamMethod(FindStreamMethod):
    """Method which finds a proto enum value."""

    def _result_type(self) -> str:
        return self._relative_type_namespace()

    def body(self) -> list[str]:
        lines: list[str] = []
        lines += [
            '::pw::Result<uint32_t> result = '
            f'{PROTOBUF_NAMESPACE}::{self._find_fn()}'
            f'(message_stream, {self.field_cast()});',
            'if (!result.ok()) {',
            '  return result.status();',
            '}',
            f'return static_cast<{self._result_type()}>(result.value());',
        ]
        return lines

    def _find_fn(self) -> str:
        return 'FindUint32'


class EnumProperty(MessageProperty):
    """Property which holds a proto enum value."""

    def type_name(self, from_root: bool = False) -> str:
        return self._relative_type_namespace(from_root=from_root)

    def wire_type(self) -> str:
        return 'kVarint'

    def varint_decode_type(self) -> str:
        return 'kUnsigned'

    def _size_fn(self) -> str:
        return 'SizeOfFieldEnum'


# Mapping of protobuf field types to their method definitions.
PROTO_FIELD_WRITE_METHODS: Dict[int, list] = {
    descriptor_pb2.FieldDescriptorProto.TYPE_DOUBLE: [
        DoubleWriteMethod,
        PackedDoubleWriteMethod,
        PackedDoubleWriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT: [
        FloatWriteMethod,
        PackedFloatWriteMethod,
        PackedFloatWriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT32: [
        Int32WriteMethod,
        PackedInt32WriteMethod,
        PackedInt32WriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT32: [
        Sint32WriteMethod,
        PackedSint32WriteMethod,
        PackedSint32WriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED32: [
        Sfixed32WriteMethod,
        PackedSfixed32WriteMethod,
        PackedSfixed32WriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT64: [
        Int64WriteMethod,
        PackedInt64WriteMethod,
        PackedInt64WriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT64: [
        Sint64WriteMethod,
        PackedSint64WriteMethod,
        PackedSint64WriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED64: [
        Sfixed64WriteMethod,
        PackedSfixed64WriteMethod,
        PackedSfixed64WriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT32: [
        Uint32WriteMethod,
        PackedUint32WriteMethod,
        PackedUint32WriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32: [
        Fixed32WriteMethod,
        PackedFixed32WriteMethod,
        PackedFixed32WriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT64: [
        Uint64WriteMethod,
        PackedUint64WriteMethod,
        PackedUint64WriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64: [
        Fixed64WriteMethod,
        PackedFixed64WriteMethod,
        PackedFixed64WriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_BOOL: [
        BoolWriteMethod,
        PackedBoolWriteMethod,
        PackedBoolWriteVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_BYTES: [BytesWriteMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_STRING: [
        StringLenWriteMethod,
        StringWriteMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE: [SubMessageEncoderMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_ENUM: [
        EnumWriteMethod,
        PackedEnumWriteMethod,
        PackedEnumWriteVectorMethod,
    ],
}

PROTO_FIELD_READ_METHODS: Dict[int, list] = {
    descriptor_pb2.FieldDescriptorProto.TYPE_DOUBLE: [
        DoubleReadMethod,
        PackedDoubleReadMethod,
        PackedDoubleReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT: [
        FloatReadMethod,
        PackedFloatReadMethod,
        PackedFloatReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT32: [
        Int32ReadMethod,
        PackedInt32ReadMethod,
        PackedInt32ReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT32: [
        Sint32ReadMethod,
        PackedSint32ReadMethod,
        PackedSint32ReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED32: [
        Sfixed32ReadMethod,
        PackedSfixed32ReadMethod,
        PackedSfixed32ReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT64: [
        Int64ReadMethod,
        PackedInt64ReadMethod,
        PackedInt64ReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT64: [
        Sint64ReadMethod,
        PackedSint64ReadMethod,
        PackedSint64ReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED64: [
        Sfixed64ReadMethod,
        PackedSfixed64ReadMethod,
        PackedSfixed64ReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT32: [
        Uint32ReadMethod,
        PackedUint32ReadMethod,
        PackedUint32ReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32: [
        Fixed32ReadMethod,
        PackedFixed32ReadMethod,
        PackedFixed32ReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT64: [
        Uint64ReadMethod,
        PackedUint64ReadMethod,
        PackedUint64ReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64: [
        Fixed64ReadMethod,
        PackedFixed64ReadMethod,
        PackedFixed64ReadVectorMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_BOOL: [
        BoolReadMethod,
        PackedBoolReadMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_BYTES: [
        BytesReadMethod,
        BytesReaderMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_STRING: [
        StringReadMethod,
        BytesReaderMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE: [SubMessageDecoderMethod],
    descriptor_pb2.FieldDescriptorProto.TYPE_ENUM: [
        EnumReadMethod,
        PackedEnumReadMethod,
        PackedEnumReadVectorMethod,
    ],
}

PROTO_FIELD_FIND_METHODS: Dict[int, list] = {
    descriptor_pb2.FieldDescriptorProto.TYPE_DOUBLE: [
        DoubleFindMethod,
        DoubleFindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT: [
        FloatFindMethod,
        FloatFindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT32: [
        Int32FindMethod,
        Int32FindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT32: [
        Sint32FindMethod,
        Sint32FindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED32: [
        Sfixed32FindMethod,
        Sfixed32FindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_INT64: [
        Int64FindMethod,
        Int64FindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT64: [
        Sint64FindMethod,
        Sint64FindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED64: [
        Sfixed64FindMethod,
        Sfixed64FindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT32: [
        Uint32FindMethod,
        Uint32FindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32: [
        Fixed32FindMethod,
        Fixed32FindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT64: [
        Uint64FindMethod,
        Uint64FindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64: [
        Fixed64FindMethod,
        Fixed64FindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_BOOL: [
        BoolFindMethod,
        BoolFindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_BYTES: [
        BytesFindMethod,
        BytesFindStreamMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_STRING: [
        StringFindMethod,
        StringFindStreamMethod,
        StringFindStreamMethodInlineString,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE: [
        SubMessageFindMethod,
    ],
    descriptor_pb2.FieldDescriptorProto.TYPE_ENUM: [
        EnumFindMethod,
        EnumFindStreamMethod,
    ],
}

PROTO_FIELD_PROPERTIES: Dict[int, Type[MessageProperty]] = {
    descriptor_pb2.FieldDescriptorProto.TYPE_DOUBLE: DoubleProperty,
    descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT: FloatProperty,
    descriptor_pb2.FieldDescriptorProto.TYPE_INT32: Int32Property,
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT32: Sint32Property,
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED32: Sfixed32Property,
    descriptor_pb2.FieldDescriptorProto.TYPE_INT64: Int64Property,
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT64: Sint64Property,
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED64: Sfixed32Property,
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT32: Uint32Property,
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32: Fixed32Property,
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT64: Uint64Property,
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64: Fixed64Property,
    descriptor_pb2.FieldDescriptorProto.TYPE_BOOL: BoolProperty,
    descriptor_pb2.FieldDescriptorProto.TYPE_BYTES: BytesProperty,
    descriptor_pb2.FieldDescriptorProto.TYPE_STRING: StringProperty,
    descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE: SubMessageProperty,
    descriptor_pb2.FieldDescriptorProto.TYPE_ENUM: EnumProperty,
}


def proto_message_field_props(
    message: ProtoMessage,
    root: ProtoNode,
) -> Iterable[MessageProperty]:
    """Yields a MessageProperty for each field in a ProtoMessage.

    Only properties which should_appear() is True are returned.

    Args:
      message: The ProtoMessage whose fields are iterated.
      root: The root ProtoNode of the tree.

    Yields:
      An appropriately-typed MessageProperty object for each field
      in the message, to which the property refers.
    """
    for field in message.fields():
        property_class = PROTO_FIELD_PROPERTIES[field.type()]
        prop = property_class(field, message, root)
        if prop.should_appear():
            yield prop


def proto_field_methods(class_type: ClassType, field_type: int) -> list:
    return (
        PROTO_FIELD_WRITE_METHODS[field_type]
        if class_type.is_encoder()
        else PROTO_FIELD_READ_METHODS[field_type]
    )


def generate_class_for_message(
    message: ProtoMessage,
    root: ProtoNode,
    output: OutputFile,
    class_type: ClassType,
) -> None:
    """Creates a C++ class to encode or decoder a protobuf message."""
    assert message.type() == ProtoNode.Type.MESSAGE

    base_class_name = class_type.base_class_name()
    class_name = class_type.codegen_class_name()

    # Message classes inherit from the base proto message class in codegen.h
    # and use its constructor.
    base_class = f'{PROTOBUF_NAMESPACE}::{base_class_name}'
    output.write_line(
        f'class {message.cpp_namespace(root=root)}::{class_name} '
        f': public {base_class} {{'
    )
    output.write_line(' public:')

    with output.indent():
        # Inherit the constructors from the base class.
        output.write_line(f'using {base_class}::{base_class_name};')

        # Declare a move constructor that takes a base class.
        output.write_line(
            f'constexpr {class_name}({base_class}&& parent) '
            f': {base_class}(std::move(parent)) {{}}'
        )

        # Allow MemoryEncoder& to be converted to StreamEncoder&.
        if class_type == ClassType.MEMORY_ENCODER:
            stream_type = (
                f'::{message.cpp_namespace()}::'
                f'{ClassType.STREAMING_ENCODER.codegen_class_name()}'
            )
            output.write_line(
                f'operator {stream_type}&() '
                f' {{ return static_cast<{stream_type}&>('
                f'*static_cast<{PROTOBUF_NAMESPACE}::StreamEncoder*>(this));}}'
            )

        # Add a typed Field() member to StreamDecoder
        if class_type == ClassType.STREAMING_DECODER:
            output.write_line()
            output.write_line('::pw::Result<Fields> Field() {')
            with output.indent():
                output.write_line(
                    '::pw::Result<uint32_t> result ' '= FieldNumber();'
                )
                output.write_line('if (!result.ok()) {')
                with output.indent():
                    output.write_line('return result.status();')
                output.write_line('}')
                output.write_line('return static_cast<Fields>(result.value());')
            output.write_line('}')

        # Generate entry for message table read or write methods.
        if class_type == ClassType.STREAMING_DECODER:
            output.write_line()
            output.write_line('::pw::Status Read(Message& message) {')
            with output.indent():
                output.write_line(
                    f'return {base_class}::Read('
                    'pw::as_writable_bytes(pw::span(&message, 1)), '
                    'kMessageFields);'
                )
            output.write_line('}')
        elif class_type in (
            ClassType.STREAMING_ENCODER,
            ClassType.MEMORY_ENCODER,
        ):
            output.write_line()
            output.write_line('::pw::Status Write(const Message& message) {')
            with output.indent():
                output.write_line(
                    f'return {base_class}::Write('
                    'pw::as_bytes(pw::span(&message, 1)), kMessageFields);'
                )
            output.write_line('}')

        # Generate methods for each of the message's fields.
        for field in message.fields():
            for method_class in proto_field_methods(class_type, field.type()):
                method = method_class(field, message, root, base_class)
                if not method.should_appear():
                    continue

                output.write_line()
                method_signature = (
                    f'{method.return_type()} '
                    f'{method.name()}({method.param_string()})'
                )

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
    message: ProtoMessage,
    root: ProtoNode,
    output: OutputFile,
    class_type: ClassType,
) -> None:
    """Defines methods for a message class that were previously declared."""
    assert message.type() == ProtoNode.Type.MESSAGE

    base_class_name = class_type.base_class_name()
    base_class = f'{PROTOBUF_NAMESPACE}::{base_class_name}'

    for field in message.fields():
        for method_class in proto_field_methods(class_type, field.type()):
            method = method_class(field, message, root, base_class)
            if not method.should_appear() or method.in_class_definition():
                continue

            output.write_line()
            class_name = (
                f'{message.cpp_namespace(root=root)}::'
                f'{class_type.codegen_class_name()}'
            )
            method_signature = (
                f'inline {method.return_type(from_root=True)} '
                f'{class_name}::{method.name()}({method.param_string()})'
            )
            output.write_line(f'{method_signature} {{')
            with output.indent():
                for line in method.body():
                    output.write_line(line)
            output.write_line('}')


def _common_value_prefix(proto_enum: ProtoEnum) -> str:
    """Calculate the common prefix of all enum values.

    Given an enumeration:
        enum Thing {
            THING_ONE = 1;
            THING_TWO = 2;
            THING_THREE = 3;
        }

    If will return 'THING_', resulting in generated "style" aliases of
    'kOne', 'kTwo', and 'kThree'.

    The prefix is walked back to the last _, so that the enumeration:
        enum Activity {
            ACTIVITY_RUN = 1;
            ACTIVITY_ROW = 2;
        }

    Returns 'ACTIVITY_' and not 'ACTIVITY_R'.
    """
    if len(proto_enum.values()) <= 1:
        return ''

    common_prefix = "".join(
        ch[0]
        for ch in takewhile(
            lambda ch: all(ch[0] == c for c in ch),
            zip(*[name for name, _ in proto_enum.values()]),
        )
    )
    (left, under, _) = common_prefix.rpartition('_')
    return left + under


def generate_code_for_enum(
    proto_enum: ProtoEnum, root: ProtoNode, output: OutputFile
) -> None:
    """Creates a C++ enum for a proto enum."""
    assert proto_enum.type() == ProtoNode.Type.ENUM

    common_prefix = _common_value_prefix(proto_enum)
    output.write_line(
        f'enum class {proto_enum.cpp_namespace(root=root)} ' f': uint32_t {{'
    )
    with output.indent():
        for name, number in proto_enum.values():
            output.write_line(f'{name} = {number},')

            style_name = 'k' + ProtoMessageField.upper_camel_case(
                name[len(common_prefix) :]
            )
            if style_name != name:
                output.write_line(f'{style_name} = {name},')

    output.write_line('};')


def generate_function_for_enum(
    proto_enum: ProtoEnum, root: ProtoNode, output: OutputFile
) -> None:
    """Creates a C++ validation function for a proto enum."""
    assert proto_enum.type() == ProtoNode.Type.ENUM

    enum_name = proto_enum.cpp_namespace(root=root)
    output.write_line(
        f'constexpr bool IsValid{enum_name}({enum_name} value) {{'
    )
    with output.indent():
        output.write_line('switch (value) {')
        with output.indent():
            for name, _ in proto_enum.values():
                output.write_line(f'case {enum_name}::{name}: return true;')
            output.write_line('default: return false;')
        output.write_line('}')
    output.write_line('}')


def generate_to_string_for_enum(
    proto_enum: ProtoEnum, root: ProtoNode, output: OutputFile
) -> None:
    """Creates a C++ to string function for a proto enum."""
    assert proto_enum.type() == ProtoNode.Type.ENUM

    enum_name = proto_enum.cpp_namespace(root=root)
    output.write_line(
        f'// Returns string names for {enum_name}; '
        'returns "" for invalid enum values.'
    )
    output.write_line(
        f'constexpr const char* {enum_name}ToString({enum_name} value) {{'
    )
    with output.indent():
        output.write_line('switch (value) {')
        with output.indent():
            for name, _ in proto_enum.values():
                output.write_line(f'case {enum_name}::{name}: return "{name}";')
            output.write_line('default: return "";')
        output.write_line('}')
    output.write_line('}')


def forward_declare(
    message: ProtoMessage,
    root: ProtoNode,
    output: OutputFile,
    exclude_legacy_snake_case_field_name_enums: bool,
) -> None:
    """Generates code forward-declaring entities in a message's namespace."""
    namespace = message.cpp_namespace(root=root)
    output.write_line()
    output.write_line(f'namespace {namespace} {{')

    # Define an enum defining each of the message's fields and their numbers.
    output.write_line('enum class Fields : uint32_t {')
    with output.indent():
        for field in message.fields():
            output.write_line(f'{field.enum_name()} = {field.number()},')

        # Migration support from SNAKE_CASE to kConstantCase.
        if not exclude_legacy_snake_case_field_name_enums:
            for field in message.fields():
                output.write_line(
                    f'{field.legacy_enum_name()} = {field.number()},'
                )

    output.write_line('};')

    # Define constants for fixed-size fields.
    output.write_line()
    for prop in proto_message_field_props(message, root):
        max_size = prop.max_size()
        if max_size:
            output.write_line(
                f'static constexpr size_t {prop.max_size_constant_name()} '
                f'= {max_size};'
            )

    # Declare the message's message struct.
    output.write_line()
    output.write_line('struct Message;')

    # Declare the message's encoder classes.
    output.write_line()
    output.write_line('class StreamEncoder;')
    output.write_line('class MemoryEncoder;')

    # Declare the message's decoder classes.
    output.write_line()
    output.write_line('class StreamDecoder;')

    # Declare the message's enums.
    for child in message.children():
        if child.type() == ProtoNode.Type.ENUM:
            output.write_line()
            generate_code_for_enum(cast(ProtoEnum, child), message, output)
            output.write_line()
            generate_function_for_enum(cast(ProtoEnum, child), message, output)
            output.write_line()
            generate_to_string_for_enum(cast(ProtoEnum, child), message, output)

    output.write_line(f'}}  // namespace {namespace}')


def generate_struct_for_message(
    message: ProtoMessage, root: ProtoNode, output: OutputFile
) -> None:
    """Creates a C++ struct to hold a protobuf message values."""
    assert message.type() == ProtoNode.Type.MESSAGE

    output.write_line(f'struct {message.cpp_namespace(root=root)}::Message {{')

    # Generate members for each of the message's fields.
    with output.indent():
        cmp: list[str] = []
        for prop in proto_message_field_props(message, root):
            type_name = prop.struct_member_type()
            name = prop.name()
            output.write_line(f'{type_name} {name};')

            if not prop.use_callback():
                cmp.append(f'this->{name} == other.{name}')

        # Equality operator
        output.write_line()
        output.write_line('bool operator==(const Message& other) const {')
        with output.indent():
            if len(cmp) > 0:
                output.write_line(f'return {" && ".join(cmp)};')
            else:
                output.write_line('static_cast<void>(other);')
                output.write_line('return true;')
        output.write_line('}')
        output.write_line(
            'bool operator!=(const Message& other) const '
            '{ return !(*this == other); }'
        )

    output.write_line('};')


def generate_table_for_message(
    message: ProtoMessage, root: ProtoNode, output: OutputFile
) -> None:
    """Creates a C++ array to hold a protobuf message description."""
    assert message.type() == ProtoNode.Type.MESSAGE

    namespace = message.cpp_namespace(root=root)
    output.write_line(f'namespace {namespace} {{')

    properties = list(proto_message_field_props(message, root))

    output.write_line('PW_MODIFY_DIAGNOSTICS_PUSH();')
    output.write_line('PW_MODIFY_DIAGNOSTIC(ignored, "-Winvalid-offsetof");')

    # Generate static_asserts to fail at compile-time if the structure cannot
    # be converted into a table.
    for idx, prop in enumerate(properties):
        if idx > 0:
            output.write_line(
                'static_assert(offsetof(Message, {}) > 0);'.format(prop.name())
            )
        output.write_line(
            'static_assert(sizeof(Message::{}) <= '
            '{}::MessageField::kMaxFieldSize);'.format(
                prop.name(), _INTERNAL_NAMESPACE
            )
        )

    # Zero-length C arrays are not permitted by the C++ standard, so only
    # generate the message fields array if it is non-empty. Zero-length
    # std::arrays are valid, but older toolchains may not support constexpr
    # std::arrays, even with -std=c++17.
    #
    # The kMessageFields span is generated whether the message has fields or
    # not. Only the span is referenced elsewhere.
    if properties:
        output.write_line(
            f'inline constexpr {_INTERNAL_NAMESPACE}::MessageField '
            ' _kMessageFields[] = {'
        )

        # Generate members for each of the message's fields.
        with output.indent():
            for prop in properties:
                table = ', '.join(prop.table_entry())
                output.write_line(f'{{{table}}},')

        output.write_line('};')
        output.write_line('PW_MODIFY_DIAGNOSTICS_POP();')

        output.write_line(
            f'inline constexpr pw::span<const {_INTERNAL_NAMESPACE}::'
            'MessageField> kMessageFields = _kMessageFields;'
        )

        member_list = ', '.join(
            [f'message.{prop.name()}' for prop in properties]
        )

        # Generate std::tuple for Message fields.
        output.write_line(
            'inline constexpr auto ToTuple(const Message &message) {'
        )
        output.write_line(f'  return std::tie({member_list});')
        output.write_line('}')

        # Generate mutable std::tuple for Message fields.
        output.write_line(
            'inline constexpr auto ToMutableTuple(Message &message) {'
        )
        output.write_line(f'  return std::tie({member_list});')
        output.write_line('}')
    else:
        output.write_line(
            f'inline constexpr pw::span<const {_INTERNAL_NAMESPACE}::'
            'MessageField> kMessageFields;'
        )

    output.write_line(f'}}  // namespace {namespace}')


def generate_sizes_for_message(
    message: ProtoMessage, root: ProtoNode, output: OutputFile
) -> None:
    """Creates C++ constants for the encoded sizes of a protobuf message."""
    assert message.type() == ProtoNode.Type.MESSAGE

    namespace = message.cpp_namespace(root=root)
    output.write_line(f'namespace {namespace} {{')

    property_sizes: list[str] = []
    scratch_sizes: list[str] = []
    for prop in proto_message_field_props(message, root):
        property_sizes.append(prop.max_encoded_size())
        if prop.include_in_scratch_size():
            scratch_sizes.append(prop.max_encoded_size())

    output.write_line('inline constexpr size_t kMaxEncodedSizeBytes =')
    with output.indent():
        if len(property_sizes) == 0:
            output.write_line('0;')
        while len(property_sizes) > 0:
            property_size = property_sizes.pop(0)
            if len(property_sizes) > 0:
                output.write_line(f'{property_size} +')
            else:
                output.write_line(f'{property_size};')

    output.write_line()
    output.write_line(
        'inline constexpr size_t kScratchBufferSizeBytes = '
        + ('std::max({' if len(scratch_sizes) > 0 else '0;')
    )
    with output.indent():
        for scratch_size in scratch_sizes:
            output.write_line(f'{scratch_size},')
    if len(scratch_sizes) > 0:
        output.write_line('});')

    output.write_line(f'}}  // namespace {namespace}')


def generate_find_functions_for_message(
    message: ProtoMessage, root: ProtoNode, output: OutputFile
) -> None:
    """Creates C++ constants for the encoded sizes of a protobuf message."""
    assert message.type() == ProtoNode.Type.MESSAGE

    namespace = message.cpp_namespace(root=root)
    output.write_line(f'namespace {namespace} {{')

    for field in message.fields():
        if field.is_repeated():
            # Find methods don't account for repeated field semantics, so
            # ignore them to avoid confusion.
            continue

        try:
            methods = PROTO_FIELD_FIND_METHODS[field.type()]
        except KeyError:
            continue

        for cls in methods:
            method = cls(field, message, root, '')
            method_signature = (
                f'inline {method.return_type()} '
                f'{method.name()}({method.param_string()})'
            )

            output.write_line()
            output.write_line(f'{method_signature} {{')

            with output.indent():
                for line in method.body():
                    output.write_line(line)

            output.write_line('}')

    output.write_line(f'}}  // namespace {namespace}')


def generate_is_trivially_comparable_specialization(
    message: ProtoMessage, root: ProtoNode, output: OutputFile
) -> None:
    is_trivially_comparable = True
    for prop in proto_message_field_props(message, root):
        if prop.use_callback():
            is_trivially_comparable = False
            break

    qualified_message = f'::{message.cpp_namespace()}::Message'

    output.write_line('template <>')
    output.write_line(
        'constexpr bool IsTriviallyComparable' f'<{qualified_message}>() {{'
    )
    output.write_line(f'  return {str(is_trivially_comparable).lower()};')
    output.write_line('}')


def _proto_filename_to_generated_header(proto_file: str) -> str:
    """Returns the generated C++ header name for a .proto file."""
    return os.path.splitext(proto_file)[0] + PROTO_H_EXTENSION


def dependency_sorted_messages(package: ProtoNode):
    """Yields the messages in the package sorted after their dependencies."""

    # Build the graph of dependencies between messages.
    graph: Dict[ProtoMessage, list[ProtoMessage]] = {}
    for node in package:
        if node.type() == ProtoNode.Type.MESSAGE:
            message = cast(ProtoMessage, node)
            graph[message] = message.dependencies()

    # Repeatedly prepare a topological sort of the dependency graph, removing
    # a dependency each time a cycle is a detected, until we're left with a
    # fully directed graph.
    tsort: TopologicalSorter
    while True:
        tsort = TopologicalSorter(graph)
        try:
            tsort.prepare()
            break
        except CycleError as err:
            dependency, message = err.args[1][0], err.args[1][1]
            message.remove_dependency_cycle(dependency)
            graph[message] = message.dependencies()

    # Yield the messages from the sorted graph.
    while tsort.is_active():
        messages = tsort.get_ready()
        yield from messages
        tsort.done(*messages)


def generate_code_for_package(
    file_descriptor_proto,
    package: ProtoNode,
    output: OutputFile,
    suppress_legacy_namespace: bool,
    exclude_legacy_snake_case_field_name_enums: bool,
) -> None:
    """Generates code for a single .pb.h file corresponding to a .proto file."""

    assert package.type() == ProtoNode.Type.PACKAGE

    output.write_line(
        f'// {os.path.basename(output.name())} automatically '
        f'generated by {PLUGIN_NAME} {PLUGIN_VERSION}'
    )
    output.write_line('#pragma once\n')
    output.write_line('#include <algorithm>')
    output.write_line('#include <array>')
    output.write_line('#include <cstddef>')
    output.write_line('#include <cstdint>')
    output.write_line('#include <optional>')
    output.write_line('#include <string_view>\n')
    output.write_line('#include "pw_assert/assert.h"')
    output.write_line('#include "pw_containers/vector.h"')
    output.write_line('#include "pw_preprocessor/compiler.h"')
    output.write_line('#include "pw_protobuf/encoder.h"')
    output.write_line('#include "pw_protobuf/find.h"')
    output.write_line('#include "pw_protobuf/internal/codegen.h"')
    output.write_line('#include "pw_protobuf/serialized_size.h"')
    output.write_line('#include "pw_protobuf/stream_decoder.h"')
    output.write_line('#include "pw_result/result.h"')
    output.write_line('#include "pw_span/span.h"')
    output.write_line('#include "pw_status/status.h"')
    output.write_line('#include "pw_status/status_with_size.h"')
    output.write_line('#include "pw_string/string.h"')

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
            forward_declare(
                cast(ProtoMessage, node),
                package,
                output,
                exclude_legacy_snake_case_field_name_enums,
            )

    # Define all top-level enums.
    for node in package.children():
        if node.type() == ProtoNode.Type.ENUM:
            output.write_line()
            generate_code_for_enum(cast(ProtoEnum, node), package, output)
            output.write_line()
            generate_function_for_enum(cast(ProtoEnum, node), package, output)
            output.write_line()
            generate_to_string_for_enum(cast(ProtoEnum, node), package, output)

    # Run through all messages, generating structs and classes for each.
    messages = []
    for message in dependency_sorted_messages(package):
        output.write_line()
        generate_struct_for_message(message, package, output)
        output.write_line()
        generate_table_for_message(message, package, output)
        output.write_line()
        generate_sizes_for_message(message, package, output)
        output.write_line()
        generate_find_functions_for_message(message, package, output)
        output.write_line()
        generate_class_for_message(
            message, package, output, ClassType.STREAMING_ENCODER
        )
        output.write_line()
        generate_class_for_message(
            message, package, output, ClassType.MEMORY_ENCODER
        )
        output.write_line()
        generate_class_for_message(
            message, package, output, ClassType.STREAMING_DECODER
        )
        messages.append(message)

    # Run a second pass through the messages, this time defining all of the
    # methods which were previously only declared.
    for message in messages:
        define_not_in_class_methods(
            message, package, output, ClassType.STREAMING_ENCODER
        )
        define_not_in_class_methods(
            message, package, output, ClassType.MEMORY_ENCODER
        )
        define_not_in_class_methods(
            message, package, output, ClassType.STREAMING_DECODER
        )

    if package.cpp_namespace():
        output.write_line(f'\n}}  // namespace {package.cpp_namespace()}')

        # Aliasing namespaces aren't needed if `package.cpp_namespace()` is
        # empty (since everyone can see the global namespace). It shouldn't
        # ever be empty, though.

        if not suppress_legacy_namespace:
            output.write_line()
            output.write_line(
                '// Aliases for legacy pwpb codegen interface. '
                'Please use the'
            )
            output.write_line('// `::pwpb`-suffixed names in new code.')
            legacy_namespace = package.cpp_namespace(codegen_subnamespace=None)
            output.write_line(f'namespace {legacy_namespace} {{')
            output.write_line(f'using namespace ::{package.cpp_namespace()};')
            output.write_line(f'}}  // namespace {legacy_namespace}')

        # TODO: b/250945489 - Remove this if possible
        output.write_line()
        output.write_line(
            '// Codegen implementation detail; do not use this namespace!'
        )

        external_lookup_namespace = "{}::{}".format(
            EXTERNAL_SYMBOL_WORKAROUND_NAMESPACE,
            package.cpp_namespace(codegen_subnamespace=None),
        )

        output.write_line(f'namespace {external_lookup_namespace} {{')
        output.write_line(f'using namespace ::{package.cpp_namespace()};')
        output.write_line(f'}}  // namespace {external_lookup_namespace}')

    if messages:
        proto_namespace = PROTOBUF_NAMESPACE.lstrip(':')
        output.write_line()
        output.write_line(f'namespace {proto_namespace} {{')

        for message in messages:
            generate_is_trivially_comparable_specialization(
                message, package, output
            )

        output.write_line(f'}}  // namespace {proto_namespace}')


def process_proto_file(
    proto_file,
    proto_options,
    suppress_legacy_namespace: bool,
    exclude_legacy_snake_case_field_name_enums: bool,
) -> Iterable[OutputFile]:
    """Generates code for a single .proto file."""

    # Two passes are made through the file. The first builds the tree of all
    # message/enum nodes, then the second creates the fields in each. This is
    # done as non-primitive fields need pointers to their types, which requires
    # the entire tree to have been parsed into memory.
    _, package_root = build_node_tree(proto_file, proto_options=proto_options)

    output_filename = _proto_filename_to_generated_header(proto_file.name)
    output_file = OutputFile(output_filename)
    generate_code_for_package(
        proto_file,
        package_root,
        output_file,
        suppress_legacy_namespace,
        exclude_legacy_snake_case_field_name_enums,
    )

    return [output_file]
