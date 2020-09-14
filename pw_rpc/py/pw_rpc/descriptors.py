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
from typing import Any, Callable, Collection, Dict, Generic, Iterable, Iterator
from typing import Tuple, TypeVar, Union

from google.protobuf import descriptor_pb2
from pw_rpc import ids
from pw_protobuf_compiler import python_protos


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
    package: str
    methods: 'Methods'

    @property
    def full_name(self):
        return f'{self.package}.{self.name}'

    @classmethod
    def from_descriptor(cls, module, descriptor):
        service = cls(descriptor.name, ids.calculate(descriptor.full_name),
                      descriptor.file.package, None)
        object.__setattr__(
            service, 'methods',
            Methods(
                Method.from_descriptor(module, method_descriptor, service)
                for method_descriptor in descriptor.methods))

        return service

    def __repr__(self) -> str:
        return f'Service({self.full_name!r})'

    def __str__(self) -> str:
        return self.full_name


def _streaming_attributes(method) -> Tuple[bool, bool]:
    # TODO(hepler): Investigate adding server_streaming and client_streaming
    #     attributes to the generated protobuf code. As a workaround,
    #     deserialize the FileDescriptorProto to get that information.
    service = method.containing_service

    file_pb = descriptor_pb2.FileDescriptorProto()
    file_pb.MergeFromString(service.file.serialized_pb)

    method_pb = file_pb.service[service.index].method[method.index]  # pylint: disable=no-member
    return method_pb.server_streaming, method_pb.client_streaming


@dataclass(frozen=True, eq=False)
class Method:
    """Describes a method in a service."""

    service: Service
    name: str
    id: int
    server_streaming: bool
    client_streaming: bool
    request_type: Any
    response_type: Any

    @classmethod
    def from_descriptor(cls, module, descriptor, service: Service):
        return Method(
            service,
            descriptor.name,
            ids.calculate(descriptor.name),
            *_streaming_attributes(descriptor),
            getattr(module, descriptor.input_type.name),
            getattr(module, descriptor.output_type.name),
        )

    class Type(enum.Enum):
        UNARY = 0
        SERVER_STREAMING = 1
        CLIENT_STREAMING = 2
        BIDI_STREAMING = 3

    @property
    def full_name(self) -> str:
        return f'{self.service.full_name}.{self.name}'

    @property
    def type(self) -> 'Method.Type':
        if self.server_streaming and self.client_streaming:
            return self.Type.BIDI_STREAMING

        if self.server_streaming:
            return self.Type.SERVER_STREAMING

        if self.client_streaming:
            return self.Type.CLIENT_STREAMING

        return self.Type.UNARY

    def get_request(self, proto, proto_kwargs: Dict[str, Any]):
        """Returns a request_type protobuf message.

        The client implementation may use this to support providing a request
        as either a message object or as keyword arguments for the message's
        fields (but not both).
        """
        if proto and proto_kwargs:
            raise TypeError(
                'Requests must be provided either as a message object or a '
                'series of keyword args, but both were provided '
                f'({proto!r} and {proto_kwargs!r})')

        if proto is None:
            return self.request_type(**proto_kwargs)

        if not isinstance(proto, self.request_type):
            try:
                bad_type = proto.DESCRIPTOR.full_name
            except AttributeError:
                bad_type = type(proto).__name__

            raise TypeError(f'Expected a message of type '
                            f'{self.request_type.DESCRIPTOR.full_name}, '
                            f'got {bad_type}')

        return proto

    def __repr__(self) -> str:
        req = self._method_parameter(self.request_type, self.client_streaming)
        res = self._method_parameter(self.response_type, self.server_streaming)
        return f'<{self.full_name}({req}) returns ({res})>'

    def _method_parameter(self, proto, streaming: bool) -> str:
        """Returns a description of the method's request or response type."""
        stream = 'stream ' if streaming else ''

        if proto.DESCRIPTOR.file.package == self.service.package:
            return stream + proto.DESCRIPTOR.name

        return stream + proto.DESCRIPTOR.full_name

    def __str__(self) -> str:
        return self.full_name


T = TypeVar('T')


def _name(item: Union[Service, Method]) -> str:
    return item.full_name if isinstance(item, Service) else item.name


class _AccessByName(Generic[T]):
    """Wrapper for accessing types by name within a proto package structure."""
    def __init__(self, name: str, item: T):
        setattr(self, name, item)


class ServiceAccessor(Collection[T]):
    """Navigates RPC services by name or ID."""
    def __init__(self, members, as_attrs: str = ''):
        """Creates accessor from an {item: value} dict or [values] iterable."""
        # If the members arg was passed as a [values] iterable, convert it to
        # an equivalent dictionary.
        if not isinstance(members, dict):
            members = {m: m for m in members}

        by_name = {_name(k): v for k, v in members.items()}
        self._by_id = {k.id: v for k, v in members.items()}

        if as_attrs == 'members':
            for name, member in by_name.items():
                setattr(self, name, member)
        elif as_attrs == 'packages':
            for package in python_protos.as_packages(
                (m.package, _AccessByName(m.name, members[m]))
                    for m in members).packages:
                setattr(self, str(package), package)
        elif as_attrs:
            raise ValueError(f'Unexpected value {as_attrs!r} for as_attrs')

    def __getitem__(self, name_or_id: Union[str, int]):
        """Accesses a service/method by the string name or ID."""
        try:
            return self._by_id[_id(name_or_id)]
        except KeyError:
            pass

        name = f' ("{name_or_id}")' if isinstance(name_or_id, str) else ''
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


class Methods(ServiceAccessor[Method]):
    """A collection of Method descriptors in a Service."""
    def __init__(self, method: Iterable[Method]):
        super().__init__(method)


class Services(ServiceAccessor[Service]):
    """A collection of Service descriptors."""
    def __init__(self, services: Iterable[Service]):
        super().__init__(services)


def get_method(service_accessor: ServiceAccessor, name: str):
    """Returns a method matching the given full name in a ServiceAccessor.

    Args:
      name: name as package.Service/Method or package.Service.Method.

    Raises:
      ValueError: the method name is not properly formatted
      KeyError: the method is not present
    """
    if '/' in name:
        service_name, method_name = name.split('/')
    else:
        service_name, method_name = name.rsplit('.', 1)

    service = service_accessor[service_name]
    if isinstance(service, Service):
        service = service.methods

    return service[method_name]
