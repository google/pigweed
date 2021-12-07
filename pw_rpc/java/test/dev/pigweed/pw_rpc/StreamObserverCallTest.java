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
import static org.mockito.Mockito.verify;

import dev.pigweed.pw.rpc.internal.Packet.PacketType;
import dev.pigweed.pw.rpc.internal.Packet.RpcPacket;
import dev.pigweed.pw_rpc.StreamObserverCall.BidirectionalStreamingFuture;
import dev.pigweed.pw_rpc.StreamObserverCall.ClientStreamingFuture;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

public final class StreamObserverCallTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  private static final Service SERVICE = new Service("pw.rpc.test1.TheTestService",
      Service.unaryMethod("SomeUnary", SomeMessage.class, AnotherMessage.class),
      Service.clientStreamingMethod("SomeClient", SomeMessage.class, AnotherMessage.class),
      Service.bidirectionalStreamingMethod(
          "SomeBidirectional", SomeMessage.class, AnotherMessage.class));
  private static final AnotherMessage PAYLOAD = AnotherMessage.getDefaultInstance();
  private static final Method METHOD = SERVICE.method("SomeUnary");
  private static final int CHANNEL_ID = 555;

  @Mock private StreamObserver<AnotherMessage> observer;
  @Mock private Channel.Output mockOutput;

  private final RpcManager rpcManager = new RpcManager();
  private final StreamObserverCall.Dispatcher dispatcher = new StreamObserverCall.Dispatcher();
  private StreamObserverCall<SomeMessage, AnotherMessage> streamObserverCall;
  private PendingRpc rpc;

  private static byte[] cancel() {
    return packetBuilder()
        .setType(PacketType.CLIENT_ERROR)
        .setStatus(Status.CANCELLED.code())
        .build()
        .toByteArray();
  }

  private static RpcPacket.Builder packetBuilder() {
    return RpcPacket.newBuilder()
        .setChannelId(CHANNEL_ID)
        .setServiceId(SERVICE.id())
        .setMethodId(METHOD.id());
  }

  @Before
  public void createCall() throws Exception {
    rpc = PendingRpc.create(new Channel(CHANNEL_ID, mockOutput), SERVICE, METHOD);
    streamObserverCall = StreamObserverCall.start(rpcManager, rpc, observer, null);
    rpcManager.start(rpc, streamObserverCall, PAYLOAD);
  }

  @Test
  public void startsActive() {
    assertThat(streamObserverCall.active()).isTrue();
  }

  @Test
  public void startsWithNullStatus() {
    assertThat(streamObserverCall.status()).isNull();
  }

  @Test
  public void cancel_sendsCancelPacket() throws Exception {
    streamObserverCall.cancel();

    verify(mockOutput).send(cancel());
  }

  @Test
  public void cancel_deactivates() throws Exception {
    streamObserverCall.cancel();

    assertThat(streamObserverCall.active()).isFalse();
  }

  @Test
  public void send_sendsClientStreamPacket() throws Exception {
    SomeMessage request = SomeMessage.newBuilder().setMagicNumber(123).build();
    streamObserverCall.send(request);

    verify(mockOutput)
        .send(packetBuilder()
                  .setType(PacketType.CLIENT_STREAM)
                  .setPayload(request.toByteString())
                  .build()
                  .toByteArray());
  }

  @Test
  public void send_raisesExceptionIfClosed() throws Exception {
    streamObserverCall.cancel();

    RpcError thrown = assertThrows(
        RpcError.class, () -> streamObserverCall.send(SomeMessage.getDefaultInstance()));
    assertThat(thrown.status()).isSameInstanceAs(Status.CANCELLED);
  }

  @Test
  public void finish_clientStreamEndPacket() throws Exception {
    streamObserverCall.finish();

    verify(mockOutput)
        .send(packetBuilder().setType(PacketType.CLIENT_STREAM_END).build().toByteArray());
  }

  @Test
  public void onNext_callsObserverIfActive() {
    dispatcher.onNext(rpcManager, rpc, streamObserverCall, PAYLOAD);

    verify(observer).onNext(PAYLOAD);
  }

  @Test
  public void onNext_ignoresIfNotActive() throws Exception {
    streamObserverCall.cancel();
    dispatcher.onNext(rpcManager, rpc, streamObserverCall, PAYLOAD);

    verify(observer, never()).onNext(any());
  }

  @Test
  public void callDispatcher_onCompleted_callsObserver() {
    dispatcher.onCompleted(rpcManager, rpc, streamObserverCall, Status.ABORTED);

    verify(observer).onCompleted(Status.ABORTED);
  }

  @Test
  public void callDispatcher_onCompleted_setsActiveAndStatus() {
    dispatcher.onCompleted(rpcManager, rpc, streamObserverCall, Status.ABORTED);

    verify(observer).onCompleted(Status.ABORTED);
    assertThat(streamObserverCall.active()).isFalse();
    assertThat(streamObserverCall.status()).isEqualTo(Status.ABORTED);
  }

  @Test
  public void callDispatcher_onError_callsObserver() {
    dispatcher.onError(rpcManager, rpc, streamObserverCall, Status.NOT_FOUND);

    verify(observer).onError(Status.NOT_FOUND);
  }

  @Test
  public void callDispatcher_onError_deactivates() {
    dispatcher.onError(rpcManager, rpc, streamObserverCall, Status.ABORTED);

    verify(observer).onError(Status.ABORTED);
    assertThat(streamObserverCall.active()).isFalse();
    assertThat(streamObserverCall.status()).isNull();
  }

  @Test
  public void unaryfuture_response_setsValue() throws Exception {
    Call.UnaryFuture<AnotherMessage> call =
        new ClientStreamingFuture<>(rpcManager, rpc, new RpcFuture.Unary<>(rpc), PAYLOAD);

    AnotherMessage response = AnotherMessage.newBuilder().setResultValue(1138).build();
    dispatcher.onNext(rpcManager, rpc, call, response);
    assertThat(call.future().isDone()).isFalse();
    dispatcher.onCompleted(rpcManager, rpc, call, Status.CANCELLED);

    assertThat(call.future().isDone()).isTrue();
    assertThat(call.future().get()).isEqualTo(UnaryResult.create(response, Status.CANCELLED));
  }

  @Test
  public void unaryfuture_serverError_setsException() throws Exception {
    Call.UnaryFuture<AnotherMessage> call =
        new ClientStreamingFuture<>(rpcManager, rpc, new RpcFuture.Unary<>(rpc), PAYLOAD);

    dispatcher.onError(rpcManager, rpc, call, Status.NOT_FOUND);

    assertThat(call.future().isDone()).isTrue();
    ExecutionException exception = assertThrows(ExecutionException.class, call.future()::get);
    assertThat(exception).hasCauseThat().isInstanceOf(RpcError.class);

    RpcError error = (RpcError) exception.getCause();
    assertThat(error).isNotNull();
    assertThat(error.rpc()).isEqualTo(rpc);
    assertThat(error.status()).isEqualTo(Status.NOT_FOUND);
  }

  @Test
  public void unaryfuture_noMessage_setsException() throws Exception {
    Call.UnaryFuture<AnotherMessage> call =
        new ClientStreamingFuture<>(rpcManager, rpc, new RpcFuture.Unary<>(rpc), PAYLOAD);

    dispatcher.onCompleted(rpcManager, rpc, call, Status.OK);

    assertThat(call.future().isDone()).isTrue();
    ExecutionException exception = assertThrows(ExecutionException.class, call.future()::get);
    assertThat(exception).hasCauseThat().isInstanceOf(IllegalStateException.class);
  }

  @Test
  public void unaryfuture_multipleResponses_setsException() throws Exception {
    Call.UnaryFuture<AnotherMessage> call =
        new ClientStreamingFuture<>(rpcManager, rpc, new RpcFuture.Unary<>(rpc), PAYLOAD);

    AnotherMessage response = AnotherMessage.newBuilder().setResultValue(1138).build();
    dispatcher.onNext(rpcManager, rpc, call, response);
    dispatcher.onNext(rpcManager, rpc, call, response);
    dispatcher.onCompleted(rpcManager, rpc, call, Status.OK);

    assertThat(call.future().isDone()).isTrue();
    ExecutionException exception = assertThrows(ExecutionException.class, call.future()::get);
    assertThat(exception).hasCauseThat().isInstanceOf(IllegalStateException.class);
  }

  @Test
  public void bidirectionalStreamingfuture_responses_setsValue() throws Exception {
    List<AnotherMessage> responses = new ArrayList<>();
    Call.BidirectionalStreamingFuture<SomeMessage> call =
        new BidirectionalStreamingFuture<SomeMessage, AnotherMessage>(
            rpcManager, rpc, new RpcFuture.Stream<>(rpc, responses::add), null);

    AnotherMessage response = AnotherMessage.newBuilder().setResultValue(1138).build();
    dispatcher.onNext(rpcManager, rpc, call, response);
    dispatcher.onNext(rpcManager, rpc, call, response);
    assertThat(call.future().isDone()).isFalse();
    dispatcher.onCompleted(rpcManager, rpc, call, Status.OK);

    assertThat(call.future().isDone()).isTrue();
    assertThat(call.future().get()).isEqualTo(Status.OK);
    assertThat(responses).containsExactly(response, response);
  }

  @Test
  public void bidirectionalStreamingfuture_serverError_setsException() throws Exception {
    Call.BidirectionalStreamingFuture<SomeMessage> call = new BidirectionalStreamingFuture<>(
        rpcManager, rpc, new RpcFuture.Stream<>(rpc, (msg) -> {}), null);

    dispatcher.onError(rpcManager, rpc, call, Status.NOT_FOUND);

    assertThat(call.future().isDone()).isTrue();
    ExecutionException exception = assertThrows(ExecutionException.class, call.future()::get);
    assertThat(exception).hasCauseThat().isInstanceOf(RpcError.class);

    RpcError error = (RpcError) exception.getCause();
    assertThat(error).isNotNull();
    assertThat(error.rpc()).isEqualTo(rpc);
    assertThat(error.status()).isEqualTo(Status.NOT_FOUND);
  }
}
