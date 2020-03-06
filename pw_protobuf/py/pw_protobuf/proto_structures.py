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
"""This module defines data structures for protobuf entities."""

import abc
import collections
import enum

from typing import Callable, Dict, Iterator, List, Optional, Tuple, TypeVar

T = TypeVar('T')  # pylint: disable=invalid-name


class ProtoNode(abc.ABC):
    """A ProtoNode represents a C++ scope mapping of an entity in a .proto file.

    Nodes form a tree beginning at a top-level (global) scope, descending into a
    hierarchy of .proto packages and the messages and enums defined within them.
    """
    class Type(enum.Enum):
        """The type of a ProtoNode.

        PACKAGE maps to a C++ namespace.
        MESSAGE maps to a C++ "Encoder" class within its own namespace.
        ENUM maps to a C++ enum within its parent's namespace.
        """
        PACKAGE = 1
        MESSAGE = 2
        ENUM = 3

    def __init__(self, name: str):
        self._name: str = name
        self._children: Dict[str, 'ProtoNode'] = collections.OrderedDict()
        self._parent: Optional['ProtoNode'] = None

    @abc.abstractmethod
    def type(self) -> 'ProtoNode.Type':
        """The type of the node."""

    def children(self) -> List['ProtoNode']:
        return list(self._children.values())

    def name(self) -> str:
        return self._name

    def cpp_name(self) -> str:
        """The name of this node in generated C++ code."""
        return self._name.replace('.', '::')

    def cpp_namespace(self, root: Optional['ProtoNode'] = None) -> str:
        """C++ namespace of the node, up to the specified root."""
        return '::'.join(
            self._attr_hierarchy(lambda node: node.cpp_name(), root))

    def common_ancestor(self, other: 'ProtoNode') -> Optional['ProtoNode']:
        """Finds the earliest common ancestor of this node and other."""

        if other is None:
            return None

        own_depth = self.depth()
        other_depth = other.depth()
        diff = abs(own_depth - other_depth)

        if own_depth < other_depth:
            first: Optional['ProtoNode'] = self
            second: Optional['ProtoNode'] = other
        else:
            first = other
            second = self

        while diff > 0:
            assert second is not None
            second = second.parent()
            diff -= 1

        while first != second:
            if first is None or second is None:
                return None

            first = first.parent()
            second = second.parent()

        return first

    def depth(self) -> int:
        """Returns the depth of this node from the root."""
        depth = 0
        node = self._parent
        while node:
            depth += 1
            node = node.parent()
        return depth

    def add_child(self, child: 'ProtoNode') -> None:
        """Inserts a new node into the tree as a child of this node.

        Args:
          child: The node to insert.

        Raises:
          ValueError: This node does not allow nesting the given type of child.
        """
        if not self._supports_child(child):
            raise ValueError('Invalid child %s for node of type %s' %
                             (child.type(), self.type()))

        # pylint: disable=protected-access
        if child._parent is not None:
            del child._parent._children[child.name()]

        child._parent = self
        self._children[child.name()] = child
        # pylint: enable=protected-access

    def find(self, path: str) -> Optional['ProtoNode']:
        """Finds a node within this node's subtree."""
        node = self

        # pylint: disable=protected-access
        for section in path.split('.'):
            node = node._children[section]
            if node is None:
                return None
        # pylint: enable=protected-access

        return node

    def parent(self) -> Optional['ProtoNode']:
        return self._parent

    def __iter__(self) -> Iterator['ProtoNode']:
        """Iterates depth-first through all nodes in this node's subtree."""
        yield self
        for child_iterator in self._children.values():
            for child in child_iterator:
                yield child

    def _attr_hierarchy(
            self,
            attr_accessor: Callable[['ProtoNode'], T],
            root: Optional['ProtoNode'],
    ) -> Iterator[T]:
        """Fetches node attributes at each level of the tree from the root.

        Args:
          attr_accessor: Function which extracts attributes from a ProtoNode.
          root: The node at which to terminate.

        Returns:
          An iterator to a list of the selected attributes from the root to the
          current node.
        """
        hierarchy = []
        node: Optional['ProtoNode'] = self
        while node is not None and node != root:
            hierarchy.append(attr_accessor(node))
            node = node.parent()
        return reversed(hierarchy)

    @abc.abstractmethod
    def _supports_child(self, child: 'ProtoNode') -> bool:
        """Returns True if child is a valid child type for the current node."""


class ProtoPackage(ProtoNode):
    """A protobuf package."""
    def type(self) -> ProtoNode.Type:
        return ProtoNode.Type.PACKAGE

    def _supports_child(self, child: ProtoNode) -> bool:
        return True


class ProtoEnum(ProtoNode):
    """Representation of an enum in a .proto file."""

    # Prefix for names of values in C++ enums.
    ENUM_PREFIX: str = 'k'

    def __init__(self, name: str):
        super().__init__(name)
        self._values: List[Tuple[str, int]] = []

    def type(self) -> ProtoNode.Type:
        return ProtoNode.Type.ENUM

    def values(self) -> List[Tuple[str, int]]:
        return list(self._values)

    def add_value(self, name: str, value: int) -> None:
        name = '{}{}'.format(ProtoEnum.ENUM_PREFIX,
                             ProtoMessageField.canonicalize_name(name))
        self._values.append((name, value))

    def _supports_child(self, child: ProtoNode) -> bool:
        # Enums cannot have nested children.
        return False


class ProtoMessage(ProtoNode):
    """Representation of a message in a .proto file."""
    def __init__(self, name: str):
        super().__init__(name)
        self._fields: List['ProtoMessageField'] = []

    def type(self) -> ProtoNode.Type:
        return ProtoNode.Type.MESSAGE

    def fields(self) -> List['ProtoMessageField']:
        return list(self._fields)

    def add_field(self, field: 'ProtoMessageField') -> None:
        self._fields.append(field)

    def _supports_child(self, child: ProtoNode) -> bool:
        return (child.type() == self.Type.ENUM
                or child.type() == self.Type.MESSAGE)


# This class is not a node and does not appear in the proto tree.
# Fields belong to proto messages and are processed separately.
class ProtoMessageField:
    """Representation of a field within a protobuf message."""
    def __init__(self,
                 field_name: str,
                 field_number: int,
                 field_type: int,
                 type_node: Optional[ProtoNode] = None,
                 repeated: bool = False):
        self._name: str = self.canonicalize_name(field_name)
        self._number: int = field_number
        self._type: int = field_type
        self._type_node: Optional[ProtoNode] = type_node
        self._repeated: bool = repeated

    def name(self) -> str:
        return self._name

    def enum_name(self) -> str:
        return '{}{}'.format(ProtoEnum.ENUM_PREFIX, self._name)

    def number(self) -> int:
        return self._number

    def type(self) -> int:
        return self._type

    def type_node(self) -> Optional[ProtoNode]:
        return self._type_node

    def is_repeated(self) -> bool:
        return self._repeated

    @staticmethod
    def canonicalize_name(field_name: str) -> str:
        """Converts a field name to UpperCamelCase."""
        name_components = field_name.split('_')
        for i, _ in enumerate(name_components):
            name_components[i] = name_components[i].lower().capitalize()
        return ''.join(name_components)
