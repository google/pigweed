// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

package dev.pigweed.pw_rpc;

import com.google.common.util.concurrent.ListenableFuture;
import com.google.protobuf.MessageLite;
import dev.pigweed.pw_rpc.Call.ClientStreaming;
import javax.annotation.Nullable;

/**
 * Represents an ongoing RPC call. Uses the StreamObserver interface responding to RPC calls.
 * Intended for use with the StreamObserverClient implementation.
 *
 * <p>This call class implements all features of unary, server streaming, client streaming, and
 * bidirectional streaming RPCs. It provides static methods for creating call objects for each RPC
 * type.
 *
 * @param <RequestT> request type of the RPC; used for client or bidirectional streaming RPCs
 * @param <ResponseT> response type of the RPC; used for all types of RPCs
 */
class StreamObserverCall<RequestT extends MessageLite, ResponseT extends MessageLite>
    implements ClientStreaming<RequestT> {
  private final RpcManager rpcs;
  private final PendingRpc rpc;
  private final StreamObserver<ResponseT> observer;

  @Nullable private Status status = null;
  @Nullable private Status error = null;

  /** Invokes the specified RPC. */
  static <RequestT extends MessageLite, ResponseT extends MessageLite>
      StreamObserverCall<RequestT, ResponseT> start(RpcManager rpcs,
          PendingRpc rpc,
          StreamObserver<ResponseT> observer,
          @Nullable MessageLite request) throws ChannelOutputException {
    StreamObserverCall<RequestT, ResponseT> call = new StreamObserverCall<>(rpcs, rpc, observer);
    rpcs.start(rpc, call, request);
    return call;
  }

  /** Invokes the specified RPC, ignoring errors that occur when the RPC is invoked. */
  static <RequestT extends MessageLite, ResponseT extends MessageLite>
      StreamObserverCall<RequestT, ResponseT> open(RpcManager rpcs,
          PendingRpc rpc,
          StreamObserver<ResponseT> observer,
          @Nullable MessageLite request) {
    StreamObserverCall<RequestT, ResponseT> call = new StreamObserverCall<>(rpcs, rpc, observer);
    rpcs.open(rpc, call, request);
    return call;
  }

  private StreamObserverCall(RpcManager rpcs, PendingRpc rpc, StreamObserver<ResponseT> observer) {
    this.rpcs = rpcs;
    this.rpc = rpc;
    this.observer = observer;
  }

  @Override
  public void cancel() throws ChannelOutputException {
    if (active()) {
      error = Status.CANCELLED;
      rpcs.cancel(rpc);
    }
  }

  @Override
  @Nullable
  public Status status() {
    return status;
  }

  @Nullable
  @Override
  public Status error() {
    return error;
  }

  @Override
  public void send(RequestT request) throws ChannelOutputException, RpcError {
    if (error != null) {
      throw new RpcError(rpc, error);
    }
    if (status != null) {
      throw new RpcError(rpc, Status.FAILED_PRECONDITION);
    }
    rpcs.clientStream(rpc, request);
  }

  @Override
  public void finish() throws ChannelOutputException {
    if (active()) {
      rpcs.clientStreamEnd(rpc);
    }
  }

  static class ClientStreamingFuture<RequestT extends MessageLite, ResponseT extends MessageLite>
      extends StreamObserverCall<RequestT, ResponseT>
      implements Call.ClientStreamingFuture<RequestT, ResponseT> {
    RpcFuture.Unary<ResponseT> future;

    ClientStreamingFuture(RpcManager rpcs,
        PendingRpc rpc,
        RpcFuture.Unary<ResponseT> future,
        @Nullable RequestT request) throws ChannelOutputException {
      super(rpcs, rpc, future);
      rpcs.start(rpc, this, request);
      this.future = future;
    }

    @Override
    public ListenableFuture<UnaryResult<ResponseT>> future() {
      return future;
    }
  }

  static class BidirectionalStreamingFuture<RequestT extends MessageLite, ResponseT
                                                extends MessageLite>
      extends StreamObserverCall<RequestT, ResponseT>
      implements Call.BidirectionalStreamingFuture<RequestT> {
    RpcFuture.Stream<ResponseT> future;

    BidirectionalStreamingFuture(RpcManager rpcs,
        PendingRpc rpc,
        RpcFuture.Stream<ResponseT> future,
        @Nullable RequestT request) throws ChannelOutputException {
      super(rpcs, rpc, future);
      rpcs.start(rpc, this, request);
      this.future = future;
    }

    @Override
    public ListenableFuture<Status> future() {
      return future;
    }
  }

  /** This class dispatches RPC events to Call objects. */
  static class Dispatcher implements ResponseProcessor {
    @Override
    public void onNext(RpcManager rpcs, PendingRpc rpc, Object context, MessageLite payload) {
      StreamObserverCall<MessageLite, MessageLite> call = getCall(context);
      if (call.active()) {
        call.observer.onNext(payload);
      }
    }

    @Override
    public void onCompleted(RpcManager rpcs, PendingRpc rpc, Object context, Status status) {
      StreamObserverCall<MessageLite, MessageLite> call = getCall(context);
      if (call.active()) {
        call.status = status;
        call.observer.onCompleted(status);
      }
    }

    @Override
    public void onError(RpcManager rpcs, PendingRpc rpc, Object context, Status status) {
      StreamObserverCall<MessageLite, MessageLite> call = getCall(context);
      if (call.active()) {
        call.error = status;
        call.observer.onError(status);
      }
    }

    @SuppressWarnings("unchecked")
    private static StreamObserverCall<MessageLite, MessageLite> getCall(Object context) {
      return (StreamObserverCall<MessageLite, MessageLite>) context;
    }
  }
}
