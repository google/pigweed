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
"""Types representing the basic pw_rpc concepts: channel, service, method."""

from dataclasses import dataclass
import enum
from typing import Any, Callable, Collection, Iterable, Iterator, NamedTuple
from typing import TypeVar, Union

from pw_rpc import ids


@dataclass(frozen=True)
class Channel:
    id: int
    output: Callable[[bytes], None]

    def __repr__(self) -> str:
        return f'Channel({self.id})'


@dataclass(frozen=True, eq=False)
class Service:
    """Describes an RPC service."""
    name: str
    id: int
    methods: 'ServiceAccessor'

    @classmethod
    def from_descriptor(cls, module, descriptor):
        service = cls(descriptor.name, ids.calculate(descriptor.name), None)
        object.__setattr__(
            service, 'methods',
            Methods(
                Method.from_descriptor(module, method_descriptor, service)
                for method_descriptor in descriptor.methods))

        return service

    def __repr__(self) -> str:
        return f'Service({self.name!r})'


@dataclass(frozen=True, eq=False)
class Method:
    """Describes a method in a service."""

    service: Service
    name: str
    id: int
    type: 'Method.Type'
    request_type: Any
    response_type: Any

    @property
    def full_name(self) -> str:
        return f'{self.service.name}.{self.name}'

    class Type(enum.Enum):
        UNARY = 0
        SERVER_STREAMING = 1
        CLIENT_STREAMING = 2
        BIDI_STREAMING = 3

        @classmethod
        def from_descriptor(cls, unused_descriptor) -> 'Method.Type':
            # TODO(hepler): Add server_streaming and client_streaming to
            #     protobuf generated code, or access these attributes by
            #     deserializing the FileDescriptor.
            return cls.UNARY

    @classmethod
    def from_descriptor(cls, module, descriptor, service: Service):
        return Method(
            service,
            descriptor.name,
            ids.calculate(descriptor.name),
            cls.Type.from_descriptor(descriptor),
            getattr(module, descriptor.input_type.name),
            getattr(module, descriptor.output_type.name),
        )

    def __repr__(self) -> str:
        return f'Method({self.name!r})'


class PendingRpc(NamedTuple):
    """Uniquely identifies an RPC call."""
    channel: Channel
    service: Service
    method: Method


T = TypeVar('T')


class ServiceAccessor(Collection[T]):
    """Navigates RPC services by name or ID."""
    def __init__(self, members, as_attrs: bool):
        """Creates accessor from a {name: value} dict or [values] iterable."""
        if isinstance(members, dict):
            by_name = members
            self._by_id = {
                ids.calculate(name): m
                for name, m in by_name.items()
            }
        else:
            by_name = {m.name: m for m in members}
            self._by_id = {m.id: m for m in by_name.values()}

        if as_attrs:
            for name, member in by_name.items():
                setattr(self, name, member)

    def __getitem__(self, name_or_id: Union[str, int]):
        """Accesses a service/method by the string name or ID."""
        try:
            return self._by_id[_id(name_or_id)]
        except KeyError:
            name = ' (name_or_id)' if isinstance(name_or_id, str) else ''
            raise KeyError(f'Unknown ID {_id(name_or_id)}{name}')

    def __iter__(self) -> Iterator[T]:
        return iter(self._by_id.values())

    def __len__(self) -> int:
        return len(self._by_id)

    def __contains__(self, name_or_id) -> bool:
        return _id(name_or_id) in self._by_id

    def __repr__(self) -> str:
        members = ', '.join(repr(m) for m in self._by_id.values())
        return f'{self.__class__.__name__}({members})'


def _id(handle: Union[str, int]) -> int:
    return ids.calculate(handle) if isinstance(handle, str) else handle


class Services(ServiceAccessor[Service]):
    """A collection of Service descriptors."""
    def __init__(self, services: Iterable[Service]):
        super().__init__(services, as_attrs=False)


class Methods(ServiceAccessor[Method]):
    """A collection of Method descriptors in a Service."""
    def __init__(self, method: Iterable[Method]):
        super().__init__(method, as_attrs=False)
