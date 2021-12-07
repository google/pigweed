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

import com.google.protobuf.MessageLite;
import dev.pigweed.pw_rpc.StreamObserverCall.BidirectionalStreamingFuture;
import dev.pigweed.pw_rpc.StreamObserverCall.ClientStreamingFuture;
import java.util.function.Consumer;

/**
 * A pw_rpc MethodClient implementation that uses a StreamObserver for all RPCs.
 *
 * <p>The semantics of the invoke functions follow the semantics of the Channel.Output
 * implementation for the channel.
 */
public class StreamObserverMethodClient extends MethodClient {
  private final StreamObserver<? extends MessageLite> defaultObserver;

  /**
   * Creates a new StreamObserverMethodClient for the specified RPC.
   *
   * @param rpcs RpcManager to use for RPC calls
   * @param rpc which RPC this is for
   * @param defaultObserver default StreamObserver to use when none is specified
   */
  StreamObserverMethodClient(
      RpcManager rpcs, PendingRpc rpc, StreamObserver<MessageLite> defaultObserver) {
    super(rpcs, rpc);
    this.defaultObserver = defaultObserver;
  }

  /** Invokes a unary RPC. Uses the default StreamObserver for RPC events. */
  public Call invokeUnary(MessageLite request) throws ChannelOutputException {
    return invokeUnary(request, defaultObserver());
  }

  /** Invokes a unary RPC. Uses the provided StreamObserver for RPC events. */
  public Call invokeUnary(MessageLite request, StreamObserver<? extends MessageLite> observer)
      throws ChannelOutputException {
    checkCallType(Method.Type.UNARY);
    return StreamObserverCall.start(rpcs(), rpc(), observer, request);
  }

  /** Invokes a unary RPC with a future that collects the response. */
  public <ResponseT extends MessageLite> Call.UnaryFuture<ResponseT> invokeUnaryFuture(
      MessageLite request) throws ChannelOutputException {
    checkCallType(Method.Type.UNARY);
    return new ClientStreamingFuture<>(rpcs(), rpc(), new RpcFuture.Unary<>(rpc()), request);
  }

  /**
   * Starts a unary RPC, ignoring any errors that occur when opening. This can be used to start
   * listening to responses to an RPC before the RPC server is available.
   *
   * <p>The RPC remains open until it is completed by the server with a response or error packet or
   * cancelled.
   */
  public Call openUnary(MessageLite request, StreamObserver<? extends MessageLite> observer) {
    checkCallType(Method.Type.UNARY);
    return StreamObserverCall.open(rpcs(), rpc(), observer, request);
  }

  /** Invokes a server streaming RPC. Uses the default StreamObserver for RPC events. */
  public Call invokeServerStreaming(MessageLite request) throws ChannelOutputException {
    return invokeServerStreaming(request, defaultObserver());
  }

  /** Invokes a server streaming RPC. Uses the provided StreamObserver for RPC events. */
  public Call invokeServerStreaming(MessageLite request,
      StreamObserver<? extends MessageLite> observer) throws ChannelOutputException {
    checkCallType(Method.Type.SERVER_STREAMING);
    return StreamObserverCall.start(rpcs(), rpc(), observer, request);
  }

  /** Invokes a server streaming RPC with a future that collects the responses. */
  public Call.ServerStreamingFuture invokeServerStreamingFuture(
      MessageLite request, Consumer<? extends MessageLite> onNext) throws ChannelOutputException {
    checkCallType(Method.Type.SERVER_STREAMING);
    return new BidirectionalStreamingFuture<>(
        rpcs(), rpc(), new RpcFuture.Stream<>(rpc(), onNext), request);
  }

  /**
   * Starts a server streaming RPC, ignoring any errors that occur when opening. This can be used to
   * start listening to responses to an RPC before the RPC server is available.
   *
   * <p>The RPC remains open until it is completed by the server with a response or error packet or
   * cancelled.
   */
  public Call openServerStreaming(
      MessageLite request, StreamObserver<? extends MessageLite> observer) {
    checkCallType(Method.Type.SERVER_STREAMING);
    return StreamObserverCall.open(rpcs(), rpc(), observer, request);
  }

  /** Invokes a client streaming RPC. Uses the default StreamObserver for RPC events. */
  public <RequestT extends MessageLite> Call.ClientStreaming<RequestT> invokeClientStreaming()
      throws ChannelOutputException {
    return invokeClientStreaming(defaultObserver());
  }

  /** Invokes a client streaming RPC. Uses the provided StreamObserver for RPC events. */
  public <RequestT extends MessageLite> Call.ClientStreaming<RequestT> invokeClientStreaming(
      StreamObserver<? extends MessageLite> observer) throws ChannelOutputException {
    checkCallType(Method.Type.CLIENT_STREAMING);
    return StreamObserverCall.start(rpcs(), rpc(), observer, null);
  }

  /** Invokes a client streaming RPC with a future that collects the response. */
  public <RequestT extends MessageLite> Call.ClientStreaming<RequestT> invokeClientStreamingFuture()
      throws ChannelOutputException {
    checkCallType(Method.Type.CLIENT_STREAMING);
    return new ClientStreamingFuture<>(rpcs(), rpc(), new RpcFuture.Unary<>(rpc()), null);
  }

  /**
   * Starts a client streaming RPC, ignoring any errors that occur when opening. This can be used to
   * start listening to responses to an RPC before the RPC server is available.
   *
   * <p>The RPC remains open until it is completed by the server with a response or error packet or
   * cancelled.
   */
  public <RequestT extends MessageLite> Call.ClientStreaming<RequestT> openClientStreaming(
      StreamObserver<? extends MessageLite> observer) {
    checkCallType(Method.Type.CLIENT_STREAMING);
    return StreamObserverCall.open(rpcs(), rpc(), observer, null);
  }

  /** Invokes a bidirectional streaming RPC. Uses the default StreamObserver for RPC events. */
  public <RequestT extends MessageLite> Call.ClientStreaming<RequestT>
  invokeBidirectionalStreaming() throws ChannelOutputException {
    return invokeBidirectionalStreaming(defaultObserver());
  }

  /** Invokes a bidirectional streaming RPC. Uses the provided StreamObserver for RPC events. */
  public <RequestT extends MessageLite> Call.ClientStreaming<RequestT> invokeBidirectionalStreaming(
      StreamObserver<? extends MessageLite> observer) throws ChannelOutputException {
    checkCallType(Method.Type.BIDIRECTIONAL_STREAMING);
    return StreamObserverCall.start(rpcs(), rpc(), observer, null);
  }

  /** Invokes a bidirectional streaming RPC with a future that finishes when the RPC finishes. */
  public <RequestT extends MessageLite, ResponseT extends MessageLite>
      Call.BidirectionalStreamingFuture<RequestT> invokeBidirectionalStreamingFuture(
          Consumer<ResponseT> onNext) throws ChannelOutputException {
    checkCallType(Method.Type.BIDIRECTIONAL_STREAMING);
    return new BidirectionalStreamingFuture<>(
        rpcs(), rpc(), new RpcFuture.Stream<>(rpc(), onNext), null);
  }

  /**
   * Starts a bidirectional streaming RPC, ignoring any errors that occur when opening. This can be
   * used to start listening to responses to an RPC before the RPC server is available.
   *
   * <p>The RPC remains open until it is completed by the server with a response or error packet or
   * cancelled.
   */
  public <RequestT extends MessageLite> Call.ClientStreaming<RequestT> openBidirectionalStreaming(
      StreamObserver<? extends MessageLite> observer) {
    checkCallType(Method.Type.BIDIRECTIONAL_STREAMING);
    return StreamObserverCall.open(rpcs(), rpc(), observer, null);
  }

  @SuppressWarnings("unchecked")
  private <ResponseT extends MessageLite> StreamObserver<ResponseT> defaultObserver() {
    return (StreamObserver<ResponseT>) defaultObserver;
  }

  private void checkCallType(Method.Type expected) {
    if (!rpc().method().type().equals(expected)) {
      throw new UnsupportedOperationException(String.format(
          "%s is a %s method, but it was invoked as a %s method. RPCs must be invoked by the"
              + " appropriate invoke function.",
          method().fullName(),
          method().type().sentenceName(),
          expected.sentenceName()));
    }
  }
}
