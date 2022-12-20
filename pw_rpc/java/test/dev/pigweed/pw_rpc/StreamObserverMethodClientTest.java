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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.google.protobuf.MessageLite;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

public final class StreamObserverMethodClientTest {
  private static final Service SERVICE = new Service("pw.rpc.test1.TheTestService",
      Service.unaryMethod("SomeUnary", SomeMessage.class, AnotherMessage.class),
      Service.serverStreamingMethod("SomeServerStreaming", SomeMessage.class, AnotherMessage.class),
      Service.clientStreamingMethod("SomeClientStreaming", SomeMessage.class, AnotherMessage.class),
      Service.bidirectionalStreamingMethod(
          "SomeBidirectionalStreaming", SomeMessage.class, AnotherMessage.class));

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private StreamObserver<MessageLite> defaultObserver;
  @Mock private StreamObserver<AnotherMessage> observer;
  @Mock private Channel.Output channelOutput;

  // Wrap Channel.Output since channelOutput will be null when the channel is initialized.
  private final Channel channel = new Channel(1, bytes -> channelOutput.send(bytes));

  private final PendingRpc UNARY_RPC =
      PendingRpc.create(channel, SERVICE, SERVICE.method("SomeUnary"));
  private final PendingRpc SERVER_STREAMING_RPC =
      PendingRpc.create(channel, SERVICE, SERVICE.method("SomeServerStreaming"));
  private final PendingRpc CLIENT_STREAMING_RPC =
      PendingRpc.create(channel, SERVICE, SERVICE.method("SomeClientStreaming"));
  private final PendingRpc BIDIRECTIONAL_STREAMING_RPC =
      PendingRpc.create(channel, SERVICE, SERVICE.method("SomeBidirectionalStreaming"));

  private final RpcManager rpcManager = new RpcManager();
  private MethodClient unaryMethodClient;
  private MethodClient serverStreamingMethodClient;
  private MethodClient clientStreamingMethodClient;
  private MethodClient bidirectionalStreamingMethodClient;

  @Before
  public void createMethodClient() {
    unaryMethodClient = new MethodClient(rpcManager, UNARY_RPC, defaultObserver);
    serverStreamingMethodClient =
        new MethodClient(rpcManager, SERVER_STREAMING_RPC, defaultObserver);
    clientStreamingMethodClient =
        new MethodClient(rpcManager, CLIENT_STREAMING_RPC, defaultObserver);
    bidirectionalStreamingMethodClient =
        new MethodClient(rpcManager, BIDIRECTIONAL_STREAMING_RPC, defaultObserver);
  }

  @Test
  public void invokeWithNoObserver_usesDefaultObserver() throws Exception {
    unaryMethodClient.invokeUnary(SomeMessage.getDefaultInstance());
    AnotherMessage reply = AnotherMessage.newBuilder().setPayload("yo").build();

    rpcManager.handleNext(UNARY_RPC, reply.toByteString());

    verify(defaultObserver).onNext(reply);
  }

  @Test
  public void invoke_usesProvidedObserver() throws Exception {
    unaryMethodClient.invokeUnary(SomeMessage.getDefaultInstance(), observer);
    AnotherMessage reply = AnotherMessage.newBuilder().setPayload("yo").build();
    rpcManager.handleNext(UNARY_RPC, reply.toByteString());

    verify(observer).onNext(reply);
  }

  @Test
  public void invokeUnary_startsRpc() throws Exception {
    Call call = unaryMethodClient.invokeUnary(SomeMessage.getDefaultInstance());
    assertThat(call.active()).isTrue();
    verify(channelOutput, times(1)).send(any());
  }

  @Test
  public void openUnary_startsRpc() throws Exception {
    Call call = unaryMethodClient.openUnary(defaultObserver);
    assertThat(call.active()).isTrue();
    verify(channelOutput, never()).send(any());
  }

  @Test
  public void invokeServerStreaming_startsRpc() throws Exception {
    Call call = serverStreamingMethodClient.invokeServerStreaming(SomeMessage.getDefaultInstance());
    assertThat(call.active()).isTrue();
    verify(channelOutput, times(1)).send(any());
  }

  @Test
  public void openServerStreaming_startsRpc() throws Exception {
    Call call = serverStreamingMethodClient.openServerStreaming(defaultObserver);
    assertThat(call.active()).isTrue();
    verify(channelOutput, never()).send(any());
  }

  @Test
  public void invokeClientStreaming_startsRpc() throws Exception {
    Call call = clientStreamingMethodClient.invokeClientStreaming();
    assertThat(call.active()).isTrue();
    verify(channelOutput, times(1)).send(any());
  }

  @Test
  public void openClientStreaming_startsRpc() throws Exception {
    Call call = clientStreamingMethodClient.openClientStreaming(defaultObserver);
    assertThat(call.active()).isTrue();
    verify(channelOutput, never()).send(any());
  }

  @Test
  public void invokeBidirectionalStreaming_startsRpc() throws Exception {
    Call call = bidirectionalStreamingMethodClient.invokeBidirectionalStreaming();
    assertThat(call.active()).isTrue();
    verify(channelOutput, times(1)).send(any());
  }

  @Test
  public void openBidirectionalStreaming_startsRpc() throws Exception {
    Call call = bidirectionalStreamingMethodClient.openBidirectionalStreaming(defaultObserver);
    assertThat(call.active()).isTrue();
    verify(channelOutput, never()).send(any());
  }

  @Test
  public void invokeUnaryFuture_startsRpc() throws Exception {
    Call call = unaryMethodClient.invokeUnaryFuture(SomeMessage.getDefaultInstance());
    assertThat(call.active()).isTrue();
    verify(channelOutput, times(1)).send(any());
  }

  @Test
  public void invokeServerStreamingFuture_startsRpc() throws Exception {
    Call call = serverStreamingMethodClient.invokeServerStreamingFuture(
        SomeMessage.getDefaultInstance(), (msg) -> {});
    assertThat(call.active()).isTrue();
    verify(channelOutput, times(1)).send(any());
  }

  @Test
  public void invokeClientStreamingFuture_startsRpc() throws Exception {
    Call call = clientStreamingMethodClient.invokeClientStreamingFuture();
    assertThat(call.active()).isTrue();
    verify(channelOutput, times(1)).send(any());
  }

  @Test
  public void invokeBidirectionalStreamingFuture_startsRpc() throws Exception {
    Call call = bidirectionalStreamingMethodClient.invokeBidirectionalStreamingFuture((msg) -> {});
    assertThat(call.active()).isTrue();
    verify(channelOutput, times(1)).send(any());
  }

  @Test
  public void invokeUnary_serverStreamingRpc_throwsException() {
    assertThrows(UnsupportedOperationException.class,
        () -> serverStreamingMethodClient.invokeUnary(SomeMessage.getDefaultInstance()));
  }

  @Test
  public void invokeServerStreaming_unaryRpc_throwsException() {
    assertThrows(UnsupportedOperationException.class,
        () -> unaryMethodClient.invokeServerStreaming(SomeMessage.getDefaultInstance()));
  }

  @Test
  public void invokeClientStreaming_bidirectionalStreamingRpc_throwsException() {
    assertThrows(UnsupportedOperationException.class,
        () -> bidirectionalStreamingMethodClient.invokeUnary(SomeMessage.getDefaultInstance()));
  }

  @Test
  public void invokeBidirectionalStreaming_clientStreamingRpc_throwsException() {
    assertThrows(UnsupportedOperationException.class,
        () -> clientStreamingMethodClient.invokeBidirectionalStreaming());
  }
}
