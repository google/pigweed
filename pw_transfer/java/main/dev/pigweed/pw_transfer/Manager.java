// Copyright 2022 The Pigweed Authors
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

package dev.pigweed.pw_transfer;

import dev.pigweed.pw_rpc.MethodClient;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

/** @deprecated Manager was renamed to TransferClient; use TransferClient instead. */
@Deprecated
public class Manager extends TransferClient {
  /**
   * Creates a new transfer client for sending and receiving data with pw_transfer.
   *
   * @param readMethod Method client for the pw.transfer.Transfer.Read method.
   * @param writeMethod Method client for the pw.transfer.Transfer.Write method.
   * @param workDispatcher Deprecated, not used.
   * @param transferTimeoutMillis How long to wait for communication from the server. If the server
   * delays longer than this, retry up to maxRetries times.
   * @param initialTransferTimeoutMillis How long to wait for the initial communication from the
   * server. If the server delays longer than this, retry up to maxRetries times.
   * @param maxRetries How many times to retry if a communication times out.
   * @param shouldAbortCallback BooleanSupplier that returns true if a transfer should be aborted.
   */
  public Manager(MethodClient readMethod,
      MethodClient writeMethod,
      Consumer<Runnable> workDispatcher,
      int transferTimeoutMillis,
      int initialTransferTimeoutMillis,
      int maxRetries,
      BooleanSupplier shouldAbortCallback) {
    super(readMethod,
        writeMethod,
        transferTimeoutMillis,
        initialTransferTimeoutMillis,
        maxRetries,
        shouldAbortCallback);
  }
}
