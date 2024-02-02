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

  private final TransferTimeoutSettings settings;
  private final BooleanSupplier shouldAbortCallback;

  private final TransferEventHandler transferEventHandler;
  private final Thread transferEventHandlerThread;

  private ProtocolVersion desiredProtocolVersion = ProtocolVersion.VERSION_TWO;

  /**
   * Creates a new transfer client for sending and receiving data with pw_transfer.
   *
   * @param readMethod Method client for the pw.transfer.Transfer.Read method.
   * @param writeMethod Method client for the pw.transfer.Transfer.Write method.
   * @param settings Settings for timeouts and retries.
   */
  public TransferClient(
      MethodClient readMethod, MethodClient writeMethod, TransferTimeoutSettings settings) {
    this(readMethod, writeMethod, settings, () -> false, TransferEventHandler::run);
  }

  /**
   * Creates a new transfer client with a callback that can be used to terminate transfers.
   *
   * @param shouldAbortCallback BooleanSupplier that returns true if a transfer should be aborted.
   */
  public TransferClient(MethodClient readMethod,
      MethodClient writeMethod,
      int transferTimeoutMillis,
      int initialTransferTimeoutMillis,
      int maxRetries,
      BooleanSupplier shouldAbortCallback) {
    this(readMethod,
        writeMethod,
        TransferTimeoutSettings.builder()
            .setTimeoutMillis(transferTimeoutMillis)
            .setInitialTimeoutMillis(initialTransferTimeoutMillis)
            .setMaxRetries(maxRetries)
            .build(),
        shouldAbortCallback,
        TransferEventHandler::run);
  }

  /** Constructor exposed to package for test use only. */
  TransferClient(MethodClient readMethod,
      MethodClient writeMethod,
      TransferTimeoutSettings settings,
      BooleanSupplier shouldAbortCallback,
      Consumer<TransferEventHandler> runFunction) {
    this.settings = settings;
    this.shouldAbortCallback = shouldAbortCallback;

    transferEventHandler = new TransferEventHandler(readMethod, writeMethod);
    transferEventHandlerThread = new Thread(() -> runFunction.accept(transferEventHandler));
    transferEventHandlerThread.start();
  }

  /** Writes the provided data to the given transfer resource. */
  public ListenableFuture<Void> write(int resourceId, byte[] data) {
    return write(resourceId, data, transferProgress -> {}, 0);
  }

  /**
   * Writes the provided data to the given transfer resource, calling the progress callback as data
   * is sent
   */
  public ListenableFuture<Void> write(
      int resourceId, byte[] data, Consumer<TransferProgress> progressCallback) {
    return write(resourceId, data, progressCallback, 0);
  }

  /**
   * Writes the provided data to the given transfer resource, starting at the given initial offset
   */
  public ListenableFuture<Void> write(int resourceId, byte[] data, int initialOffset) {
    return write(resourceId, data, transferProgress -> {}, initialOffset);
  }

  /**
   * Writes data to the specified transfer resource, calling the progress
   * callback as data is sent.
   *
   * @param resourceId The ID of the resource to which to write
   * @param data the data to write
   * @param progressCallback called each time a packet is sent
   * @param initialOffset The offset to start writing to on the server side
   */
  public ListenableFuture<Void> write(
      int resourceId, byte[] data, Consumer<TransferProgress> progressCallback, int initialOffset) {
    return transferEventHandler.startWriteTransferAsClient(resourceId,
        desiredProtocolVersion,
        settings,
        data,
        progressCallback,
        shouldAbortCallback,
        initialOffset);
  }

  /** Reads the data from the given transfer resource ID. */
  public ListenableFuture<byte[]> read(int resourceId) {
    return read(resourceId, DEFAULT_READ_TRANSFER_PARAMETERS, progressCallback -> {}, 0);
  }

  /** Reads the data for a transfer resource, calling the progress callback as data is received. */
  public ListenableFuture<byte[]> read(
      int resourceId, Consumer<TransferProgress> progressCallback) {
    return read(resourceId, DEFAULT_READ_TRANSFER_PARAMETERS, progressCallback, 0);
  }

  /** Reads the data for a transfer resource, using the specified transfer parameters. */
  public ListenableFuture<byte[]> read(int resourceId, TransferParameters parameters) {
    return read(resourceId, parameters, (progressCallback) -> {}, 0);
  }

  /**
   * Reads the data for a transfer resource, using the specified parameters and progress callback.
   */
  public ListenableFuture<byte[]> read(
      int resourceId, TransferParameters parameters, Consumer<TransferProgress> progressCallback) {
    return read(resourceId, parameters, progressCallback, 0);
  }

  /** Reads the data from the given transfer resource ID. */
  public ListenableFuture<byte[]> read(int resourceId, int initialOffset) {
    return read(
        resourceId, DEFAULT_READ_TRANSFER_PARAMETERS, progressCallback -> {}, initialOffset);
  }

  /** Reads the data for a transfer resource, calling the progress callback as data is received. */
  public ListenableFuture<byte[]> read(
      int resourceId, Consumer<TransferProgress> progressCallback, int initialOffset) {
    return read(resourceId, DEFAULT_READ_TRANSFER_PARAMETERS, progressCallback, initialOffset);
  }

  /** Reads the data for a transfer resource, using the specified transfer parameters. */
  public ListenableFuture<byte[]> read(
      int resourceId, TransferParameters parameters, int initialOffset) {
    return read(resourceId, parameters, (progressCallback) -> {}, initialOffset);
  }

  /**
   * Reads the data for a transfer resource, using the specified parameters and progress callback.
   *
   * @param resourceId The ID of the resource to which to read
   * @param parameters The transfer parameters to use
   * @param progressCallback called each time a packet is sent
   * @param initialOffset The offset to start reading from on the server side
   */
  public ListenableFuture<byte[]> read(int resourceId,
      TransferParameters parameters,
      Consumer<TransferProgress> progressCallback,
      int initialOffset) {
    return transferEventHandler.startReadTransferAsClient(resourceId,
        desiredProtocolVersion,
        settings,
        parameters,
        progressCallback,
        shouldAbortCallback,
        initialOffset);
  }

  /**
   * Sets the protocol version to request for future transfers
   *
   * Does not affect ongoing transfers. Version cannot be set to UNKNOWN!
   *
   * @throws IllegalArgumentException if the protocol version is UNKNOWN
   */
  public void setProtocolVersion(ProtocolVersion version) {
    if (version == ProtocolVersion.UNKNOWN) {
      throw new IllegalArgumentException("Cannot set protocol version to UNKNOWN!");
    }
    desiredProtocolVersion = version;
  }

  /** Stops the background thread and waits until it terminates. */
  public void close() throws InterruptedException {
    transferEventHandler.stop();
    transferEventHandlerThread.join();
  }

  // Functions for test use only.
  // TODO: b/279808806 - These could be annotated with test-only visibility.

  final void waitUntilEventsAreProcessedForTest() {
    transferEventHandler.waitUntilEventsAreProcessedForTest();
  }

  final int getNextSessionIdForTest() {
    return transferEventHandler.getNextSessionIdForTest();
  }

  final WriteTransfer getWriteTransferForTest(ListenableFuture<?> transferFuture) {
    return (WriteTransfer) transferFuture;
  }

  final ReadTransfer getReadTransferForTest(ListenableFuture<?> transferFuture) {
    return (ReadTransfer) transferFuture;
  }
}
