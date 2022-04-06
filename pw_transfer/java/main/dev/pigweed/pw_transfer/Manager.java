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

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.protobuf.ByteString;
import dev.pigweed.pw_log.Logger;
import dev.pigweed.pw_rpc.Call;
import dev.pigweed.pw_rpc.ChannelOutputException;
import dev.pigweed.pw_rpc.MethodClient;
import dev.pigweed.pw_rpc.RpcError;
import dev.pigweed.pw_rpc.Status;
import dev.pigweed.pw_rpc.StreamObserver;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Timer;
import java.util.function.Consumer;
import javax.annotation.Nullable;

/**
 * Manages ongoing pw_transfer data transfers.
 *
 * <p>Use Manager to send data to and receive data from a pw_transfer service running on a pw_rpc
 * server.
 */
public class Manager {
  private static final Logger logger = Logger.forClass(Manager.class);

  public static final TransferParameters DEFAULT_READ_TRANSFER_PARAMETERS =
      TransferParameters.create(8192, 1024, 0);

  private final MethodClient readMethod;
  private final MethodClient writeMethod;
  private final Consumer<Runnable> workDispatcher;
  private final int transferTimeoutMillis;
  private final int maxRetries;
  private final Timer timer = new Timer("pw_transfer timer", /*isDaemon=*/true);

  private final Map<Integer, Transfer<?>> readTransfers = new HashMap<>();
  private final Map<Integer, Transfer<?>> writeTransfers = new HashMap<>();

  @Nullable private Call.ClientStreaming<Chunk> readStream = null;
  @Nullable private Call.ClientStreaming<Chunk> writeStream = null;

  /**
   * Callback for adjusting the size of a pw_transfer write chunk.
   *
   * <p>If the adjusted size is 0 or negative, the transfer is aborted. If the adjusted size is
   * larger than the chunk, the chunk size remains unchanged.
   */
  public interface ChunkSizeAdjuster {
    int getAdjustedChunkSize(ByteString chunkData, int maxChunkSizeBytes);
  }

  /**
   * Creates a new Manager for sending and receiving data with pw_transfer.
   *
   * @param readMethod Method client for the pw.transfer.Transfer.Read method.
   * @param writeMethod Method client for the pw.transfer.Transfer.Write method.
   * @param workDispatcher Dispatches work when a chunk is received. Pass Runnable::run to handle
   *     the chunk immediately on the RPC thread. Handling write chunks in particular can take a
   *     significant amount of time if the server has requested a large number of chunks or a long
   *     delay.
   * @param transferTimeoutMillis How long to wait for communication from the server. If the server
   *     delays longer than this, retry up to maxRetries times.
   * @param maxRetries How many times to retry if a communication times out.
   */
  public Manager(MethodClient readMethod,
      MethodClient writeMethod,
      Consumer<Runnable> workDispatcher,
      int transferTimeoutMillis,
      int maxRetries) {
    this.readMethod = readMethod;
    this.writeMethod = writeMethod;
    this.workDispatcher = workDispatcher;
    this.transferTimeoutMillis = transferTimeoutMillis;
    this.maxRetries = maxRetries;
  }

  /** Writes the provided data to the given transfer ID. */
  public ListenableFuture<Void> write(int transferId, byte[] data) {
    return write(transferId, data, transferProgress -> {});
  }

  /** Writes the data to the transfer ID; calls the progress callback as data is sent. */
  public ListenableFuture<Void> write(
      int transferId, byte[] data, Consumer<TransferProgress> progressCallback) {
    return write(transferId, data, progressCallback, (chunk, maxSize) -> chunk.size());
  }
  /**
   * Writes the data to the transfer ID.
   *
   * @param transferId The transfer ID to which to write
   * @param data the data to write
   * @param progressCallback called each time a packet is sent
   * @param chunkSizeAdjustment callback to optionally reduce the number of bytes to send
   */
  public synchronized ListenableFuture<Void> write(int transferId,
      byte[] data,
      Consumer<TransferProgress> progressCallback,
      ChunkSizeAdjuster chunkSizeAdjustment) {
    return startTransfer(writeTransfers,
        new WriteTransfer(transferId,
            this::sendWriteChunk,
            writeTransfers::remove,
            timer,
            transferTimeoutMillis,
            maxRetries,
            data,
            progressCallback,
            chunkSizeAdjustment));
  }

  /** Reads the data with the given transfer ID. */
  public ListenableFuture<byte[]> read(int transferId) {
    return read(transferId, DEFAULT_READ_TRANSFER_PARAMETERS, (progressCallback) -> {});
  }

  /** Reads the data for this transfer ID; calls the progress callback when data is received. */
  public ListenableFuture<byte[]> read(
      int transferId, Consumer<TransferProgress> progressCallback) {
    return read(transferId, DEFAULT_READ_TRANSFER_PARAMETERS, progressCallback);
  }

  /** Reads the data for this transfer ID, using the specified transfer parameters. */
  public ListenableFuture<byte[]> read(int transferId, TransferParameters parameters) {
    return read(transferId, parameters, (progressCallback) -> {});
  }

  /** Reads the data for this transfer ID, using the specified parameters and progress callback. */
  public synchronized ListenableFuture<byte[]> read(
      int transferId, TransferParameters parameters, Consumer<TransferProgress> progressCallback) {
    return startTransfer(readTransfers,
        new ReadTransfer(transferId,
            this::sendReadChunk,
            readTransfers::remove,
            timer,
            transferTimeoutMillis,
            maxRetries,
            parameters,
            progressCallback));
  }

  private static <T> ListenableFuture<T> startTransfer(
      Map<Integer, Transfer<?>> transfers, Transfer<T> transfer) {
    if (transfers.containsKey(transfer.getId())) {
      return Futures.immediateFailedFuture(new TransferError("Transfer ID " + transfer.getId()
              + " is already in progress! Only one read/write transfer per ID is supported at a"
              + " time",
          Status.ALREADY_EXISTS));
    }

    // Record the transfer before calling start() in case the server responds immediately.
    transfers.put(transfer.getId(), transfer);
    transfer.start();
    return transfer.getFuture();
  }

  private synchronized void resetWriteStream() {
    writeStream = null;
  }

  private synchronized Call.ClientStreaming<Chunk> getWriteStream() throws ChannelOutputException {
    if (writeStream == null) {
      writeStream = writeMethod.invokeBidirectionalStreaming(
          new ChunkHandler(writeTransfers, workDispatcher) {
            @Override
            void resetStream() {
              resetWriteStream();
            }
          });
    }
    return writeStream;
  }

  private synchronized void resetReadStream() {
    readStream = null;
  }

  private synchronized Call.ClientStreaming<Chunk> getReadStream() throws ChannelOutputException {
    if (readStream == null) {
      readStream =
          readMethod.invokeBidirectionalStreaming(new ChunkHandler(readTransfers, workDispatcher) {
            @Override
            void resetStream() {
              resetReadStream();
            }
          });
    }
    return readStream;
  }

  /** Handles responses on the pw_transfer RPCs. */
  private abstract static class ChunkHandler implements StreamObserver<Chunk> {
    private final Map<Integer, Transfer<?>> transfers;
    private final Consumer<Runnable> workDispatcher;

    private ChunkHandler(Map<Integer, Transfer<?>> transfers, Consumer<Runnable> workDispatcher) {
      this.transfers = transfers;
      this.workDispatcher = workDispatcher;
    }

    @Override
    public final void onNext(Chunk chunk) {
      Transfer<?> transfer = transfers.get(chunk.getTransferId());
      if (transfer != null) {
        logger.atFinest().log("Received chunk for transfer %d", chunk.getTransferId());
        workDispatcher.accept(() -> transfer.handleChunk(chunk));
      } else {
        logger.atWarning().log("Ignoring unrecognized transfer ID %d", chunk.getTransferId());
      }
    }

    @Override
    public final void onCompleted(Status status) {
      onError(Status.INTERNAL); // This RPC should never complete: treat as an internal error.
    }

    @Override
    public final void onError(Status status) {
      resetStream();

      // The transfers remove themselves from the Map during cleanup, iterate over a copied list.
      List<Transfer<?>> activeTransfers = new ArrayList<>(transfers.values());

      // FAILED_PRECONDITION indicates that the stream packet was not recognized as the stream is
      // not open. This could occur if the server resets. Notify pending transfers that this has
      // occurred so they can restart.
      if (status.equals(Status.FAILED_PRECONDITION)) {
        activeTransfers.forEach(Transfer::handleDisconnection);
      } else {
        TransferError error = new TransferError(
            "Transfer stream RPC closed unexpectedly with status " + status, Status.INTERNAL);
        activeTransfers.forEach(t -> t.cleanUp(error));
      }
    }

    abstract void resetStream();
  }

  private void sendWriteChunk(Chunk chunk) throws TransferError {
    try {
      getWriteStream().send(chunk);
    } catch (ChannelOutputException | RpcError e) {
      throw new TransferError("Failed to send chunk for write transfer", e);
    }
  }

  private void sendReadChunk(Chunk chunk) throws TransferError {
    try {
      getReadStream().send(chunk);
    } catch (ChannelOutputException | RpcError e) {
      throw new TransferError("Failed to send chunk for read transfer", e);
    }
  }
}
