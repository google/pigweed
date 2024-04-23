// Copyright 2023 The Pigweed Authors
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

import static java.lang.Math.min;

import com.google.protobuf.ByteString;
import dev.pigweed.pw_log.Logger;
import dev.pigweed.pw_rpc.Status;
import dev.pigweed.pw_transfer.TransferEventHandler.TransferInterface;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

class WriteTransfer extends Transfer<Void> {
  private static final Logger logger = Logger.forClass(WriteTransfer.class);

  // Short chunk delays often turn into much longer delays. Ignore delays <10ms to avoid impacting
  // performance.
  private static final int MIN_CHUNK_DELAY_TO_SLEEP_MICROS = 10000;

  private int maxChunkSizeBytes = 0;
  private int minChunkDelayMicros = 0;
  private int initialOffset;
  private long totalDroppedBytes;

  private final byte[] data;

  protected WriteTransfer(int resourceId,
      int sessionId,
      ProtocolVersion desiredProtocolVersion,
      TransferInterface transferManager,
      TransferTimeoutSettings timeoutSettings,
      byte[] data,
      Consumer<TransferProgress> progressCallback,
      BooleanSupplier shouldAbortCallback,
      int initialOffset) {
    super(resourceId,
        sessionId,
        desiredProtocolVersion,
        transferManager,
        timeoutSettings,
        progressCallback,
        shouldAbortCallback,
        initialOffset);
    this.data = data;
    this.initialOffset = initialOffset;
  }

  @Override
  void prepareInitialChunk(VersionedChunk.Builder chunk) {
    chunk.setInitialOffset(getOffset());
    chunk.setRemainingBytes(data.length);
  }

  @Override
  State getWaitingForDataState() {
    return new WaitingForTransferParameters();
  }

  private class WaitingForTransferParameters extends ActiveState {
    @Override
    public void handleDataChunk(VersionedChunk chunk) throws TransferAbortedException {
      updateTransferParameters(chunk);
    }
  }

  /** Transmitting a transfer window. */
  private class Transmitting extends ActiveState {
    private final int windowStartOffset;
    private final int windowEndOffset;

    Transmitting(int windowStartOffset, int windowEndOffset) {
      this.windowStartOffset = windowStartOffset;
      this.windowEndOffset = windowEndOffset;
    }

    @Override
    public void handleDataChunk(VersionedChunk chunk) throws TransferAbortedException {
      updateTransferParameters(chunk);
    }

    @Override
    public void handleTimeout() throws TransferAbortedException {
      ByteString chunkData = ByteString.copyFrom(
          data, getOffset() - initialOffset, min(windowEndOffset - getOffset(), maxChunkSizeBytes));

      if (VERBOSE_LOGGING) {
        logger.atFinest().log("%s sending bytes %d-%d (%d B chunk, max size %d B)",
            WriteTransfer.this,
            getOffset(),
            getOffset() + chunkData.size() - 1,
            chunkData.size(),
            maxChunkSizeBytes);
      }

      sendChunk(buildDataChunk(chunkData));

      setOffset(getOffset() + chunkData.size());
      updateProgress(getOffset(), windowStartOffset, data.length + initialOffset);

      if (getOffset() < windowEndOffset) {
        setTimeoutMicros(minChunkDelayMicros);
        return; // Keep transmitting packets
      }
      setNextChunkTimeout();
      changeState(new WaitingForTransferParameters());
    }
  }

  @Override
  VersionedChunk getChunkForRetry() {
    // The service should resend transfer parameters if there was a timeout. In case the service
    // doesn't support timeouts and to avoid unnecessary waits, resend the last chunk. If there
    // were drops, this will trigger a transfer parameters update.
    return getLastChunkSent();
  }

  @Override
  void setFutureResult() {
    updateProgress(data.length, data.length, data.length);
    set(null);
  }

  private void updateTransferParameters(VersionedChunk chunk) throws TransferAbortedException {
    logger.atFiner().log("%s received new chunk %s", this, chunk);

    if (chunk.offset() > data.length + initialOffset) {
      setStateTerminatingAndSendFinalChunk(Status.OUT_OF_RANGE);
      return;
    }

    int windowEndOffset = min(chunk.windowEndOffset(), data.length + initialOffset);
    if (chunk.requestsTransmissionFromOffset()) {
      long droppedBytes = getOffset() - chunk.offset();
      if (droppedBytes > 0) {
        totalDroppedBytes += droppedBytes;
        logger.atFine().log("%s retransmitting %d B (%d retransmitted of %d sent)",
            this,
            droppedBytes,
            totalDroppedBytes,
            getOffset());
      }
      setOffset(chunk.offset());
    } else if (windowEndOffset <= getOffset()) {
      logger.atFiner().log("%s ignoring old rolling window packet", this);
      setNextChunkTimeout();
      return; // Received an old rolling window packet, ignore it.
    }

    // Update transfer parameters if they're set.
    chunk.maxChunkSizeBytes().ifPresent(size -> maxChunkSizeBytes = size);
    chunk.minDelayMicroseconds().ifPresent(delay -> {
      if (delay > MIN_CHUNK_DELAY_TO_SLEEP_MICROS) {
        minChunkDelayMicros = delay;
      }
    });

    if (maxChunkSizeBytes == 0) {
      if (windowEndOffset == getOffset()) {
        logger.atWarning().log("%s server requested 0 bytes; aborting", this);
        setStateTerminatingAndSendFinalChunk(Status.INVALID_ARGUMENT);
        return;
      }
      // Default to sending the entire window if the max chunk size is not specified (or is 0).
      maxChunkSizeBytes = windowEndOffset - getOffset();
    }

    // Enter the transmitting state and immediately send the first packet
    changeState(new Transmitting(chunk.offset(), windowEndOffset)).handleTimeout();
  }

  private VersionedChunk buildDataChunk(ByteString chunkData) {
    VersionedChunk.Builder chunk =
        newChunk(Chunk.Type.DATA).setOffset(getOffset()).setData(chunkData);

    // If this is the last data chunk, setRemainingBytes to 0.
    if (getOffset() + chunkData.size() == data.length + initialOffset) {
      logger.atFiner().log("%s sending final chunk with %d B", this, chunkData.size());
      chunk.setRemainingBytes(0);
    }
    return chunk.build();
  }
}
