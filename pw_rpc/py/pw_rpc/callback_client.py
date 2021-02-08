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

  call = client.channel(1).call.MyServer.MyUnary.invoke(
      callback, some_field=123)

  call = client.channel(1).call.MyService.MyServerStreaming.invoke(
      callback, request):

When invoking a method, requests may be provided as a message object or as
kwargs for the message fields (but not both).
"""

import enum
import inspect
import logging
import queue
import textwrap
from typing import Any, Callable, NamedTuple, Union, Optional

from pw_status import Status

from pw_protobuf_compiler.python_protos import proto_repr
from pw_rpc import client, descriptors
from pw_rpc.descriptors import Channel, Method, Service

_LOG = logging.getLogger(__name__)


class UseDefault(enum.Enum):
    """Marker for args that should use a default value, when None is valid."""
    VALUE = 0


OptionalTimeout = Union[UseDefault, float, None]

Callback = Callable[[client.PendingRpc, Optional[Status], Any], Any]


class _MethodClient:
    """A method that can be invoked for a particular channel."""
    def __init__(self, client_impl: 'Impl', rpcs: client.PendingRpcs,
                 channel: Channel, method: Method,
                 default_timeout_s: Optional[float]):
        self._impl = client_impl
        self._rpcs = rpcs
        self._rpc = client.PendingRpc(channel, method.service, method)
        self.default_timeout_s: Optional[float] = default_timeout_s

    @property
    def channel(self) -> Channel:
        return self._rpc.channel

    @property
    def method(self) -> Method:
        return self._rpc.method

    @property
    def service(self) -> Service:
        return self._rpc.service

    def invoke(self, callback: Callback, _request=None, **request_fields):
        """Invokes an RPC with a callback."""
        self._rpcs.send_request(self._rpc,
                                self.method.get_request(
                                    _request, request_fields),
                                callback,
                                override_pending=False)
        return _AsyncCall(self._rpcs, self._rpc)

    def reinvoke(self, callback: Callback, _request=None, **request_fields):
        """Invokes an RPC with a callback, overriding any pending requests."""
        self._rpcs.send_request(self._rpc,
                                self.method.get_request(
                                    _request, request_fields),
                                callback,
                                override_pending=True)
        return _AsyncCall(self._rpcs, self._rpc)

    def __repr__(self) -> str:
        return self.help()

    def __call__(self):
        raise NotImplementedError('Implemented by derived classes')

    def help(self) -> str:
        """Returns a help message about this RPC."""
        function_call = self.method.full_name + '('

        docstring = inspect.getdoc(self.__call__)
        assert docstring is not None

        annotation = inspect.Signature.from_callable(self).return_annotation
        if isinstance(annotation, type):
            annotation = annotation.__name__

        arg_sep = f',\n{" " * len(function_call)}'
        return (
            f'{function_call}'
            f'{arg_sep.join(descriptors.field_help(self.method.request_type))})'
            f'\n\n{textwrap.indent(docstring, "  ")}\n\n'
            f'  Returns {annotation}.')


class RpcTimeout(Exception):
    def __init__(self, rpc: client.PendingRpc, timeout: Optional[float]):
        super().__init__(
            f'No response received for {rpc.method} after {timeout} s')
        self.rpc = rpc
        self.timeout = timeout


class _AsyncCall:
    """Represents an ongoing callback-based call."""

    # TODO(hepler): Consider alternatives (futures) and/or expand functionality.

    def __init__(self, rpcs: client.PendingRpcs, rpc: client.PendingRpc):
        self.rpc = rpc
        self._rpcs = rpcs

    def cancel(self) -> bool:
        return self._rpcs.send_cancel(self.rpc)

    def __enter__(self) -> '_AsyncCall':
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.cancel()


class StreamingResponses:
    """Used to iterate over a queue.SimpleQueue."""
    def __init__(self, method_client: _MethodClient,
                 responses: queue.SimpleQueue,
                 default_timeout_s: OptionalTimeout):
        self._method_client = method_client
        self._queue = responses
        self.status: Optional[Status] = None

        if default_timeout_s is UseDefault.VALUE:
            self.default_timeout_s = self._method_client.default_timeout_s
        else:
            self.default_timeout_s = default_timeout_s

    @property
    def method(self) -> Method:
        return self._method_client.method

    def responses(self,
                  *,
                  block: bool = True,
                  timeout_s: OptionalTimeout = UseDefault.VALUE):
        """Returns an iterator of stream responses.

        Args:
          timeout_s: timeout in seconds; None blocks indefinitely
        """
        if timeout_s is UseDefault.VALUE:
            timeout_s = self.default_timeout_s

        try:
            while True:
                self.status, response = self._queue.get(block, timeout_s)
                if self.status is not None:
                    return

                yield response
        except queue.Empty:
            pass

        raise RpcTimeout(self._method_client._rpc, timeout_s)  # pylint: disable=protected-access

    def __iter__(self):
        return self.responses()

    def __repr__(self) -> str:
        return f'{type(self).__name__}({self.method})'


def _method_client_docstring(method: Method) -> str:
    return f'''\
Class that invokes the {method.full_name} {method.type.sentence_name()} RPC.

Calling this directly invokes the RPC synchronously. The RPC can be invoked
asynchronously using the invoke or reinvoke methods.
'''


def _function_docstring(method: Method) -> str:
    return f'''\
Invokes the {method.full_name} {method.type.sentence_name()} RPC.

This function accepts either the request protobuf fields as keyword arguments or
a request protobuf as a positional argument.
'''


def _update_function_signature(method: Method, function: Callable) -> None:
    """Updates the name, docstring, and parameters to match a method."""
    function.__name__ = method.full_name
    function.__doc__ = _function_docstring(method)

    # In order to have good tab completion and help messages, update the
    # function signature to accept only keyword arguments for the proto message
    # fields. This doesn't actually change the function signature -- it just
    # updates how it appears when inspected.
    sig = inspect.signature(function)

    params = [next(iter(sig.parameters.values()))]  # Get the "self" parameter
    params += method.request_parameters()
    function.__signature__ = sig.replace(  # type: ignore[attr-defined]
        parameters=params)


class UnaryResponse(NamedTuple):
    """Result of invoking a unary RPC: status and response."""
    status: Status
    response: Any

    def __repr__(self) -> str:
        return f'({self.status}, {proto_repr(self.response)})'


def unary_method_client(client_impl: 'Impl', rpcs: client.PendingRpcs,
                        channel: Channel, method: Method,
                        default_timeout: Optional[float]) -> _MethodClient:
    """Creates an object used to call a unary method."""
    def call(self: _MethodClient,
             _rpc_request_proto=None,
             *,
             pw_rpc_timeout_s=UseDefault.VALUE,
             **request_fields) -> UnaryResponse:
        responses: queue.SimpleQueue = queue.SimpleQueue()

        def enqueue_response(_, status: Optional[Status], payload: Any):
            assert isinstance(status, Status)
            responses.put(UnaryResponse(status, payload))

        self.reinvoke(enqueue_response, _rpc_request_proto, **request_fields)

        if pw_rpc_timeout_s is UseDefault.VALUE:
            pw_rpc_timeout_s = self.default_timeout_s

        try:
            return responses.get(timeout=pw_rpc_timeout_s)
        except queue.Empty:
            pass

        raise RpcTimeout(self._rpc, pw_rpc_timeout_s)  # pylint: disable=protected-access

    _update_function_signature(method, call)

    # The MethodClient class is created dynamically so that the __call__ method
    # can be configured differently for each method.
    method_client_type = type(
        f'{method.name}_UnaryMethodClient', (_MethodClient, ),
        dict(__call__=call, __doc__=_method_client_docstring(method)))
    return method_client_type(client_impl, rpcs, channel, method,
                              default_timeout)


def server_streaming_method_client(client_impl: 'Impl',
                                   rpcs: client.PendingRpcs, channel: Channel,
                                   method: Method,
                                   default_timeout: Optional[float]):
    """Creates an object used to call a server streaming method."""
    def call(self: _MethodClient,
             _rpc_request_proto=None,
             *,
             pw_rpc_timeout_s=UseDefault.VALUE,
             **request_fields) -> StreamingResponses:
        responses: queue.SimpleQueue = queue.SimpleQueue()
        self.reinvoke(
            lambda _, status, payload: responses.put((status, payload)),
            _rpc_request_proto, **request_fields)
        return StreamingResponses(self, responses, pw_rpc_timeout_s)

    _update_function_signature(method, call)

    # The MethodClient class is created dynamically so that the __call__ method
    # can be configured differently for each method type.
    method_client_type = type(
        f'{method.name}_ServerStreamingMethodClient', (_MethodClient, ),
        dict(__call__=call, __doc__=_method_client_docstring(method)))
    return method_client_type(client_impl, rpcs, channel, method,
                              default_timeout)


class ClientStreamingMethodClient(_MethodClient):
    def __call__(self):
        raise NotImplementedError

    def invoke(self, callback: Callback, _request=None, **request_fields):
        raise NotImplementedError


class BidirectionalStreamingMethodClient(_MethodClient):
    def __call__(self):
        raise NotImplementedError

    def invoke(self, callback: Callback, _request=None, **request_fields):
        raise NotImplementedError


class Impl(client.ClientImpl):
    """Callback-based client.ClientImpl."""
    def __init__(self,
                 default_unary_timeout_s: Optional[float] = 1.0,
                 default_stream_timeout_s: Optional[float] = 1.0):
        self._default_unary_timeout_s = default_unary_timeout_s
        self._default_stream_timeout_s = default_stream_timeout_s

    @property
    def default_unary_timeout_s(self) -> Optional[float]:
        return self._default_unary_timeout_s

    @property
    def default_stream_timeout_s(self) -> Optional[float]:
        return self._default_stream_timeout_s

    def method_client(self, rpcs: client.PendingRpcs, channel: Channel,
                      method: Method) -> _MethodClient:
        """Returns an object that invokes a method using the given chanel."""

        if method.type is Method.Type.UNARY:
            return unary_method_client(self, rpcs, channel, method,
                                       self.default_unary_timeout_s)

        if method.type is Method.Type.SERVER_STREAMING:
            return server_streaming_method_client(
                self, rpcs, channel, method, self.default_stream_timeout_s)

        if method.type is Method.Type.CLIENT_STREAMING:
            return ClientStreamingMethodClient(self, rpcs, channel, method,
                                               self.default_unary_timeout_s)

        if method.type is Method.Type.BIDIRECTIONAL_STREAMING:
            return BidirectionalStreamingMethodClient(
                self, rpcs, channel, method, self.default_stream_timeout_s)

        raise AssertionError(f'Unknown method type {method.type}')

    def process_response(self,
                         rpcs: client.PendingRpcs,
                         rpc: client.PendingRpc,
                         context,
                         status: Optional[Status],
                         payload,
                         *,
                         args: tuple = (),
                         kwargs: dict = None) -> None:
        """Invokes the callback associated with this RPC.

        Any additional positional and keyword args passed through
        Client.process_packet are forwarded to the callback.
        """
        if kwargs is None:
            kwargs = {}

        try:
            context(rpc, status, payload, *args, **kwargs)
        except:  # pylint: disable=bare-except
            rpcs.send_cancel(rpc)
            _LOG.exception('Callback %s for %s raised exception', context, rpc)
