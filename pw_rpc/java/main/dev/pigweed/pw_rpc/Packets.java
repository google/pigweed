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
import dev.pigweed.pw_rpc.internal.Packet.PacketType;
import dev.pigweed.pw_rpc.internal.Packet.RpcPacket;

/** Encodes pw_rpc packets of various types. */
/* package */ class Packets {
  private final CallIdMode callIdMode;

  public Packets(CallIdMode callIdMode) {
    this.callIdMode = callIdMode;
  }

  public boolean callIdsEnabled() {
    return callIdMode == CallIdMode.ENABLED;
  }

  public byte[] request(PendingRpc rpc, MessageLite payload) {
    RpcPacket.Builder builder = newBuilder(rpc.callId())
                                    .setType(PacketType.REQUEST)
                                    .setChannelId(rpc.channel().id())
                                    .setServiceId(rpc.service().id())
                                    .setMethodId(rpc.method().id());
    if (payload != null) {
      builder.setPayload(payload.toByteString());
    }
    return builder.build().toByteArray();
  }

  public byte[] cancel(PendingRpc rpc) {
    return newBuilder(rpc.callId())
        .setType(PacketType.CLIENT_ERROR)
        .setChannelId(rpc.channel().id())
        .setServiceId(rpc.service().id())
        .setMethodId(rpc.method().id())
        .setStatus(Status.CANCELLED.code())
        .build()
        .toByteArray();
  }

  public byte[] error(RpcPacket packet, Status status) {
    return newBuilder(packet.getCallId())
        .setType(PacketType.CLIENT_ERROR)
        .setChannelId(packet.getChannelId())
        .setServiceId(packet.getServiceId())
        .setMethodId(packet.getMethodId())
        .setStatus(status.code())
        .build()
        .toByteArray();
  }

  public byte[] clientStream(PendingRpc rpc, MessageLite payload) {
    return newBuilder(rpc.callId())
        .setType(PacketType.CLIENT_STREAM)
        .setChannelId(rpc.channel().id())
        .setServiceId(rpc.service().id())
        .setMethodId(rpc.method().id())
        .setPayload(payload.toByteString())
        .build()
        .toByteArray();
  }

  public byte[] clientStreamEnd(PendingRpc rpc) {
    return newBuilder(rpc.callId())
        .setType(PacketType.CLIENT_REQUEST_COMPLETION)
        .setChannelId(rpc.channel().id())
        .setServiceId(rpc.service().id())
        .setMethodId(rpc.method().id())
        .build()
        .toByteArray();
  }

  public byte[] serverError(
      int channelId, String service, String method, int callId, Status error) {
    return newBuilder(callId)
        .setType(PacketType.SERVER_ERROR)
        .setChannelId(channelId)
        .setServiceId(Ids.calculate(service))
        .setMethodId(Ids.calculate(method))
        .setStatus(error.code())
        .build()
        .toByteArray();
  }

  public RpcPacket.Builder startServerStream(
      int channelId, String service, String method, int callId) {
    return newBuilder(callId)
        .setType(PacketType.SERVER_STREAM)
        .setChannelId(channelId)
        .setServiceId(Ids.calculate(service))
        .setMethodId(Ids.calculate(method));
  }

  private RpcPacket.Builder newBuilder(int callId) {
    RpcPacket.Builder builder = RpcPacket.newBuilder();
    if (callIdMode == CallIdMode.ENABLED) {
      builder.setCallId(callId);
    }
    return builder;
  }
}
