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
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import com.google.protobuf.ByteString;
import com.google.protobuf.MessageLite;
import dev.pigweed.pw_rpc.internal.Packet.PacketType;
import dev.pigweed.pw_rpc.internal.Packet.RpcPacket;
import org.junit.Rule;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

public final class RpcManagerTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  private static final Service SERVICE = new Service("pw.rpc.test1.TheTestService",
      Service.unaryMethod("SomeUnary", SomeMessage.class, SomeMessage.class),
      Service.serverStreamingMethod("SomeServerStreaming", SomeMessage.class, SomeMessage.class),
      Service.clientStreamingMethod("SomeClientStreaming", SomeMessage.class, SomeMessage.class),
      Service.bidirectionalStreamingMethod(
          "SomeBidiStreaming", SomeMessage.class, SomeMessage.class));

  private static final Method METHOD = SERVICE.method("SomeUnary");

  private static final SomeMessage REQUEST_PAYLOAD =
      SomeMessage.newBuilder().setMagicNumber(1337).build();
  private static final byte[] REQUEST = request(REQUEST_PAYLOAD);
  private static final int CHANNEL_ID = 555;

  @Mock private Channel.Output mockOutput;

  private final RpcManager manager = new RpcManager();
  private final PendingRpc rpc =
      PendingRpc.create(new Channel(CHANNEL_ID, bytes -> mockOutput.send(bytes)), SERVICE, METHOD);
  private final AbstractCall<MessageLite, MessageLite> call =
      new AbstractCall<MessageLite, MessageLite>(manager, rpc) {
        @Override
        void doHandleNext(MessageLite response) {}

        @Override
        void doHandleCompleted() {}

        @Override
        void doHandleError() {}
      };

  private static byte[] request(MessageLite payload) {
    return packetBuilder()
        .setType(PacketType.REQUEST)
        .setPayload(payload.toByteString())
        .build()
        .toByteArray();
  }

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

  @Test
  public void start_succeeds_rpcIsPending() throws Exception {
    manager.start(call, REQUEST_PAYLOAD);

    verify(mockOutput).send(REQUEST);
    assertThat(manager.abandon(call)).isTrue();
  }

  @Test
  public void start_sendingFails_callsHandleError() throws Exception {
    doThrow(new ChannelOutputException()).when(mockOutput).send(any());

    assertThrows(ChannelOutputException.class, () -> manager.start(call, REQUEST_PAYLOAD));

    verify(mockOutput).send(REQUEST);
    assertThat(manager.abandon(call)).isFalse();
  }

  @Test
  public void abandon_rpcNoLongerPending() throws Exception {
    manager.start(call, REQUEST_PAYLOAD);
    assertThat(manager.abandon(call)).isTrue();

    assertThat(manager.abandon(call)).isFalse();
  }

  @Test
  public void abandon_sendsNoPackets() throws Exception {
    manager.start(call, REQUEST_PAYLOAD);
    verify(mockOutput).send(REQUEST);
    verifyNoMoreInteractions(mockOutput);

    assertThat(manager.abandon(call)).isTrue();
  }

  @Test
  public void cancel_rpcNoLongerPending() throws Exception {
    manager.start(call, REQUEST_PAYLOAD);
    assertThat(manager.cancel(call)).isTrue();

    assertThat(manager.abandon(call)).isFalse();
  }

  @Test
  public void cancel_sendsCancelPacket() throws Exception {
    manager.start(call, REQUEST_PAYLOAD);
    assertThat(manager.cancel(call)).isTrue();

    verify(mockOutput).send(cancel());
  }

  @Test
  public void open_sendsNoPacketsButRpcIsPending() {
    manager.open(call);

    assertThat(manager.abandon(call)).isTrue();
    verifyNoInteractions(mockOutput);
  }

  @Test
  public void ignoresActionsIfCallIsNotPending() throws Exception {
    assertThat(manager.cancel(call)).isFalse();
    assertThat(manager.abandon(call)).isFalse();
    assertThat(manager.clientStream(call, REQUEST_PAYLOAD)).isFalse();
    assertThat(manager.clientStreamEnd(call)).isFalse();
  }

  @Test
  public void ignoresPacketsIfCallIsNotPending() {
    assertThat(manager.handleNext(call.rpc(), ByteString.EMPTY)).isFalse();
    assertThat(manager.handleUnaryCompleted(call.rpc(), ByteString.EMPTY, Status.OK)).isFalse();
    assertThat(manager.handleStreamCompleted(call.rpc(), Status.OK)).isFalse();
    assertThat(manager.handleError(call.rpc(), Status.INVALID_ARGUMENT)).isFalse();
  }
}
