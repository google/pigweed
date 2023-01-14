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
import dev.pigweed.pw_rpc.FutureCall.StreamResponseFuture;
import dev.pigweed.pw_rpc.FutureCall.UnaryResponseFuture;
import java.util.function.BiFunction;
import java.util.function.Consumer;
import javax.annotation.Nullable;

/**
 * Represents a method ready to be invoked on a particular RPC channel.
 *
 * Invoking an RPC with a method client may throw exceptions:
 *
 * TODO(hepler): This class should be split into four types -- one for each method type. The call
 *     type checks should be done when the object is created. Also, the client should be typed on
 *     the request/response.
 */
public class MethodClient {
  private final Client client;
  private final int channelId;
  private final Method method;
  private final StreamObserver<? extends MessageLite> defaultObserver;

  MethodClient(
      Client client, int channelId, Method method, StreamObserver<MessageLite> defaultObserver) {
    this.client = client;
    this.channelId = channelId;
    this.method = method;
    this.defaultObserver = defaultObserver;
  }

  public final Method method() {
    return method;
  }

  /**
   * Invokes a unary RPC. Uses the default StreamObserver for RPC events.
   *
   * @throws InvalidRpcChannelException the client has no channel with this ID
   * @throws InvalidRpcServiceException if the service was removed from the client
   * @throws InvalidRpcServiceMethodException if the method was removed or changed since this
   *     MethodClient was created
   */
  public Call invokeUnary(MessageLite request) throws ChannelOutputException {
    return invokeUnary(request, defaultObserver());
  }

  /** Invokes a unary RPC. Uses the provided StreamObserver for RPC events. */
  public Call invokeUnary(MessageLite request, StreamObserver<? extends MessageLite> observer)
      throws ChannelOutputException {
    checkCallType(Method.Type.UNARY);
    return client.invokeRpc(channelId, method, StreamObserverCall.getFactory(observer), request);
  }

  /** Invokes a unary RPC with a future that collects the response. */
  public <ResponseT extends MessageLite> Call.UnaryFuture<ResponseT> invokeUnaryFuture(
      MessageLite request) {
    checkCallType(Method.Type.UNARY);
    return invokeFuture(UnaryResponseFuture::new, request);
  }

  /**
   * Creates a call object for a unary RPC without starting the RPC on the server. This can be used
   * to start listening to responses to an RPC before the RPC server is available.
   *
   * <p>The RPC remains open until it is completed by the server with a response or error packet or
   * cancelled.
   */
  public Call openUnary(StreamObserver<? extends MessageLite> observer) {
    checkCallType(Method.Type.UNARY);
    return client.openRpc(channelId, method, StreamObserverCall.getFactory(observer));
  }

  /** Invokes a server streaming RPC. Uses the default StreamObserver for RPC events. */
  public Call invokeServerStreaming(MessageLite request) throws ChannelOutputException {
    return invokeServerStreaming(request, defaultObserver());
  }

  /** Invokes a server streaming RPC. Uses the provided StreamObserver for RPC events. */
  public Call invokeServerStreaming(MessageLite request,
      StreamObserver<? extends MessageLite> observer) throws ChannelOutputException {
    checkCallType(Method.Type.SERVER_STREAMING);
    return client.invokeRpc(channelId, method, StreamObserverCall.getFactory(observer), request);
  }

  /** Invokes a server streaming RPC with a future that collects the responses. */
  public Call.ServerStreamingFuture invokeServerStreamingFuture(
      MessageLite request, Consumer<? extends MessageLite> onNext) {
    checkCallType(Method.Type.SERVER_STREAMING);
    return invokeFuture(StreamResponseFuture.getFactory(onNext), request);
  }

  /**
   * Creates a call object for a server streaming RPC without starting the RPC on the server. This
   * can be used to start listening to responses to an RPC before the RPC server is available.
   *
   * <p>The RPC remains open until it is completed by the server with a response or error packet or
   * cancelled.
   */
  public Call openServerStreaming(StreamObserver<? extends MessageLite> observer) {
    checkCallType(Method.Type.SERVER_STREAMING);
    return client.openRpc(channelId, method, StreamObserverCall.getFactory(observer));
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
    return client.invokeRpc(channelId, method, StreamObserverCall.getFactory(observer), null);
  }

  /** Invokes a client streaming RPC with a future that collects the response. */
  public <RequestT extends MessageLite> Call.ClientStreaming<RequestT>
  invokeClientStreamingFuture() {
    checkCallType(Method.Type.CLIENT_STREAMING);
    return invokeFuture(UnaryResponseFuture::new, null);
  }

  /**
   * Creates a call object for a client streaming RPC without starting the RPC on the server. This
   * can be used to start listening to responses to an RPC before the RPC server is available.
   *
   * <p>The RPC remains open until it is completed by the server with a response or error packet or
   * cancelled.
   */
  public <RequestT extends MessageLite> Call.ClientStreaming<RequestT> openClientStreaming(
      StreamObserver<? extends MessageLite> observer) {
    checkCallType(Method.Type.CLIENT_STREAMING);
    return client.openRpc(channelId, method, StreamObserverCall.getFactory(observer));
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
    return client.invokeRpc(channelId, method, StreamObserverCall.getFactory(observer), null);
  }

  /** Invokes a bidirectional streaming RPC with a future that finishes when the RPC finishes. */
  public <RequestT extends MessageLite, ResponseT extends MessageLite>
      Call.BidirectionalStreamingFuture<RequestT> invokeBidirectionalStreamingFuture(
          Consumer<ResponseT> onNext) {
    checkCallType(Method.Type.BIDIRECTIONAL_STREAMING);
    return invokeFuture(StreamResponseFuture.getFactory(onNext), null);
  }

  /**
   * Creates a call object for a bidirectional streaming RPC without starting the RPC on the server.
   * This can be used to start listening to responses to an RPC before the RPC server is available.
   *
   * <p>The RPC remains open until it is completed by the server with a response or error packet or
   * cancelled.
   */
  public <RequestT extends MessageLite> Call.ClientStreaming<RequestT> openBidirectionalStreaming(
      StreamObserver<? extends MessageLite> observer) {
    checkCallType(Method.Type.BIDIRECTIONAL_STREAMING);
    return client.openRpc(channelId, method, StreamObserverCall.getFactory(observer));
  }

  @SuppressWarnings("unchecked")
  private <ResponseT extends MessageLite> StreamObserver<ResponseT> defaultObserver() {
    return (StreamObserver<ResponseT>) defaultObserver;
  }

  public <RequestT extends MessageLite, ResponseT extends MessageLite, CallT
              extends FutureCall<RequestT, ResponseT, ?>> CallT
  invokeFuture(BiFunction<Endpoint, PendingRpc, CallT> createCall, @Nullable MessageLite request) {
    try {
      return client.invokeRpc(channelId, method, createCall, request);
    } catch (ChannelOutputException e) {
      throw new AssertionError("Starting a future-based RPC call should never throw", e);
    }
  }

  private void checkCallType(Method.Type expected) {
    if (!method.type().equals(expected)) {
      throw new UnsupportedOperationException(String.format(
          "%s is a %s method, but it was invoked as a %s method. RPCs must be invoked by the"
              + " appropriate invoke function.",
          method.fullName(),
          method.type().sentenceName(),
          expected.sentenceName()));
    }
  }
}
