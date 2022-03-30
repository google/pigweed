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

import static java.util.Arrays.stream;

import com.google.common.collect.ImmutableList;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.MessageLite;
import dev.pigweed.pw_rpc.internal.Packet.PacketType;
import dev.pigweed.pw_rpc.internal.Packet.RpcPacket;
import java.util.ArrayList;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
import javax.annotation.Nullable;

/**
 * Wraps a StreamObserverMethodClient for use in tests. Provides methods for simulating the server
 * interactions with the client.
 */
public class TestClient {
  private static final int CHANNEL_ID = 1;

  private final Client client;

  private final List<RpcPacket> sentPackets = new ArrayList<>();
  private final List<RpcPacket> enqueuedPackets = new ArrayList<>();
  private int receiveEnqueuedPacketsAfter = 1;
  final Map<PacketType, Integer> sentPayloadIndices = new EnumMap<>(PacketType.class);

  @Nullable ChannelOutputException channelOutputException = null;

  public TestClient(List<Service> services) {
    Channel.Output channelOutput = packet -> {
      if (channelOutputException == null) {
        sentPackets.add(parsePacket(packet));
      } else {
        throw channelOutputException;
      }

      // Process any enqueued packets.
      if (receiveEnqueuedPacketsAfter > 1) {
        receiveEnqueuedPacketsAfter -= 1;
        return;
      }
      if (!enqueuedPackets.isEmpty()) {
        List<RpcPacket> packetsToProcess = new ArrayList<>(enqueuedPackets);
        enqueuedPackets.clear();
        packetsToProcess.forEach(this::processPacket);
      }
    };
    client = Client.create(ImmutableList.of(new Channel(CHANNEL_ID, channelOutput)), services);
  }

  public Client client() {
    return client;
  }

  /**
   * Sets the exception to throw the next time a packet is sent. Set to null to accept the packet
   * without errors.
   *
   * <p>When Channel.Output throws an exception, TestClient does not store those outgoing packets.
   */
  public void setChannelOutputException(@Nullable ChannelOutputException exception) {
    this.channelOutputException = exception;
  }

  /** Returns all payloads that were sent since the last latestClientStreams call. */
  public <T extends MessageLite> List<T> lastClientStreams(Class<T> payloadType) {
    return sentPayloads(payloadType, PacketType.CLIENT_STREAM);
  }

  /** Simulates receiving SERVER_STREAM packets from the server. */
  public void receiveServerStream(String service, String method, MessageLite... payloads) {
    RpcPacket base = startPacket(service, method, PacketType.SERVER_STREAM).build();
    for (MessageLite payload : payloads) {
      processPacket(RpcPacket.newBuilder(base).setPayload(payload.toByteString()));
    }
  }

  public void receiveServerStream(String service, String method, MessageLite.Builder... builders) {
    receiveServerStream(service,
        method,
        stream(builders).map(MessageLite.Builder::build).toArray(MessageLite[] ::new));
  }

  /**
   * Enqueues a SERVER_STREAM packet so that the client receives it after a packet is sent.
   *
   * @param afterPackets Wait until this many packets have been sent before the client receives
   *     these stream packets. The minimum value (and the default) is 1.
   */
  public void enqueueServerStream(
      String service, String method, int afterPackets, MessageLite... payloads) {
    if (afterPackets < 1) {
      throw new IllegalArgumentException("afterPackets must be at least 1");
    }
    if (afterPackets != 1 && receiveEnqueuedPacketsAfter != 1) {
      throw new AssertionError(
          "May only set afterPackets once before enqueued packets are processed");
    }
    receiveEnqueuedPacketsAfter = afterPackets;

    RpcPacket base = startPacket(service, method, PacketType.SERVER_STREAM).build();
    for (MessageLite payload : payloads) {
      enqueuedPackets.add(RpcPacket.newBuilder(base).setPayload(payload.toByteString()).build());
    }
  }

  public void enqueueServerStream(String service, String method, MessageLite... payloads) {
    enqueueServerStream(service, method, 1, payloads);
  }

  public void enqueueServerStream(
      String service, String method, int afterPackets, MessageLite.Builder... builders) {
    enqueueServerStream(service,
        method,
        afterPackets,
        stream(builders).map(MessageLite.Builder::build).toArray(MessageLite[] ::new));
  }

  /** Simulates receiving a SERVER_ERROR packet from the server. */
  public void receiveServerError(String service, String method, Status error) {
    processPacket(startPacket(service, method, PacketType.SERVER_ERROR).setStatus(error.code()));
  }

  /** Parses sent payloads for the given type of packet. */
  private <T extends MessageLite> List<T> sentPayloads(Class<T> payloadType, PacketType type) {
    int sentPayloadIndex = sentPayloadIndices.getOrDefault(type, 0);

    // Filter only the specified packets.
    List<T> newPayloads = sentPackets.stream()
                              .filter(packet -> packet.getType().equals(type))
                              .skip(sentPayloadIndex)
                              .map(p -> parseRequestPayload(payloadType, p))
                              .collect(Collectors.toList());

    // Store the index of the last read payload. Could drop the viewed packets instead to reduce
    // memory usage, but that probably won't matter in practice.
    sentPayloadIndices.put(type, sentPayloadIndex + newPayloads.size());

    return newPayloads;
  }

  private void processPacket(RpcPacket packet) {
    if (!client.processPacket(packet.toByteArray())) {
      throw new AssertionError("TestClient failed to process a packet!");
    }
  }

  private void processPacket(RpcPacket.Builder packet) {
    processPacket(packet.build());
  }

  private static RpcPacket.Builder startPacket(String service, String method, PacketType type) {
    return RpcPacket.newBuilder()
        .setType(type)
        .setChannelId(CHANNEL_ID)
        .setServiceId(Ids.calculate(service))
        .setMethodId(Ids.calculate(method));
  }

  private static RpcPacket parsePacket(byte[] packet) {
    try {
      return RpcPacket.parseFrom(packet);
    } catch (InvalidProtocolBufferException e) {
      throw new AssertionError("Decoding sent packet failed", e);
    }
  }

  private <T extends MessageLite> T parseRequestPayload(Class<T> payloadType, RpcPacket packet) {
    try {
      return payloadType.cast(Method.decodeProtobuf(
          client.method(CHANNEL_ID, packet.getServiceId(), packet.getMethodId()).method().request(),
          packet.getPayload()));
    } catch (InvalidProtocolBufferException e) {
      throw new AssertionError("Decoding sent packet payload failed", e);
    }
  }
}
