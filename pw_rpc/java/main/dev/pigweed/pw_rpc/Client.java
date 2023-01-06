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

import com.google.protobuf.ExtensionRegistryLite;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.MessageLite;
import dev.pigweed.pw_log.Logger;
import dev.pigweed.pw_rpc.internal.Packet.RpcPacket;
import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.BiFunction;
import java.util.function.Function;
import java.util.stream.Collectors;
import javax.annotation.Nullable;

/**
 * A client for a pw_rpc server. Invokes RPCs through a MethodClient and handles RPC responses
 * through the processPacket function.
 */
public class Client {
  private static final Logger logger = Logger.forClass(Client.class);

  private final Map<Integer, Service> services;
  private final Endpoint rpcs;

  private final Map<RpcKey, MethodClient> methodClients = new HashMap<>();

  private final Function<RpcKey, StreamObserver<MessageLite>> defaultObserverFactory;

  /**
   * Creates a new RPC client.
   *
   * @param channels supported channels, which are used to send requests to the server
   * @param services which RPC services this client supports; used to handle encoding and decoding
   */
  private Client(List<Channel> channels,
      List<Service> services,
      Function<RpcKey, StreamObserver<MessageLite>> defaultObserverFactory) {
    this.services = services.stream().collect(Collectors.toMap(Service::id, s -> s));
    this.rpcs = new Endpoint(channels);

    this.defaultObserverFactory = defaultObserverFactory;
  }

  /**
   * Creates a new pw_rpc client.
   *
   * @param channels the set of channels for the client to send requests over
   * @param services the services to support on this client
   * @param defaultObserverFactory function that creates a default observer for each RPC
   * @return the new pw.rpc.Client
   */
  public static Client create(List<Channel> channels,
      List<Service> services,
      Function<RpcKey, StreamObserver<MessageLite>> defaultObserverFactory) {
    return new Client(channels, services, defaultObserverFactory);
  }

  /**
   * Creates a new pw_rpc client that logs responses when no observer is provided to calls.
   */
  public static Client create(List<Channel> channels, List<Service> services) {
    return create(channels, services, (rpc) -> new StreamObserver<MessageLite>() {
      @Override
      public void onNext(MessageLite value) {
        logger.atFine().log("%s received response: %s", rpc, value);
      }

      @Override
      public void onCompleted(Status status) {
        logger.atInfo().log("%s completed with status %s", rpc, status);
      }

      @Override
      public void onError(Status status) {
        logger.atWarning().log("%s terminated with error %s", rpc, status);
      }
    });
  }

  /**
   * Returns a MethodClient with the given name for the provided channelID
   *
   * @param channelId the ID for the channel through which to invoke the RPC
   * @param fullMethodName the method name as "package.Service.Method" or "package.Service/Method"
   */
  public MethodClient method(int channelId, String fullMethodName) {
    for (char delimiter : new char[] {'/', '.'}) {
      int index = fullMethodName.lastIndexOf(delimiter);
      if (index != -1) {
        return method(
            channelId, fullMethodName.substring(0, index), fullMethodName.substring(index + 1));
      }
    }
    throw new IllegalArgumentException("Invalid method name '" + fullMethodName
        + "'; does not match required package.Service/Method format");
  }

  /**
   * Returns a MethodClient on the provided channel using separate arguments for "package.Service"
   * and "Method".
   */
  public MethodClient method(int channelId, String fullServiceName, String methodName) {
    return method(channelId, Ids.calculate(fullServiceName), Ids.calculate(methodName));
  }

  /**
   * Returns a MethodClient instance from a Method instance.
   */
  public MethodClient method(int channelId, Method serviceMethod) {
    return method(channelId, serviceMethod.service().id(), serviceMethod.id());
  }

  /**
   * Returns a MethodClient with the provided service and method IDs.
   */
  synchronized MethodClient method(int channelId, int serviceId, int methodId) {
    Method method = getMethod(serviceId, methodId);

    RpcKey rpc = RpcKey.create(channelId, method);
    if (!methodClients.containsKey(rpc)) {
      methodClients.put(
          rpc, new MethodClient(this, channelId, method, defaultObserverFactory.apply(rpc)));
    }
    return methodClients.get(rpc);
  }

  synchronized<CallT extends AbstractCall<?, ?>> CallT invokeRpc(int channelId,
      Method method,
      BiFunction<Endpoint, PendingRpc, CallT> createCall,
      @Nullable MessageLite request) throws ChannelOutputException {
    return rpcs.invokeRpc(channelId, checkMethod(method), createCall, request);
  }

  synchronized<CallT extends AbstractCall<?, ?>> CallT openRpc(
      int channelId, Method method, BiFunction<Endpoint, PendingRpc, CallT> createCall) {
    return rpcs.openRpc(channelId, checkMethod(method), createCall);
  }

  private Method checkMethod(Method method) {
    // Check that the method on this service object matches the method this client is for.
    // If the service was swapped out, the method could be different.
    Method foundMethod = getMethod(method.service().id(), method.id());
    if (!method.equals(foundMethod)) {
      throw new InvalidRpcServiceMethodException(foundMethod);
    }
    return foundMethod;
  }

  private synchronized Method getMethod(int serviceId, int methodId) {
    // Make sure the service is still present on the class.
    Service service = services.get(serviceId);
    if (service == null) {
      throw new InvalidRpcServiceException(serviceId);
    }

    Method method = service.methods().get(methodId);
    if (method == null) {
      throw new InvalidRpcServiceMethodException(service, methodId);
    }

    return method;
  }

  /**
   * Processes a single RPC packet.
   *
   * @param data a single, binary encoded RPC packet
   * @return true if the packet was decoded and processed by this client; returns false for invalid
   *     packets or packets for a server or unrecognized channel
   */
  public boolean processPacket(byte[] data) {
    return processPacket(ByteBuffer.wrap(data));
  }

  public boolean processPacket(ByteBuffer data) {
    RpcPacket packet;
    try {
      packet = RpcPacket.parseFrom(data, ExtensionRegistryLite.getEmptyRegistry());
    } catch (InvalidProtocolBufferException e) {
      logger.atWarning().withCause(e).log("Failed to decode packet");
      return false;
    }

    if (packet.getChannelId() == 0 || packet.getServiceId() == 0 || packet.getMethodId() == 0) {
      logger.atWarning().log("Received corrupt packet with unset IDs");
      return false;
    }

    // Packets for the server use even type values.
    if (packet.getTypeValue() % 2 == 0) {
      logger.atFine().log("Ignoring %s packet for server", packet.getType().name());
      return false;
    }

    Method method;
    try {
      method = getMethod(packet.getServiceId(), packet.getMethodId());
    } catch (InvalidRpcStateException e) {
      method = null;
    }
    return rpcs.processClientPacket(method, packet);
  }
}
