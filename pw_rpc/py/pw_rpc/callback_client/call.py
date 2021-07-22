# Copyright 2021 The Pigweed Authors
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
"""Classes for handling ongoing RPC calls."""

import enum
import logging
import queue
from typing import (Any, Callable, Iterable, Iterator, NamedTuple, Union,
                    Optional, Sequence, TypeVar)

from pw_protobuf_compiler.python_protos import proto_repr
from pw_status import Status
from google.protobuf.message import Message

from pw_rpc.callback_client.errors import RpcTimeout, RpcError
from pw_rpc.client import PendingRpc, PendingRpcs
from pw_rpc.descriptors import Method

_LOG = logging.getLogger(__package__)


class UseDefault(enum.Enum):
    """Marker for args that should use a default value, when None is valid."""
    VALUE = 0


CallType = TypeVar('CallType', 'UnaryCall', 'ServerStreamingCall',
                   'ClientStreamingCall', 'BidirectionalStreamingCall')

OnNextCallback = Callable[[CallType, Any], Any]
OnCompletedCallback = Callable[[CallType, Any], Any]
OnErrorCallback = Callable[[CallType, Any], Any]

OptionalTimeout = Union[UseDefault, float, None]


class InternalCallbacks(NamedTuple):
    """Callbacks used with the low-level pw_rpc client interface."""
    on_next: Callable[[Message], None]
    on_completed: Callable[[Status], None]
    on_error: Callable[[Status], None]


class UnaryResponse(NamedTuple):
    """Result from a unary or client streaming RPC: status and response."""
    status: Status
    response: Any

    def __repr__(self) -> str:
        return f'({self.status}, {proto_repr(self.response)})'


class StreamResponse(NamedTuple):
    """Results from a server or bidirectional streaming RPC."""
    status: Status
    responses: Sequence[Any]

    def __repr__(self) -> str:
        return (f'({self.status}, '
                f'[{", ".join(proto_repr(r) for r in self.responses)}])')


class _Call:
    """Represents an in-progress or completed RPC call."""
    def __init__(self, rpcs: PendingRpcs, rpc: PendingRpc,
                 default_timeout_s: Optional[float],
                 on_next: Optional[OnNextCallback],
                 on_completed: Optional[OnCompletedCallback],
                 on_error: Optional[OnErrorCallback]) -> None:
        self._rpcs = rpcs
        self._rpc = rpc
        self.default_timeout_s = default_timeout_s

        self.status: Optional[Status] = None
        self.error: Optional[RpcError] = None
        self._responses: list = []
        self._response_queue: queue.SimpleQueue = queue.SimpleQueue()

        self.on_next = on_next or _Call._default_response
        self.on_completed = on_completed or _Call._default_completion
        self.on_error = on_error or _Call._default_error

    def _invoke(self, request: Optional[Message]) -> None:
        """Calls the RPC. This must be called immediately after __init__."""
        self._rpcs.send_request(self._rpc,
                                request,
                                InternalCallbacks(self._handle_response,
                                                  self._handle_completion,
                                                  self._handle_error),
                                override_pending=True)

    def _default_response(self, response: Message) -> None:
        _LOG.debug('%s received response: %s', self._rpc, response)

    def _default_completion(self, status: Status) -> None:
        _LOG.info('%s completed: %s', self._rpc, status)

    def _default_error(self, error: Status) -> None:
        _LOG.warning('%s termianted due to an error: %s', self._rpc, error)

    @property
    def method(self) -> Method:
        return self._rpc.method

    def completed(self) -> bool:
        """True if the RPC call has completed, successfully or from an error."""
        return self.status is not None or self.error is not None

    def _send_client_stream(self, request_proto, request_fields: dict) -> None:
        """Sends a client to the server in the client stream."""
        if self.error:
            raise self.error

        if self.status is not None:
            raise RpcError(self._rpc, Status.FAILED_PRECONDITION)

        self._rpcs.send_client_stream(
            self._rpc, self.method.get_request(request_proto, request_fields))

    def _finish_client_stream(self, requests: Iterable[Message]) -> None:
        for request in requests:
            self._send_client_stream(request, {})

        if not self.completed():
            self._rpcs.send_client_stream_end(self._rpc)

    def _unary_wait(self, timeout_s: OptionalTimeout) -> UnaryResponse:
        """Waits until the RPC has completed."""
        for _ in self._get_responses(block=True, timeout_s=timeout_s):
            pass

        assert self.status is not None and self._responses
        return UnaryResponse(self.status, self._responses[-1])

    def _stream_wait(self, timeout_s: OptionalTimeout) -> StreamResponse:
        """Waits until the RPC has completed."""
        for _ in self._get_responses(block=True, timeout_s=timeout_s):
            pass

        assert self.status is not None
        return StreamResponse(self.status, self._responses)

    def _get_responses(self, *, block: bool,
                       timeout_s: OptionalTimeout) -> Iterator:
        """Returns an iterator of stream responses.

        Args:
          timeout_s: timeout in seconds; None blocks indefinitely
        """
        if self.error:
            raise self.error

        if self.completed():
            return

        if timeout_s is UseDefault.VALUE:
            timeout_s = self.default_timeout_s

        try:
            while True:
                response = self._response_queue.get(block, timeout_s)

                if isinstance(response, Exception):
                    raise response

                if isinstance(response, Status):
                    return

                yield response
        except queue.Empty:
            self.cancel()
            raise RpcTimeout(self._rpc, timeout_s)
        except:
            self.cancel()
            raise

    def cancel(self) -> bool:
        """Cancels the RPC; returns whether the RPC was active."""
        if self.completed():
            return False

        self.error = RpcError(self._rpc, Status.CANCELLED)
        return self._rpcs.send_cancel(self._rpc)

    def _handle_response(self, response: Any) -> None:
        # TODO(frolv): These lists could grow very large for persistent
        # streaming RPCs such as logs. The size should be limited.
        self._responses.append(response)
        self._response_queue.put(response)

        self.on_next(self, response)

    def _handle_completion(self, status: Status) -> None:
        self.status = status
        self._response_queue.put(status)

        self.on_completed(self, status)

    def _handle_error(self, error: Status) -> None:
        self.error = RpcError(self._rpc, error)
        self._response_queue.put(self.error)

        self.on_error(self, error)

    def __enter__(self) -> '_Call':
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.cancel()

    def __repr__(self) -> str:
        return f'{type(self).__name__}({self.method})'


class UnaryCall(_Call):
    """Tracks the state of a unary RPC call."""
    @property
    def response(self) -> Any:
        return self._responses[-1] if self._responses else None

    def wait(self,
             timeout_s: OptionalTimeout = UseDefault.VALUE) -> UnaryResponse:
        return self._unary_wait(timeout_s)


class ServerStreamingCall(_Call):
    """Tracks the state of a server streaming RPC call."""
    @property
    def responses(self) -> Sequence:
        return self._responses

    def wait(self,
             timeout_s: OptionalTimeout = UseDefault.VALUE) -> StreamResponse:
        return self._stream_wait(timeout_s)

    def get_responses(
            self,
            *,
            block: bool = True,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> Iterator:
        return self._get_responses(block=block, timeout_s=timeout_s)

    def __iter__(self) -> Iterator:
        return self.get_responses()


class ClientStreamingCall(_Call):
    """Tracks the state of a client streaming RPC call."""
    @property
    def response(self) -> Any:
        return self._responses[-1] if self._responses else None

    # TODO(hepler): Use / to mark the first arg as positional-only
    #     when when Python 3.7 support is no longer required.
    def send(self,
             _rpc_request_proto: Message = None,
             **request_fields) -> None:
        """Sends client stream request to the server."""
        self._send_client_stream(_rpc_request_proto, request_fields)

    def finish_and_wait(
            self,
            requests: Iterable[Message] = (),
            *,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> UnaryResponse:
        """Ends the client stream and waits for the RPC to complete."""
        self._finish_client_stream(requests)
        return self._unary_wait(timeout_s)

    def get_responses(
            self,
            *,
            block: bool = True,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> Iterator:
        return self._get_responses(block=block, timeout_s=timeout_s)

    def __iter__(self) -> Iterator:
        return self.get_responses()


class BidirectionalStreamingCall(_Call):
    """Tracks the state of a bidirectional streaming RPC call."""
    @property
    def responses(self) -> Sequence:
        return self._responses

    # TODO(hepler): Use / to mark the first arg as positional-only
    #     when when Python 3.7 support is no longer required.
    def send(self,
             _rpc_request_proto: Message = None,
             **request_fields) -> None:
        """Sends a message to the server in the client stream."""
        self._send_client_stream(_rpc_request_proto, request_fields)

    def finish_and_wait(
            self,
            requests: Iterable[Message] = (),
            *,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> StreamResponse:
        """Ends the client stream and waits for the RPC to complete."""
        self._finish_client_stream(requests)
        return self._stream_wait(timeout_s)

    def get_responses(
            self,
            *,
            block: bool = True,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> Iterator:
        return self._get_responses(block=block, timeout_s=timeout_s)

    def __iter__(self) -> Iterator:
        return self.get_responses()
