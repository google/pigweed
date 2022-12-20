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
import static com.google.common.util.concurrent.MoreExecutors.directExecutor;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.google.protobuf.ByteString;
import dev.pigweed.pw_rpc.Call.UnaryFuture;
import dev.pigweed.pw_rpc.FutureCall.StreamResponseFuture;
import dev.pigweed.pw_rpc.FutureCall.UnaryResponseFuture;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import org.junit.Rule;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

public final class FutureCallTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  private static final Service SERVICE = new Service("pw.rpc.test1.TheTestService",
      Service.unaryMethod("SomeUnary", SomeMessage.class, AnotherMessage.class),
      Service.clientStreamingMethod("SomeClient", SomeMessage.class, AnotherMessage.class),
      Service.bidirectionalStreamingMethod(
          "SomeBidirectional", SomeMessage.class, AnotherMessage.class));
  private static final Method METHOD = SERVICE.method("SomeUnary");
  private static final int CHANNEL_ID = 555;

  @Mock private Channel.Output mockOutput;

  private final Endpoint endpoint = new Endpoint();
  private final PendingRpc rpc = PendingRpc.create(
      new Channel(CHANNEL_ID, packet -> mockOutput.send(packet)), SERVICE, METHOD);

  @Test
  public void unaryFuture_response_setsValue() throws Exception {
    UnaryResponseFuture<SomeMessage, AnotherMessage> call =
        new UnaryResponseFuture<>(endpoint, rpc, SomeMessage.getDefaultInstance());

    AnotherMessage response = AnotherMessage.newBuilder().setResultValue(1138).build();
    call.handleUnaryCompleted(response.toByteString(), Status.CANCELLED);

    assertThat(call.isDone()).isTrue();
    assertThat(call.get()).isEqualTo(UnaryResult.create(response, Status.CANCELLED));
  }

  @Test
  public void unaryFuture_serverError_setsException() {
    UnaryResponseFuture<SomeMessage, AnotherMessage> call =
        new UnaryResponseFuture<>(endpoint, rpc, SomeMessage.getDefaultInstance());

    call.handleError(Status.NOT_FOUND);

    assertThat(call.isDone()).isTrue();
    ExecutionException exception = assertThrows(ExecutionException.class, call::get);
    assertThat(exception).hasCauseThat().isInstanceOf(RpcError.class);

    RpcError error = (RpcError) exception.getCause();
    assertThat(error).isNotNull();
    assertThat(error.rpc()).isEqualTo(rpc);
    assertThat(error.status()).isEqualTo(Status.NOT_FOUND);
  }

  @Test
  public void unaryFuture_cancelOnCall_cancelsTheCallAndFuture() throws Exception {
    UnaryFuture<SomeMessage> call =
        new UnaryResponseFuture<>(endpoint, rpc, SomeMessage.getDefaultInstance());
    assertThat(call.cancel()).isTrue();
    assertThat(call.isCancelled()).isTrue();

    ExecutionException exception = assertThrows(ExecutionException.class, call::get);
    assertThat(exception).hasCauseThat().isInstanceOf(RpcError.class);

    RpcError error = (RpcError) exception.getCause();
    assertThat(error).isNotNull();
    assertThat(error.rpc()).isEqualTo(rpc);
    assertThat(error.status()).isEqualTo(Status.CANCELLED);
  }

  @Test
  public void unaryFuture_cancelOnFuture_cancelsTheCallAndFuture() {
    UnaryFuture<SomeMessage> call =
        new UnaryResponseFuture<>(endpoint, rpc, SomeMessage.getDefaultInstance());
    assertThat(call.cancel(true)).isTrue();
    assertThat(call.isCancelled()).isTrue();

    ExecutionException exception = assertThrows(ExecutionException.class, call::get);
    assertThat(exception).hasCauseThat().isInstanceOf(RpcError.class);

    RpcError error = (RpcError) exception.getCause();
    assertThat(error).isNotNull();
    assertThat(error.rpc()).isEqualTo(rpc);
    assertThat(error.status()).isEqualTo(Status.CANCELLED);
  }

  @Test
  public void unaryFuture_cancelOnFutureSendFails_cancelsTheCallAndFuture() throws Exception {
    UnaryFuture<SomeMessage> call =
        new UnaryResponseFuture<>(endpoint, rpc, SomeMessage.getDefaultInstance());

    doThrow(new ChannelOutputException()).when(mockOutput).send(any());

    assertThat(call.cancel(true)).isTrue();
    assertThat(call.isCancelled()).isTrue();

    ExecutionException exception = assertThrows(ExecutionException.class, call::get);
    assertThat(exception).hasCauseThat().isInstanceOf(RpcError.class);

    RpcError error = (RpcError) exception.getCause();
    assertThat(error).isNotNull();
    assertThat(error.rpc()).isEqualTo(rpc);
    assertThat(error.status()).isEqualTo(Status.CANCELLED);
  }

  @Test
  public void unaryFuture_multipleResponses_setsException() {
    UnaryResponseFuture<SomeMessage, AnotherMessage> call =
        new UnaryResponseFuture<>(endpoint, rpc, SomeMessage.getDefaultInstance());

    AnotherMessage response = AnotherMessage.newBuilder().setResultValue(1138).build();
    call.doHandleNext(response);
    call.handleUnaryCompleted(ByteString.EMPTY, Status.OK);

    assertThat(call.isDone()).isTrue();
    ExecutionException exception = assertThrows(ExecutionException.class, call::get);
    assertThat(exception).hasCauseThat().isInstanceOf(IllegalStateException.class);
  }

  @Test
  public void unaryFuture_addListener_calledOnCompletion() {
    UnaryResponseFuture<SomeMessage, AnotherMessage> call =
        new UnaryResponseFuture<>(endpoint, rpc, SomeMessage.getDefaultInstance());

    Runnable listener = mock(Runnable.class);
    call.addListener(listener, directExecutor());

    AnotherMessage response = AnotherMessage.newBuilder().setResultValue(1138).build();
    call.handleUnaryCompleted(response.toByteString(), Status.OK);

    verify(listener, times(1)).run();
  }

  @Test
  public void unaryFuture_exceptionDuringStart() throws Exception {
    ChannelOutputException exceptionToThrow = new ChannelOutputException();
    doThrow(exceptionToThrow).when(mockOutput).send(any());

    UnaryResponseFuture<SomeMessage, AnotherMessage> call =
        new UnaryResponseFuture<>(endpoint, rpc, SomeMessage.getDefaultInstance());

    assertThat(call.error()).isEqualTo(Status.ABORTED);
    ExecutionException exception = assertThrows(ExecutionException.class, call::get);
    assertThat(exception).hasCauseThat().isInstanceOf(ChannelOutputException.class);

    assertThat(exception.getCause()).isSameInstanceAs(exceptionToThrow);
  }

  @Test
  public void bidirectionalStreamingFuture_responses_setsValue() throws Exception {
    List<AnotherMessage> responses = new ArrayList<>();
    StreamResponseFuture<SomeMessage, AnotherMessage> call =
        new StreamResponseFuture<>(endpoint, rpc, responses::add, null);

    AnotherMessage message = AnotherMessage.newBuilder().setResultValue(1138).build();
    call.doHandleNext(message);
    call.doHandleNext(message);
    assertThat(call.isDone()).isFalse();
    call.handleStreamCompleted(Status.OK);

    assertThat(call.isDone()).isTrue();
    assertThat(call.get()).isEqualTo(Status.OK);
    assertThat(responses).containsExactly(message, message);
  }

  @Test
  public void bidirectionalStreamingFuture_serverError_setsException() {
    StreamResponseFuture<SomeMessage, AnotherMessage> call =
        new StreamResponseFuture<>(endpoint, rpc, (msg) -> {}, null);

    call.handleError(Status.NOT_FOUND);

    assertThat(call.isDone()).isTrue();
    ExecutionException exception = assertThrows(ExecutionException.class, call::get);
    assertThat(exception).hasCauseThat().isInstanceOf(RpcError.class);

    RpcError error = (RpcError) exception.getCause();
    assertThat(error).isNotNull();
    assertThat(error.rpc()).isEqualTo(rpc);
    assertThat(error.status()).isEqualTo(Status.NOT_FOUND);
  }
}
