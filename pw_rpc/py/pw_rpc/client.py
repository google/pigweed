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
"""Creates an RPC client."""

import abc
from collections import defaultdict
from dataclasses import dataclass
import logging
from queue import SimpleQueue
from typing import Any, Collection, Dict, Iterable

from pw_rpc import descriptors, packets
from pw_rpc.descriptors import Channel, Service, Method, PendingRpc
from pw_status import Status

_LOG = logging.getLogger(__name__)


class ClientImpl(abc.ABC):
    """The internal interface of the RPC client.

    This interface defines the semantics for invoking an RPC on a particular
    client. The return values can objects that provide for synchronous or
    asynchronous behavior.
    """
    @abc.abstractmethod
    def invoke_unary(self, rpc: PendingRpc, request) -> Any:
        """Invokes a unary RPC."""

    @abc.abstractmethod
    def invoke_server_streaming(self, rpc: PendingRpc, request) -> Any:
        """Invokes a server streaming RPC."""

    @abc.abstractmethod
    def invoke_client_streaming(self, rpc: PendingRpc) -> Any:
        """Invokes a client streaming streaming RPC."""

    @abc.abstractmethod
    def invoke_bidirectional_streaming(self, rpc: PendingRpc) -> Any:
        """Invokes a bidirectional streaming streaming RPC."""

    @abc.abstractmethod
    def process_response(self, rpc: PendingRpc, payload,
                         status: Status) -> None:
        """Processes a response from the RPC server."""


class SimpleSynchronousClient(ClientImpl):
    """A client that blocks until a response is received for unary RPCs."""
    def __init__(self):
        self._responses: Dict[PendingRpc,
                              SimpleQueue] = defaultdict(SimpleQueue)
        self._pending: Dict[PendingRpc, bool] = defaultdict(bool)

    def invoke_unary(self, rpc: PendingRpc, request: packets.Message):
        queue = self._responses[rpc]

        assert not self._pending[rpc], f'{rpc} is already pending!'
        self._pending[rpc] = True

        try:
            rpc.channel.output(packets.encode(rpc, request))
            result = queue.get()
        finally:
            self._pending[rpc] = False
        return result

    def invoke_server_streaming(self, rpc: PendingRpc, request):
        raise NotImplementedError

    def invoke_client_streaming(self, rpc: PendingRpc):
        raise NotImplementedError

    def invoke_bidirectional_streaming(self, rpc: PendingRpc):
        raise NotImplementedError

    def process_response(self, rpc: PendingRpc, payload,
                         status: Status) -> None:
        if not self._pending[rpc]:
            _LOG.warning('Discarding packet for %s', rpc)
            return

        self._responses[rpc].put((status, payload))


class _MethodClient:
    """A method that can be invoked for a particular channel."""
    @classmethod
    def create(cls, client_impl: ClientImpl, channel: Channel, method: Method):
        """Instantiates a _MethodClient according to the RPC type."""
        if method.type is Method.Type.UNARY:
            return UnaryMethodClient(client_impl, channel, method)

        raise NotImplementedError('Streaming methods are not yet supported')

    def __init__(self, client_impl: ClientImpl, channel: Channel,
                 method: Method):
        self._client_impl = client_impl
        self.channel = channel
        self.method = method

    def _get_request(self, proto: packets.Message,
                     kwargs: dict) -> packets.Message:
        if proto and kwargs:
            raise TypeError(
                'Requests must be provided either as a message object or a '
                'series of keyword args, but both were provided')

        if proto is None:
            return self.method.request_type(**kwargs)

        if not isinstance(proto, self.method.request_type):
            try:
                bad_type = proto.DESCRIPTOR.full_name
            except AttributeError:
                bad_type = type(proto).__name__

            raise TypeError(
                f'Expected a message of type '
                f'{self.method.request_type.DESCRIPTOR.full_name}, '
                f'got {bad_type}')

        return proto


class UnaryMethodClient(_MethodClient):
    # TODO(hepler): This function should make _request a positional-only
    #     argument, to avoid confusion with keyword-specified protobuf fields.
    #     However, yapf does not yet support Python 3.8's grammar, and
    #     positional-only arguments crash it.
    def __call__(self, _request=None, **request_fields):
        """Invokes this unary method using its associated channel.

        The request can be provided as either a message object or as keyword
        arguments for the message's fields (but not both).
        """
        return self._client_impl.invoke_unary(
            PendingRpc(self.channel, self.method.service, self.method),
            self._get_request(_request, request_fields))


class _MethodClients(descriptors.ServiceAccessor[_MethodClient]):
    """Navigates the methods in a service provided by a ChannelClient."""
    def __init__(self, client_impl: ClientImpl, channel: Channel,
                 methods: Collection[Method]):
        super().__init__(
            {
                method.name: _MethodClient.create(client_impl, channel, method)
                for method in methods
            },
            as_attrs=True)


class _ServiceClients(descriptors.ServiceAccessor[_MethodClients]):
    """Navigates the services provided by a ChannelClient."""
    def __init__(self, client_impl, channel: Channel,
                 services: Collection[Service]):
        super().__init__(
            {
                s.name: _MethodClients(client_impl, channel, s.methods)
                for s in services
            },
            as_attrs=True)


@dataclass(frozen=True, eq=False)
class ChannelClient:
    """RPC services and methods bound to a particular channel.

    RPCs are invoked from a ChannelClient using its call member. The service and
    method may be selected as attributes or by indexing call with service and
    method name or ID:

      response = client.channel(1).call.FooService.SomeMethod(foo=bar)

      response = client.channel(1).call[foo_service_id]['SomeMethod'](foo=bar)

    The type and semantics of the return value, if there is one, are determined
    by the ClientImpl instance used by the Client.
    """
    channel: Channel
    call: _ServiceClients


class Client:
    """Sends requests and handles responses for a set of channels.

    RPC invocations occur through a ChannelClient.
    """
    @classmethod
    def from_modules(cls, impl: ClientImpl, channels: Iterable[Channel],
                     modules: Iterable):
        return cls(
            impl, channels,
            (Service.from_descriptor(module, service) for module in modules
             for service in module.DESCRIPTOR.services_by_name.values()))

    def __init__(self, impl: ClientImpl, channels: Iterable[Channel],
                 services: Iterable[Service]):
        self.services = descriptors.Services(services)
        self._impl = impl
        self._channels_by_id = {
            channel.id:
            ChannelClient(channel,
                          _ServiceClients(self._impl, channel, self.services))
            for channel in channels
        }

    def channel(self, channel_id: int) -> ChannelClient:
        """Returns a ChannelClient, which is used to call RPCs on a channel."""
        return self._channels_by_id[channel_id]

    def process_packet(self, data: bytes) -> bool:
        """Processes an incoming packet.

        Args:
            data: raw binary data for exactly one RPC packet

        Returns:
            True if the packet was decoded and handled by this client
        """
        try:
            packet = packets.decode(data)
        except packets.DecodeError as err:
            _LOG.warning('Failed to decode packet: %s', err)
            _LOG.debug('Raw packet: %r', data)
            return False

        try:
            rpc = self._lookup_packet(packet)
        except ValueError as err:
            _LOG.warning('Unable to process packet: %s', err)
            return False

        try:
            response = packets.decode_payload(packet, rpc.method.response_type)
        except packets.DecodeError as err:
            response = None
            _LOG.warning('Failed to decode %s response for %s: %s',
                         rpc.method.response_type.DESCRIPTOR.full_name,
                         rpc.method.full_name, err)

        self._impl.process_response(rpc, response, Status(packet.status))
        return True

    def _lookup_packet(self, packet: packets.RpcPacket) -> PendingRpc:
        try:
            channel_client = self._channels_by_id[packet.channel_id]
        except KeyError:
            raise ValueError(f'Unrecognized channel ID {packet.channel_id}')

        try:
            service = self.services[packet.service_id]
        except KeyError:
            raise ValueError(f'Unrecognized service ID {packet.service_id}')

        try:
            method = service.methods[packet.method_id]
        except KeyError:
            raise ValueError(
                f'No method ID {packet.method_id} in service {service.name}')

        return PendingRpc(channel_client.channel, service, method)
