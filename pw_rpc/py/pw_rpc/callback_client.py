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
"""Defines a callback-based RPC ClientImpl to use with pw_rpc.client.Client.

callback_client.Impl supports invoking RPCs synchronously or asynchronously.
Asynchronous invocations use a callback.

Synchronous invocations look like a function call:

  status, response = client.channel(1).call.MyServer.MyUnary(some_field=123)

  # Streaming calls return an iterable of responses
  for reply in client.channel(1).call.MyService.MyServerStreaming(request):
      pass

Asynchronous invocations pass a callback in addition to the request. The
callback must be a callable that accepts a status and a payload, either of
which may be None. The Status is only set when the RPC is completed.

  callback = lambda status, payload: print('Response:', status, payload)

  call = client.channel(1).call.MyServer.MyUnary.with_callback(
      callback, some_field=123)

  call = client.channel(1).call.MyService.MyServerStreaming.with_callback(
      callback, request):

When invoking a method, requests may be provided as a message object or as
kwargs for the message fields (but not both).
"""

import logging
import queue
from typing import Any, Callable, Optional, Tuple

from pw_rpc import client
from pw_rpc.descriptors import Channel, Method, Service
from pw_status import Status

_LOG = logging.getLogger(__name__)

UnaryCallback = Callable[[Status, Any], Any]
Callback = Callable[[Optional[Status], Any], Any]


class _MethodClient:
    """A method that can be invoked for a particular channel."""
    def __init__(self, client_impl: 'Impl', rpcs: client.PendingRpcs,
                 channel: Channel, method: Method):
        self._impl = client_impl
        self._rpcs = rpcs
        self._rpc = client.PendingRpc(channel, method.service, method)

    @property
    def channel(self) -> Channel:
        return self._rpc.channel

    @property
    def method(self) -> Method:
        return self._rpc.method

    @property
    def service(self) -> Service:
        return self._rpc.service


class _AsyncCall:
    """Represents an ongoing callback-based call."""

    # TODO(hepler): Consider alternatives (futures) and/or expand functionality.

    def __init__(self, rpcs: client.PendingRpcs, rpc: client.PendingRpc):
        self.rpc = rpc
        self._rpcs = rpcs

    def cancel(self) -> bool:
        return self._rpcs.cancel(self.rpc)

    def __enter__(self) -> '_AsyncCall':
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.cancel()


class _StreamingResponses:
    """Used to iterate over a queue.SimpleQueue."""
    def __init__(self, responses: queue.SimpleQueue):
        self._queue = responses
        self.status: Optional[Status] = None

    def get(self, block: bool = True, timeout_s: float = None):
        while True:
            self.status, response = self._queue.get(block, timeout_s)
            if self.status is not None:
                return

            yield response

    def __iter__(self):
        return self.get()


class UnaryMethodClient(_MethodClient):
    def __call__(self, _request=None, **request_fields) -> Tuple[Status, Any]:
        responses: queue.SimpleQueue = queue.SimpleQueue()
        self.with_callback(
            lambda status, payload: responses.put((status, payload)), _request,
            **request_fields)
        return responses.get()

    def with_callback(self,
                      callback: UnaryCallback,
                      _request=None,
                      **request_fields):
        self._rpcs.invoke(self._rpc, callback, _request, **request_fields)
        return _AsyncCall(self._rpcs, self._rpc)


class ServerStreamingMethodClient(_MethodClient):
    def __call__(self, _request=None, **request_fields) -> _StreamingResponses:
        responses: queue.SimpleQueue = queue.SimpleQueue()
        self.with_callback(
            lambda status, payload: responses.put((status, payload)), _request,
            **request_fields)
        return _StreamingResponses(responses)

    def with_callback(self,
                      callback: Callback,
                      _request=None,
                      **request_fields):
        self._rpcs.invoke(self._rpc, callback, _request, **request_fields)
        return _AsyncCall(self._rpcs, self._rpc)


class ClientStreamingMethodClient(_MethodClient):
    def __call__(self):
        raise NotImplementedError

    def with_callback(self, callback: Callback):
        raise NotImplementedError


class BidirectionalStreamingMethodClient(_MethodClient):
    def __call__(self):
        raise NotImplementedError

    def with_callback(self, callback: Callback):
        raise NotImplementedError


class Impl(client.ClientImpl):
    """Callback-based client.ClientImpl."""
    def method_client(self, rpcs: client.PendingRpcs, channel: Channel,
                      method: Method) -> _MethodClient:
        """Returns an object that invokes a method using the given chanel."""

        if method.type is Method.Type.UNARY:
            return UnaryMethodClient(self, rpcs, channel, method)

        if method.type is Method.Type.SERVER_STREAMING:
            return ServerStreamingMethodClient(self, rpcs, channel, method)

        if method.type is Method.Type.CLIENT_STREAMING:
            return ClientStreamingMethodClient(self, rpcs, channel, method)

        if method.type is Method.Type.BIDI_STREAMING:
            return BidirectionalStreamingMethodClient(self, rpcs, channel,
                                                      method)

        raise AssertionError(f'Unknown method type {method.type}')

    def process_response(self, rpcs: client.PendingRpcs,
                         rpc: client.PendingRpc, context,
                         status: Optional[Status], payload) -> None:
        try:
            context(status, payload)
        except:  # pylint: disable=bare-except
            rpcs.cancel(rpc)
            _LOG.exception('Callback %s for %s raised exception', context, rpc)
