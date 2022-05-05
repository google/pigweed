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

import com.google.common.util.concurrent.ListenableFuture;
import dev.pigweed.pw_log.Logger;
import dev.pigweed.pw_rpc.MethodClient;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

/**
 * Manages ongoing pw_transfer data transfers.
 *
 * <p>Use TransferClient to send data to and receive data from a pw_transfer service running on a
 * pw_rpc server.
 */
public class TransferClient {
  public static final TransferParameters DEFAULT_READ_TRANSFER_PARAMETERS =
      TransferParameters.create(8192, 1024, 0);

  private final int transferTimeoutMillis;
  private final int initialTransferTimeoutMillis;
  private final int maxRetries;
  private final BooleanSupplier shouldAbortCallback;

  private final TransferEventHandler transferEventHandler;
  private final Thread transferEventHandlerThread;

  /**
   * Creates a new transfer client for sending and receiving data with pw_transfer.
   *
   * @param readMethod Method client for the pw.transfer.Transfer.Read method.
   * @param writeMethod Method client for the pw.transfer.Transfer.Write method.
   * @param transferTimeoutMillis How long to wait for communication from the server. If the server
   *     delays longer than this, retry up to maxRetries times.
   * @param initialTransferTimeoutMillis How long to wait for the initial communication from the
   *     server. If the server delays longer than this, retry up to maxRetries times.
   * @param maxRetries How many times to retry if a communication times out.
   * @param shouldAbortCallback BooleanSupplier that returns true if a transfer should be aborted.
   */
  public TransferClient(MethodClient readMethod,
      MethodClient writeMethod,
      int transferTimeoutMillis,
      int initialTransferTimeoutMillis,
      int maxRetries,
      BooleanSupplier shouldAbortCallback) {
    this.transferTimeoutMillis = transferTimeoutMillis;
    this.initialTransferTimeoutMillis = initialTransferTimeoutMillis;
    this.maxRetries = maxRetries;
    this.shouldAbortCallback = shouldAbortCallback;

    transferEventHandler = new TransferEventHandler(readMethod, writeMethod);
    transferEventHandlerThread = new Thread(transferEventHandler::run);
    transferEventHandlerThread.start();
  }

  /** Writes the provided data to the given transfer resource. */
  public ListenableFuture<Void> write(int resourceId, byte[] data) {
    return write(resourceId, data, transferProgress -> {});
  }

  /**
   * Writes data to the specified transfer resource, calling the progress
   * callback as data is sent.
   *
   * @param resourceId The ID of the resource to which to write
   * @param data the data to write
   * @param progressCallback called each time a packet is sent
   */
  public ListenableFuture<Void> write(
      int resourceId, byte[] data, Consumer<TransferProgress> progressCallback) {
    return transferEventHandler.startWriteTransferAsClient(resourceId,
        transferTimeoutMillis,
        initialTransferTimeoutMillis,
        maxRetries,
        data,
        progressCallback,
        shouldAbortCallback);
  }

  /** Reads the data from the given transfer resource ID. */
  public ListenableFuture<byte[]> read(int resourceId) {
    return read(resourceId, DEFAULT_READ_TRANSFER_PARAMETERS, progressCallback -> {});
  }

  /** Reads the data for a transfer resource, calling the progress callback as data is received. */
  public ListenableFuture<byte[]> read(
      int resourceId, Consumer<TransferProgress> progressCallback) {
    return read(resourceId, DEFAULT_READ_TRANSFER_PARAMETERS, progressCallback);
  }

  /** Reads the data for a transfer resource, using the specified transfer parameters. */
  public ListenableFuture<byte[]> read(int resourceId, TransferParameters parameters) {
    return read(resourceId, parameters, (progressCallback) -> {});
  }

  /**
   * Reads the data for a transfer resource, using the specified parameters and progress callback.
   */
  public ListenableFuture<byte[]> read(
      int resourceId, TransferParameters parameters, Consumer<TransferProgress> progressCallback) {
    return transferEventHandler.startReadTransferAsClient(resourceId,
        transferTimeoutMillis,
        initialTransferTimeoutMillis,
        maxRetries,
        parameters,
        progressCallback,
        shouldAbortCallback);
  }

  /** Stops the background thread and waits until it terminates. */
  public void close() throws InterruptedException {
    transferEventHandler.stop();
    transferEventHandlerThread.join();
  }

  void waitUntilEventsAreProcessedForTest() {
    transferEventHandler.waitUntilEventsAreProcessedForTest();
  }
}
