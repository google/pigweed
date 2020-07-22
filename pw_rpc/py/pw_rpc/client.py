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
from dataclasses import dataclass
import logging
from typing import Collection, Dict, Iterable, Iterator, List, NamedTuple
from typing import Optional

from pw_rpc import descriptors, packets
from pw_rpc.packets import PacketType
from pw_rpc.descriptors import Channel, Service, Method
from pw_status import Status

_LOG = logging.getLogger(__name__)


class Error(Exception):
    """Error from incorrectly using the RPC client classes."""


class PendingRpc(NamedTuple):
    """Uniquely identifies an RPC call."""
    channel: Channel
    service: Service
    method: Method

    def __str__(self) -> str:
        return f'PendingRpc(channel={self.channel.id}, method={self.method})'


class PendingRpcs:
    """Internal object for tracking whether an RPC is pending."""
    def __init__(self):
        self._pending: Dict[PendingRpc, List] = {}

    def invoke(self,
               rpc: PendingRpc,
               request,
               context,
               override_pending=False) -> None:
        """Invokes the provided RPC."""
        # Ensure that every context is a unique object by wrapping it in a list.
        unique_ctx = [context]

        if override_pending:
            self._pending[rpc] = unique_ctx
        elif self._pending.setdefault(rpc, unique_ctx) is not unique_ctx:
            # If the context was not added, the RPC was already pending.
            raise Error(f'Sent request for {rpc}, but it is already pending! '
                        'Cancel the RPC before invoking it again')

        _LOG.debug('Starting %s', rpc)
        rpc.channel.output(packets.encode_request(rpc, request))

    def cancel(self, rpc: PendingRpc) -> bool:
        """Cancels the RPC, including sending a CANCEL packet.

        Returns:
          True if the RPC was cancelled; False if it was not pending
        """
        try:
            _LOG.debug('Cancelling %s', rpc)
            del self._pending[rpc]
        except KeyError:
            return False

        if rpc.method.type is not Method.Type.UNARY:
            rpc.channel.output(packets.encode_cancel(rpc))

        return True

    def clear(self, rpc: PendingRpc) -> bool:
        """Clears the RPC's pending status without sending a CANCEL packet."""
        try:
            _LOG.debug('Clearing %s', rpc)
            del self._pending[rpc]
        except KeyError:
            return False

        return True

    def get_pending(self, rpc: PendingRpc, status: Optional[Status]):
        if status is None:
            return self._pending[rpc][0]  # Unwrap the context from the list

        _LOG.debug('Finishing %s with status %s', rpc, status)
        return self._pending.pop(rpc)[0]


class ClientImpl(abc.ABC):
    """The internal interface of the RPC client.

    This interface defines the semantics for invoking an RPC on a particular
    client.
    """
    @abc.abstractmethod
    def method_client(self, rpcs: PendingRpcs, channel: Channel,
                      method: Method):
        """Returns an object that invokes a method using the given channel."""

    @abc.abstractmethod
    def process_response(self,
                         rpcs: PendingRpcs,
                         rpc: PendingRpc,
                         context,
                         status: Optional[Status],
                         payload,
                         *,
                         args: tuple = (),
                         kwargs: dict = None) -> None:
        """Processes a response from the RPC server.

        Args:
          rpcs: The PendingRpcs object used by the client.
          rpc: Information about the pending RPC
          context: Arbitrary context object associated with the pending RPC
          status: If set, this is the last packet for this RPC. None otherwise.
          payload: A protobuf message, if present. None otherwise.
        """


class _MethodClients(descriptors.ServiceAccessor):
    """Navigates the methods in a service provided by a ChannelClient."""
    def __init__(self, rpcs: PendingRpcs, client_impl: ClientImpl,
                 channel: Channel, methods: Collection[Method]):
        super().__init__(
            {
                method: client_impl.method_client(rpcs, channel, method)
                for method in methods
            },
            as_attrs='members')


class _ServiceClients(descriptors.ServiceAccessor[_MethodClients]):
    """Navigates the services provided by a ChannelClient."""
    def __init__(self, rpcs: PendingRpcs, client_impl, channel: Channel,
                 services: Collection[Service]):
        super().__init__(
            {
                s: _MethodClients(rpcs, client_impl, channel, s.methods)
                for s in services
            },
            as_attrs='packages')


def _decode_status(rpc: PendingRpc, packet) -> Optional[Status]:
    # Server streaming RPC packets never have a status; all other packets do.
    if packet.type == PacketType.RPC and rpc.method.server_streaming:
        return None

    try:
        return Status(packet.status)
    except ValueError:
        _LOG.warning('Illegal status code %d for %s', packet.status, rpc)

    return None


def _decode_payload(rpc: PendingRpc, packet):
    if packet.type == PacketType.RPC:
        try:
            return packets.decode_payload(packet, rpc.method.response_type)
        except packets.DecodeError as err:
            _LOG.warning('Failed to decode %s response for %s: %s',
                         rpc.method.response_type.DESCRIPTOR.full_name,
                         rpc.method.full_name, err)
    return None


@dataclass(frozen=True, eq=False)
class ChannelClient:
    """RPC services and methods bound to a particular channel.

    RPCs are invoked through service method clients. These may be accessed via
    the `rpcs` member. Service methods use a fully qualified name: package,
    service, method. Service methods may be selected as attributes or by
    indexing the rpcs member by service and method name or ID.

      # Access the service method client as an attribute
      rpc = client.channel(1).rpcs.the.package.FooService.SomeMethod

      # Access the service method client by string name
      rpc = client.channel(1).rpcs[foo_service_id]['SomeMethod']

    RPCs may also be accessed from their canonical name.

      # Access the service method client from its full name:
      rpc = client.channel(1).method('the.package.FooService/SomeMethod')

      # Using a . instead of a / is also supported:
      rpc = client.channel(1).method('the.package.FooService.SomeMethod')

    The ClientImpl class determines the type of the service method client. A
    synchronous RPC client might return a callable object, so an RPC could be
    invoked directly (e.g. rpc(field1=123, field2=b'456')).
    """
    client: 'Client'
    channel: Channel
    rpcs: _ServiceClients

    def method(self, method_name: str):
        """Returns a method client matching the given name.

        Args:
          method_name: name as package.Service/Method or package.Service.Method.

        Raises:
          ValueError: the method name is not properly formatted
          KeyError: the method is not present
        """
        return descriptors.get_method(self.rpcs, method_name)

    def methods(self) -> Iterator:
        """Iterates over all method clients in this ChannelClient."""
        for service_client in self.rpcs:
            yield from service_client


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
        self._impl = impl

        self.services = descriptors.Services(services)

        self._rpcs = PendingRpcs()

        self._channels_by_id = {
            channel.id: ChannelClient(
                self, channel,
                _ServiceClients(self._rpcs, self._impl, channel,
                                self.services))
            for channel in channels
        }

    def channel(self, channel_id: int) -> ChannelClient:
        """Returns a ChannelClient, which is used to call RPCs on a channel."""
        return self._channels_by_id[channel_id]

    def method(self, method_name: str) -> Method:
        """Returns a Method matching the given name.

        Args:
          method_name: name as package.Service/Method or package.Service.Method.

        Raises:
          ValueError: the method name is not properly formatted
          KeyError: the method is not present
        """
        return descriptors.get_method(self.services, method_name)

    def methods(self) -> Iterator[Method]:
        """Iterates over all Methods supported by this client."""
        for service in self.services:
            yield from service.methods

    def process_packet(self, pw_rpc_raw_packet_data: bytes, *impl_args,
                       **impl_kwargs) -> bool:
        """Processes an incoming packet.

        Args:
            pw_rpc_raw_packet_data: raw binary data for exactly one RPC packet
            impl_args: optional positional arguments passed to the ClientImpl
            impl_kwargs: optional keyword arguments passed to the ClientImpl

        Returns:
            True if the packet was decoded and handled by this client
        """
        try:
            packet = packets.decode(pw_rpc_raw_packet_data)
        except packets.DecodeError as err:
            _LOG.warning('Failed to decode packet: %s', err)
            _LOG.debug('Raw packet: %r', pw_rpc_raw_packet_data)
            return False

        try:
            rpc = self._lookup_rpc(packet)
        except ValueError as err:
            _LOG.warning('Unable to process packet: %s', err)
            _LOG.debug('Packet:\n%s', packet)
            return False

        status = _decode_status(rpc, packet)

        if packet.type == PacketType.ERROR:
            self._rpcs.clear(rpc)
            _LOG.warning('%s: invocation failed with %s', rpc, status)
            return True  # Handled packet, even though it was an error

        if packet.type not in (PacketType.RPC, PacketType.STREAM_END):
            _LOG.error('%s: unexpected PacketType %s', rpc, packet.type)
            _LOG.debug('Packet:\n%s', packet)
            return True

        payload = _decode_payload(rpc, packet)

        try:
            context = self._rpcs.get_pending(rpc, status)
        except KeyError:
            _LOG.debug('Discarding response for %s, which is not pending', rpc)
            return True

        self._impl.process_response(self._rpcs,
                                    rpc,
                                    context,
                                    status,
                                    payload,
                                    args=impl_args,
                                    kwargs=impl_kwargs)
        return True

    def _lookup_rpc(self, packet: packets.RpcPacket) -> PendingRpc:
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
