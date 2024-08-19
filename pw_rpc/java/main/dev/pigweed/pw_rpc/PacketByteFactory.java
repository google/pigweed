// Copyright 2024 The Pigweed Authors
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

/** Util class for creating request / response packets for testing. */
public final class PacketByteFactory {
  private PacketByteFactory() {}

  /**
   * Creates encoded request packet bytes.
   *
   * <p>There is no parameter for {@link Status} since request packets always have a status of
   * {@link Status#OK}.
   *
   * @param channelId the channel ID for the packet
   * @param serviceName the service name for the packet
   * @param methodName the method name for the packet
   * @param payload the payload for the packet
   * @return the encoded packet bytes
   */
  public static byte[] requestBytes(
      int channelId, String serviceName, String methodName, MessageLite payload) {
    return packetBytes(channelId, Status.OK, serviceName, methodName, payload, PacketType.REQUEST);
  }

  /**
   * Creates encoded response packet bytes.
   *
   * @param channelId the channel ID for the packet
   * @param status the status for the packet
   * @param serviceName the service name for the packet
   * @param methodName the method name for the packet
   * @param payload the payload for the packet
   * @return the encoded packet bytes
   */
  public static byte[] responseBytes(
      int channelId, Status status, String serviceName, String methodName, MessageLite payload) {
    return packetBytes(channelId, status, serviceName, methodName, payload, PacketType.RESPONSE);
  }

  private static byte[] packetBytes(int channelId,
      Status status,
      String serviceName,
      String methodName,
      MessageLite payload,
      PacketType type) {
    RpcPacket.Builder builder = RpcPacket.newBuilder()
                                    .setStatus(status.code())
                                    .setType(type)
                                    .setChannelId(channelId)
                                    .setServiceId(Ids.calculate(serviceName))
                                    .setMethodId(Ids.calculate(methodName));

    if (payload != null) {
      builder.setPayload(payload.toByteString());
    }
    return builder.build().toByteArray();
  }
}
