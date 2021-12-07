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

/**
 * Represents a method ready to be invoked on a particular RPC channel.
 *
 * <p>The Client has the concrete MethodClient as a type parameter. This allows implementations to
 * fully define the interface and semantics for RPC calls.
 */
public abstract class MethodClient {
  private final RpcManager rpcs;
  private final PendingRpc rpc;

  protected MethodClient(RpcManager rpcs, PendingRpc rpc) {
    this.rpcs = rpcs;
    this.rpc = rpc;
  }

  public final Method method() {
    return rpc.method();
  }

  /** Gives implementations access to the RpcManager shared with the Client. */
  protected final RpcManager rpcs() {
    return rpcs;
  }

  /** Gives implementations access to the PendingRpc this MethodClient represents. */
  protected final PendingRpc rpc() {
    return rpc;
  }
}
