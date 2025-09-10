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

import com.google.protobuf.ExtensionRegistryLite;
import dev.pigweed.pw_rpc.internal.Packet.PacketType;
import dev.pigweed.pw_rpc.internal.Packet.RpcPacket;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class PacketsTest {
  private static final Service SERVICE = new Service(
      "Greetings", Service.unaryMethod("Hello", RpcPacket.parser(), RpcPacket.parser()));

  private static final PendingRpc RPC =
      PendingRpc.create(new Channel(123, null), SERVICE.method("Hello"), Endpoint.FIRST_CALL_ID);

  private static final RpcPacket PACKET = RpcPacket.newBuilder()
                                              .setChannelId(123)
                                              .setCallId(Endpoint.FIRST_CALL_ID)
                                              .setServiceId(RPC.service().id())
                                              .setMethodId(RPC.method().id())
                                              .build();

  private static final Packets PACKETS = new Packets(CallIdMode.ENABLED);

  @Test
  public void request() throws Exception {
    RpcPacket payload = RpcPacket.newBuilder().setType(PacketType.SERVER_STREAM).build();
    RpcPacket packet = RpcPacket.parseFrom(
        PACKETS.request(RPC, payload), ExtensionRegistryLite.getEmptyRegistry());
    assertThat(packet).isEqualTo(
        packet().setType(PacketType.REQUEST).setPayload(payload.toByteString()).build());
  }

  @Test
  public void cancel() throws Exception {
    RpcPacket packet =
        RpcPacket.parseFrom(PACKETS.cancel(RPC), ExtensionRegistryLite.getEmptyRegistry());
    assertThat(packet).isEqualTo(
        packet().setType(PacketType.CLIENT_ERROR).setStatus(Status.CANCELLED.code()).build());
  }

  @Test
  public void error() throws Exception {
    RpcPacket packet = RpcPacket.parseFrom(
        PACKETS.error(PACKET, Status.ALREADY_EXISTS), ExtensionRegistryLite.getEmptyRegistry());
    assertThat(packet).isEqualTo(
        packet().setType(PacketType.CLIENT_ERROR).setStatus(Status.ALREADY_EXISTS.code()).build());
  }

  private static RpcPacket.Builder packet() {
    return RpcPacket.newBuilder()
        .setChannelId(123)
        .setCallId(Endpoint.FIRST_CALL_ID)
        .setServiceId(Ids.calculate("Greetings"))
        .setMethodId(Ids.calculate("Hello"));
  }
}
