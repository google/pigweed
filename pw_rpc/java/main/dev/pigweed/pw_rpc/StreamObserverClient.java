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

import com.google.common.flogger.FluentLogger;
import com.google.protobuf.MessageLite;
import dev.pigweed.pw_rpc.StreamObserverCall.Dispatcher;
import java.util.List;
import java.util.function.Function;

/** Creates a pw.rpc.Client that uses the StreamObserver class for RPCs. */
public final class StreamObserverClient {
  private static final FluentLogger logger = FluentLogger.forEnclosingClass();

  /**
   * Creates a new pw_rpc client.
   *
   * @param channels the set of channels for the client to send requests over
   * @param services the services to support on this client
   * @param defaultObserverFactory function that creates a default observer for each RPC
   * @return the new pw.rpc.Client
   */
  public static Client<StreamObserverMethodClient> create(List<Channel> channels,
      List<Service> services,
      Function<PendingRpc, StreamObserver<MessageLite>> defaultObserverFactory) {
    return new Client<>(channels,
        services,
        new Dispatcher(),
        (rpcs,
            rpc) -> new StreamObserverMethodClient(rpcs, rpc, defaultObserverFactory.apply(rpc)));
  }

  /** Creates a new pw_rpc client that logs responses when no observer is provided to calls. */
  public static Client<StreamObserverMethodClient> create(
      List<Channel> channels, List<Service> services) {
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

  private StreamObserverClient() {}
}
