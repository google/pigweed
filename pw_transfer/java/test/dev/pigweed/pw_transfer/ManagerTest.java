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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.google.common.collect.ImmutableList;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.protobuf.ByteString;
import dev.pigweed.pw_rpc.ChannelOutputException;
import dev.pigweed.pw_rpc.Status;
import dev.pigweed.pw_rpc.TestClient;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.function.Consumer;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

public final class ManagerTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  private static final int CHANNEL_ID = 1;
  private static final String SERVICE = "pw.transfer.Transfer";

  private static final ByteString TEST_DATA_SHORT = ByteString.copyFromUtf8("O_o");
  private static final ByteString TEST_DATA_100B = range(0, 100);

  private static final TransferParameters TRANSFER_PARAMETERS =
      TransferParameters.create(50, 30, 0);
  private static final int MAX_RETRIES = 2;
  private static final int ID = 123;

  private boolean shouldAbortFlag = false;
  private TestClient client;

  Manager manager;

  @Mock private Consumer<TransferProgress> progressCallback;
  @Captor private ArgumentCaptor<TransferProgress> progress;

  @Before
  public void setup() {
    setPlatformTransferEnabled();
    client = new TestClient(ImmutableList.of(TransferService.get()));

    manager = createManager(60000, 60000); // Default to a long timeout that should never trigger.
  }

  @Test
  public void read_singleChunk_successful() throws Exception {
    ListenableFuture<byte[]> future = manager.read(1);
    assertThat(future.isDone()).isFalse();

    receiveReadChunks(newChunk(1).setOffset(0).setData(TEST_DATA_SHORT).setRemainingBytes(0));

    assertThat(future.isDone()).isTrue();
    assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
  }

  @Test
  public void read_failedPreconditionError_retries() throws Exception {
    ListenableFuture<byte[]> future = manager.read(1, TRANSFER_PARAMETERS);

    assertThat(lastChunks())
        .containsExactly(newChunk(1)
                             .setPendingBytes(TRANSFER_PARAMETERS.maxPendingBytes())
                             .setWindowEndOffset(TRANSFER_PARAMETERS.maxPendingBytes())
                             .setMaxChunkSizeBytes(TRANSFER_PARAMETERS.maxChunkSizeBytes())
                             .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                             .build());

    client.receiveServerError(SERVICE, "Read", Status.FAILED_PRECONDITION);

    assertThat(lastChunks())
        .containsExactly(newChunk(1)
                             .setPendingBytes(TRANSFER_PARAMETERS.maxPendingBytes())
                             .setWindowEndOffset(TRANSFER_PARAMETERS.maxPendingBytes())
                             .setMaxChunkSizeBytes(TRANSFER_PARAMETERS.maxChunkSizeBytes())
                             .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                             .build());

    receiveReadChunks(newChunk(1).setOffset(0).setData(TEST_DATA_SHORT).setRemainingBytes(0));

    assertThat(future.isDone()).isTrue();
    assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
  }

  @Test
  public void read_failedPreconditionErrorMaxRetriesTimes_aborts() {
    ListenableFuture<byte[]> future = manager.read(1, TRANSFER_PARAMETERS);

    for (int i = 0; i < MAX_RETRIES; ++i) {
      client.receiveServerError(SERVICE, "Read", Status.FAILED_PRECONDITION);
    }

    Chunk initialChunk = newChunk(1)
                             .setPendingBytes(TRANSFER_PARAMETERS.maxPendingBytes())
                             .setWindowEndOffset(TRANSFER_PARAMETERS.maxPendingBytes())
                             .setMaxChunkSizeBytes(TRANSFER_PARAMETERS.maxChunkSizeBytes())
                             .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                             .build();
    assertThat(lastChunks())
        .containsExactlyElementsIn(Collections.nCopies(1 + MAX_RETRIES, initialChunk));

    client.receiveServerError(SERVICE, "Read", Status.FAILED_PRECONDITION);

    assertThat(lastChunks()).isEmpty();

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void read_singleChunk_ignoresUnknownIdOrWriteChunks() throws Exception {
    ListenableFuture<byte[]> future = manager.read(1);
    assertThat(future.isDone()).isFalse();

    receiveReadChunks(newChunk(2).setOffset(0).setStatus(Status.OK.code()),
        newChunk(0).setOffset(0).setData(TEST_DATA_100B).setRemainingBytes(0),
        newChunk(3).setOffset(0).setData(TEST_DATA_100B).setRemainingBytes(0));
    receiveWriteChunks(newChunk(1).setOffset(0).setStatus(Status.OK.code()),
        newChunk(1).setOffset(0).setData(TEST_DATA_100B).setRemainingBytes(0),
        newChunk(2).setOffset(0).setData(TEST_DATA_100B).setRemainingBytes(0));

    assertThat(future.isDone()).isFalse();

    receiveReadChunks(newChunk(1).setOffset(0).setData(TEST_DATA_SHORT).setRemainingBytes(0));

    assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
  }

  @Test
  public void read_empty() throws Exception {
    ListenableFuture<byte[]> future = manager.read(2);
    lastChunks(); // Discard initial chunk (tested elsewhere)

    receiveReadChunks(newChunk(2).setRemainingBytes(0));

    assertThat(lastChunks()).containsExactly(newChunk(2).setStatus(Status.OK.code()).build());

    assertThat(future.get()).isEqualTo(new byte[] {});
  }

  @Test
  public void read_sendsTransferParametersFirst() {
    ListenableFuture<byte[]> future = manager.read(99, TransferParameters.create(3, 2, 1));

    assertThat(lastChunks())
        .containsExactly(newChunk(99)
                             .setPendingBytes(3)
                             .setWindowEndOffset(3)
                             .setMaxChunkSizeBytes(2)
                             .setMinDelayMicroseconds(1)
                             .setOffset(0)
                             .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                             .build());
    assertThat(future.cancel(true)).isTrue();
  }

  @Test
  public void read_severalChunks() throws Exception {
    ListenableFuture<byte[]> future = manager.read(ID, TRANSFER_PARAMETERS);

    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setPendingBytes(50)
                             .setWindowEndOffset(50)
                             .setMaxChunkSizeBytes(30)
                             .setOffset(0)
                             .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                             .build());

    receiveReadChunks(newChunk(ID).setOffset(0).setData(range(0, 20)).setRemainingBytes(70),
        newChunk(ID).setOffset(20).setData(range(20, 40)));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setOffset(40)
                             .setPendingBytes(50)
                             .setMaxChunkSizeBytes(30)
                             .setWindowEndOffset(90)
                             .setType(Chunk.Type.PARAMETERS_CONTINUE)
                             .build());

    receiveReadChunks(newChunk(ID).setOffset(40).setData(range(40, 70)));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setOffset(70)
                             .setPendingBytes(50)
                             .setMaxChunkSizeBytes(30)
                             .setWindowEndOffset(120)
                             .setType(Chunk.Type.PARAMETERS_CONTINUE)
                             .build());

    receiveReadChunks(newChunk(ID).setOffset(70).setData(range(70, 100)).setRemainingBytes(0));

    assertThat(lastChunks()).containsExactly(newChunk(ID).setStatus(Status.OK.code()).build());

    assertThat(future.get()).isEqualTo(TEST_DATA_100B.toByteArray());
  }

  @Test
  public void read_progressCallbackIsCalled() {
    ListenableFuture<byte[]> future = manager.read(ID, TRANSFER_PARAMETERS, progressCallback);

    receiveReadChunks(newChunk(ID).setOffset(0).setData(range(0, 30)),
        newChunk(ID).setOffset(30).setData(range(30, 50)),
        newChunk(ID).setOffset(50).setData(range(50, 60)).setRemainingBytes(5),
        newChunk(ID).setOffset(60).setData(range(60, 70)),
        newChunk(ID).setOffset(70).setData(range(70, 80)).setRemainingBytes(20),
        newChunk(ID).setOffset(90).setData(range(90, 100)),
        newChunk(ID).setOffset(80).setData(range(80, 100)).setRemainingBytes(0));

    verify(progressCallback, times(6)).accept(progress.capture());
    assertThat(progress.getAllValues())
        .containsExactly(TransferProgress.create(30, 30, TransferProgress.UNKNOWN_TRANSFER_SIZE),
            TransferProgress.create(50, 50, TransferProgress.UNKNOWN_TRANSFER_SIZE),
            TransferProgress.create(60, 60, 65),
            TransferProgress.create(70, 70, TransferProgress.UNKNOWN_TRANSFER_SIZE),
            TransferProgress.create(80, 80, 100),
            TransferProgress.create(100, 100, 100));
    assertThat(future.isDone()).isTrue();
  }

  @Test
  public void read_rewindWhenPacketsSkipped() throws Exception {
    ListenableFuture<byte[]> future = manager.read(ID, TRANSFER_PARAMETERS);

    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setPendingBytes(50)
                             .setWindowEndOffset(50)
                             .setMaxChunkSizeBytes(30)
                             .setOffset(0)
                             .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                             .build());

    receiveReadChunks(newChunk(ID).setOffset(50).setData(range(30, 50)));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setPendingBytes(50)
                             .setWindowEndOffset(50)
                             .setMaxChunkSizeBytes(30)
                             .setOffset(0)
                             .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                             .build());

    receiveReadChunks(newChunk(ID).setOffset(0).setData(range(0, 30)),
        newChunk(ID).setOffset(30).setData(range(30, 50)));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setOffset(30)
                             .setPendingBytes(50)
                             .setWindowEndOffset(80)
                             .setMaxChunkSizeBytes(30)
                             .setType(Chunk.Type.PARAMETERS_CONTINUE)
                             .build());

    receiveReadChunks(newChunk(ID).setOffset(80).setData(range(80, 100)).setRemainingBytes(0));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setOffset(50)
                             .setPendingBytes(50)
                             .setWindowEndOffset(100)
                             .setMaxChunkSizeBytes(30)
                             .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                             .build());

    receiveReadChunks(newChunk(ID).setOffset(50).setData(range(50, 80)),
        newChunk(ID).setOffset(80).setData(range(80, 100)).setRemainingBytes(0));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setOffset(80)
                             .setPendingBytes(50)
                             .setWindowEndOffset(130)
                             .setMaxChunkSizeBytes(30)
                             .setType(Chunk.Type.PARAMETERS_CONTINUE)
                             .build(),
            newChunk(ID).setStatus(Status.OK.code()).build());

    assertThat(future.get()).isEqualTo(TEST_DATA_100B.toByteArray());
  }

  @Test
  public void read_multipleWithSameId_sequentially_successful() throws Exception {
    for (int i = 0; i < 3; ++i) {
      ListenableFuture<byte[]> future = manager.read(1);

      receiveReadChunks(newChunk(1).setOffset(0).setData(TEST_DATA_SHORT).setRemainingBytes(0));

      assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
    }
  }

  @Test
  public void read_multipleWithSameId_atSameTime_failsImmediately() {
    ListenableFuture<byte[]> first = manager.read(123);
    ListenableFuture<byte[]> second = manager.read(123);

    assertThat(first.isDone()).isFalse();
    assertThat(second.isDone()).isTrue();

    ExecutionException thrown = assertThrows(ExecutionException.class, second::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ALREADY_EXISTS);
  }

  @Test
  public void read_sendErrorOnFirstPacket_failsImmediately() {
    ChannelOutputException exception = new ChannelOutputException("blah");
    client.setChannelOutputException(exception);
    ListenableFuture<byte[]> future = manager.read(123);

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void read_sendErrorOnLaterPacket_aborts() {
    ListenableFuture<byte[]> future = manager.read(ID, TRANSFER_PARAMETERS);

    receiveReadChunks(newChunk(ID).setOffset(0).setData(range(0, 20)));

    ChannelOutputException exception = new ChannelOutputException("blah");
    client.setChannelOutputException(exception);

    receiveReadChunks(newChunk(ID).setOffset(20).setData(range(20, 50)));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void read_cancelFuture_abortsTransfer() {
    ListenableFuture<byte[]> future = manager.read(ID, TRANSFER_PARAMETERS);

    assertThat(future.cancel(true)).isTrue();

    receiveReadChunks(newChunk(ID).setOffset(30).setData(range(30, 50)));
    assertThat(lastChunks()).contains(newChunk(ID).setStatus(Status.CANCELLED.code()).build());
  }

  @Test
  public void read_protocolError_aborts() {
    ListenableFuture<byte[]> future = manager.read(ID);

    receiveReadChunks(newChunk(ID).setStatus(Status.ALREADY_EXISTS.code()));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);

    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ALREADY_EXISTS);
  }

  @Test
  public void read_rpcError_aborts() {
    ListenableFuture<byte[]> future = manager.read(2);

    client.receiveServerError(SERVICE, "Read", Status.NOT_FOUND);

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void read_timeout() {
    manager = createManager(1, 1); // Create a manager with a very short timeout.
    ListenableFuture<byte[]> future = manager.read(ID, TRANSFER_PARAMETERS);

    // Call future.get() without sending any server-side packets.
    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.DEADLINE_EXCEEDED);

    // read should have retried sending the transfer parameters 2 times, for a total of 3
    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setPendingBytes(50)
                             .setWindowEndOffset(50)
                             .setMaxChunkSizeBytes(30)
                             .setOffset(0)
                             .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                             .build(),
            newChunk(ID)
                .setPendingBytes(50)
                .setWindowEndOffset(50)
                .setMaxChunkSizeBytes(30)
                .setOffset(0)
                .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                .build(),
            newChunk(ID)
                .setPendingBytes(50)
                .setWindowEndOffset(50)
                .setMaxChunkSizeBytes(30)
                .setOffset(0)
                .setType(Chunk.Type.PARAMETERS_RETRANSMIT)
                .build(),
            newChunk(ID).setStatus(Status.DEADLINE_EXCEEDED.code()).build());
  }

  @Test
  public void write_singleChunk() throws Exception {
    ListenableFuture<Void> future = manager.write(2, TEST_DATA_SHORT.toByteArray());
    assertThat(future.isDone()).isFalse();

    receiveWriteChunks(newChunk(2).setOffset(0).setPendingBytes(1024).setMaxChunkSizeBytes(128),
        newChunk(2).setStatus(Status.OK.code()));

    assertThat(future.isDone()).isTrue();
    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_platformTransferDisabled_aborted() {
    ListenableFuture<Void> future = manager.write(2, TEST_DATA_SHORT.toByteArray());
    assertThat(future.isDone()).isFalse();

    setPlatformTransferDisabled();
    receiveWriteChunks(newChunk(2).setOffset(0).setPendingBytes(1024).setMaxChunkSizeBytes(128),
        newChunk(2).setStatus(Status.OK.code()));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ABORTED);
  }

  @Test
  public void write_failedPreconditionError_retries() throws Exception {
    ListenableFuture<Void> future = manager.write(2, TEST_DATA_SHORT.toByteArray());

    assertThat(lastChunks())
        .containsExactly(
            newChunk(2).setResourceId(2).setRemainingBytes(TEST_DATA_SHORT.size()).build());

    client.receiveServerError(SERVICE, "Write", Status.FAILED_PRECONDITION);

    assertThat(lastChunks())
        .containsExactly(
            newChunk(2).setResourceId(2).setRemainingBytes(TEST_DATA_SHORT.size()).build());

    receiveWriteChunks(newChunk(2).setOffset(0).setPendingBytes(1024).setMaxChunkSizeBytes(128),
        newChunk(2).setStatus(Status.OK.code()));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_failedPreconditionErrorMaxRetriesTimes_aborts() {
    ListenableFuture<Void> future = manager.write(2, TEST_DATA_SHORT.toByteArray());

    for (int i = 0; i < MAX_RETRIES; ++i) {
      client.receiveServerError(SERVICE, "Write", Status.FAILED_PRECONDITION);
    }

    Chunk initialChunk =
        newChunk(2).setResourceId(2).setRemainingBytes(TEST_DATA_SHORT.size()).build();
    assertThat(lastChunks())
        .containsExactlyElementsIn(Collections.nCopies(1 + MAX_RETRIES, initialChunk));

    client.receiveServerError(SERVICE, "Write", Status.FAILED_PRECONDITION);

    assertThat(lastChunks()).isEmpty();

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void write_empty() throws Exception {
    ListenableFuture<Void> future = manager.write(2, new byte[] {});
    assertThat(future.isDone()).isFalse();

    receiveWriteChunks(newChunk(2).setStatus(Status.OK.code()));

    assertThat(lastChunks())
        .containsExactly(newChunk(2).setResourceId(2).setRemainingBytes(0).build());

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_severalChunks() throws Exception {
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_100B.toByteArray());

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setResourceId(ID).setRemainingBytes(TEST_DATA_100B.size()).build());

    receiveWriteChunks(newChunk(ID)
                           .setOffset(0)
                           .setPendingBytes(50)
                           .setMaxChunkSizeBytes(30)
                           .setMinDelayMicroseconds(1));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID).setOffset(0).setData(range(0, 30)).build(),
            newChunk(ID).setOffset(30).setData(range(30, 50)).build());

    receiveWriteChunks(newChunk(ID).setOffset(50).setPendingBytes(40).setMaxChunkSizeBytes(25));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID).setOffset(50).setData(range(50, 75)).build(),
            newChunk(ID).setOffset(75).setData(range(75, 90)).build());

    receiveWriteChunks(newChunk(ID).setOffset(90).setPendingBytes(50));

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setOffset(90).setData(range(90, 100)).setRemainingBytes(0).build());

    assertThat(future.isDone()).isFalse();

    receiveWriteChunks(newChunk(ID).setStatus(Status.OK.code()));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_adjustChunkSize() throws Exception {
    ListenableFuture<Void> future = // Always request 30-byte chunks
        manager.write(ID, TEST_DATA_100B.toByteArray(), progress -> {}, (chunk, maxSize) -> 30);

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setResourceId(ID).setRemainingBytes(TEST_DATA_100B.size()).build());

    receiveWriteChunks(newChunk(ID).setOffset(0).setPendingBytes(1024).setMaxChunkSizeBytes(100));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID).setOffset(0).setData(range(0, 30)).build(),
            newChunk(ID).setOffset(30).setData(range(30, 60)).build(),
            newChunk(ID).setOffset(60).setData(range(60, 90)).build(),
            newChunk(ID).setOffset(90).setData(range(90, 100)).setRemainingBytes(0).build());

    receiveWriteChunks(newChunk(ID).setStatus(Status.OK.code()));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_adjustChunkSize_zeroLengthAdjustment_abortsTransfer() {
    ListenableFuture<Void> future = // Always request 0-byte chunks, which is invalid.
        manager.write(ID, TEST_DATA_100B.toByteArray(), progress -> {}, (chunk, maxSize) -> 0);

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setResourceId(ID).setRemainingBytes(TEST_DATA_100B.size()).build());

    receiveWriteChunks(newChunk(ID).setOffset(0).setPendingBytes(1024).setMaxChunkSizeBytes(100));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INVALID_ARGUMENT);
  }

  @Test
  public void write_adjustChunkSize_negativeAdjustment_abortsTransfer() {
    ListenableFuture<Void> future = // Always request negative chunks, which is invalid.
        manager.write(ID, TEST_DATA_100B.toByteArray(), progress -> {}, (chunk, maxSize) -> - 1);

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setResourceId(ID).setRemainingBytes(TEST_DATA_100B.size()).build());

    receiveWriteChunks(newChunk(ID).setOffset(0).setPendingBytes(1024).setMaxChunkSizeBytes(100));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INVALID_ARGUMENT);
  }

  @Test
  public void write_parametersContinue() throws Exception {
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_100B.toByteArray());

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setResourceId(ID).setRemainingBytes(TEST_DATA_100B.size()).build());

    receiveWriteChunks(newChunk(ID)
                           .setOffset(0)
                           .setPendingBytes(50)
                           .setWindowEndOffset(50)
                           .setMaxChunkSizeBytes(30)
                           .setMinDelayMicroseconds(1)
                           .setType(Chunk.Type.PARAMETERS_RETRANSMIT));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID).setOffset(0).setData(range(0, 30)).build(),
            newChunk(ID).setOffset(30).setData(range(30, 50)).build());

    receiveWriteChunks(
        newChunk(ID).setOffset(30).setPendingBytes(50).setWindowEndOffset(80).setType(
            Chunk.Type.PARAMETERS_CONTINUE));

    // Transfer doesn't roll back to offset 30 but instead continues sending up to 80.
    assertThat(lastChunks())
        .containsExactly(newChunk(ID).setOffset(50).setData(range(50, 80)).build());

    receiveWriteChunks(
        newChunk(ID).setOffset(80).setPendingBytes(50).setWindowEndOffset(130).setType(
            Chunk.Type.PARAMETERS_CONTINUE));

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setOffset(80).setData(range(80, 100)).setRemainingBytes(0).build());

    assertThat(future.isDone()).isFalse();

    receiveWriteChunks(newChunk(ID).setStatus(Status.OK.code()));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_continuePacketWithWindowEndBeforeOffsetIsIgnored() throws Exception {
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_100B.toByteArray());

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setResourceId(ID).setRemainingBytes(TEST_DATA_100B.size()).build());

    receiveWriteChunks(newChunk(ID)
                           .setOffset(0)
                           .setPendingBytes(90)
                           .setWindowEndOffset(90)
                           .setMaxChunkSizeBytes(90)
                           .setMinDelayMicroseconds(1)
                           .setType(Chunk.Type.PARAMETERS_RETRANSMIT));

    assertThat(lastChunks())
        .containsExactly(newChunk(ID).setOffset(0).setData(range(0, 90)).build());

    receiveWriteChunks(
        // This stale packet with a window end before the offset should be ignored.
        newChunk(ID).setOffset(25).setPendingBytes(25).setWindowEndOffset(50).setType(
            Chunk.Type.PARAMETERS_CONTINUE),
        // Start from an arbitrary offset before the current, but extend the window to the end.
        newChunk(ID).setOffset(80).setWindowEndOffset(100).setType(Chunk.Type.PARAMETERS_CONTINUE));

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setOffset(90).setData(range(90, 100)).setRemainingBytes(0).build());

    receiveWriteChunks(newChunk(ID).setStatus(Status.OK.code()));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_progressCallbackIsCalled() {
    ListenableFuture<Void> future =
        manager.write(ID, TEST_DATA_100B.toByteArray(), progressCallback);

    receiveWriteChunks(newChunk(ID).setOffset(0).setPendingBytes(90).setMaxChunkSizeBytes(30),
        newChunk(ID).setOffset(50).setPendingBytes(50),
        newChunk(ID).setStatus(Status.OK.code()));

    verify(progressCallback, times(6)).accept(progress.capture());
    assertThat(progress.getAllValues())
        .containsExactly(TransferProgress.create(30, 0, 100),
            TransferProgress.create(60, 0, 100),
            TransferProgress.create(90, 0, 100),
            TransferProgress.create(80, 50, 100),
            TransferProgress.create(100, 50, 100),
            TransferProgress.create(100, 100, 100));
    assertThat(future.isDone()).isTrue();
  }

  @Test
  public void write_asksForFinalOffset_sendsFinalPacket() {
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_100B.toByteArray());

    receiveWriteChunks(newChunk(ID).setOffset(100).setPendingBytes(40).setMaxChunkSizeBytes(25));

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setResourceId(ID).setRemainingBytes(TEST_DATA_100B.size()).build(),
            newChunk(ID).setOffset(100).setRemainingBytes(0).build());
    assertThat(future.isDone()).isFalse();
  }

  @Test
  public void write_multipleWithSameId_sequentially_successful() throws Exception {
    for (int i = 0; i < 3; ++i) {
      ListenableFuture<Void> future = manager.write(ID, TEST_DATA_SHORT.toByteArray());

      receiveWriteChunks(
          newChunk(ID).setOffset(0).setPendingBytes(50), newChunk(ID).setStatus(Status.OK.code()));

      future.get();
    }
  }

  @Test
  public void write_multipleWithSameId_atSameTime_failsImmediately() {
    ListenableFuture<Void> first = manager.write(123, TEST_DATA_SHORT.toByteArray());
    ListenableFuture<Void> second = manager.write(123, TEST_DATA_SHORT.toByteArray());

    assertThat(first.isDone()).isFalse();
    assertThat(second.isDone()).isTrue();

    ExecutionException thrown = assertThrows(ExecutionException.class, second::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ALREADY_EXISTS);
  }

  @Test
  public void write_sendErrorOnFirstPacket_failsImmediately() {
    ChannelOutputException exception = new ChannelOutputException("blah");
    client.setChannelOutputException(exception);
    ListenableFuture<Void> future = manager.write(123, TEST_DATA_SHORT.toByteArray());

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void write_serviceRequestsNoData_aborts() throws Exception {
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_SHORT.toByteArray());

    receiveWriteChunks(newChunk(ID).setOffset(0));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INVALID_ARGUMENT);
  }

  @Test
  public void write_invalidOffset_aborts() {
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_100B.toByteArray());

    receiveWriteChunks(newChunk(ID).setOffset(101).setPendingBytes(40).setMaxChunkSizeBytes(25));

    assertThat(lastChunks())
        .containsExactly(
            newChunk(ID).setResourceId(ID).setRemainingBytes(TEST_DATA_100B.size()).build(),
            newChunk(ID).setStatus(Status.OUT_OF_RANGE.code()).build());

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.OUT_OF_RANGE);
  }

  @Test
  public void write_sendErrorOnLaterPacket_aborts() {
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_SHORT.toByteArray());

    ChannelOutputException exception = new ChannelOutputException("blah");
    client.setChannelOutputException(exception);

    receiveWriteChunks(newChunk(ID).setOffset(0).setPendingBytes(50).setMaxChunkSizeBytes(30));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void write_cancelFuture_abortsTransfer() {
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_100B.toByteArray());

    assertThat(future.cancel(true)).isTrue();

    receiveWriteChunks(newChunk(ID).setOffset(0).setPendingBytes(50).setMaxChunkSizeBytes(50));
    assertThat(lastChunks()).contains(newChunk(ID).setStatus(Status.CANCELLED.code()).build());
  }

  @Test
  public void write_protocolError_aborts() {
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_SHORT.toByteArray());

    receiveWriteChunks(newChunk(ID).setStatus(Status.NOT_FOUND.code()));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);

    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.NOT_FOUND);
  }

  @Test
  public void write_rpcError_aborts() {
    ListenableFuture<Void> future = manager.write(2, TEST_DATA_SHORT.toByteArray());

    client.receiveServerError(SERVICE, "Write", Status.NOT_FOUND);

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void write_timeoutAfterInitialChunk() {
    manager = createManager(1, 1); // Create a manager with a very short timeout.
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_SHORT.toByteArray());

    // Call future.get() without sending any server-side packets.
    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.DEADLINE_EXCEEDED);

    // Client should have resent the last chunk (the initial chunk in this case) for each timeout.
    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setResourceId(ID)
                             .setRemainingBytes(TEST_DATA_SHORT.size())
                             .build(), // initial
            newChunk(ID)
                .setResourceId(ID)
                .setRemainingBytes(TEST_DATA_SHORT.size())
                .build(), // retry
                          // 1
            newChunk(ID)
                .setResourceId(ID)
                .setRemainingBytes(TEST_DATA_SHORT.size())
                .build(), // retry
                          // 2
            newChunk(ID).setStatus(Status.DEADLINE_EXCEEDED.code()).build()); // abort
  }

  @Test
  public void write_timeoutAfterIntermediateChunk() {
    manager = createManager(1, 1); // Create a manager with a very short timeout.

    // Wait for two outgoing packets (Write RPC request and first chunk), then send the parameters.
    enqueueWriteChunks(2, newChunk(ID).setOffset(0).setPendingBytes(90).setMaxChunkSizeBytes(30));
    ListenableFuture<Void> future = manager.write(ID, TEST_DATA_SHORT.toByteArray());

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.DEADLINE_EXCEEDED);

    Chunk data = newChunk(ID).setOffset(0).setData(TEST_DATA_SHORT).setRemainingBytes(0).build();
    assertThat(lastChunks())
        .containsExactly(newChunk(ID)
                             .setResourceId(ID)
                             .setRemainingBytes(TEST_DATA_SHORT.size())
                             .build(), // initial
            data, // data chunk
            data, // retry 1
            data, // retry 2
            newChunk(ID).setStatus(Status.DEADLINE_EXCEEDED.code()).build()); // abort
  }

  private static ByteString range(int startInclusive, int endExclusive) {
    assertThat(startInclusive).isLessThan((int) Byte.MAX_VALUE);
    assertThat(endExclusive).isLessThan((int) Byte.MAX_VALUE);

    byte[] bytes = new byte[endExclusive - startInclusive];
    for (byte i = 0; i < bytes.length; ++i) {
      bytes[i] = (byte) (i + startInclusive);
    }
    return ByteString.copyFrom(bytes);
  }

  private static Chunk.Builder newChunk(int resourceId) {
    return Chunk.newBuilder().setSessionId(resourceId);
  }

  private void receiveReadChunks(Chunk.Builder... chunks) {
    client.receiveServerStream(SERVICE, "Read", chunks);
  }

  private void receiveWriteChunks(Chunk.Builder... chunks) {
    client.receiveServerStream(SERVICE, "Write", chunks);
  }

  /** Receive these chunks after a chunk is sent. */
  private void enqueueWriteChunks(int afterPackets, Chunk.Builder... chunks) {
    client.enqueueServerStream(SERVICE, "Write", afterPackets, chunks);
  }

  private List<Chunk> lastChunks() {
    return client.lastClientStreams(Chunk.class);
  }

  private Manager createManager(int transferTimeoutMillis, int initialTransferTimeoutMillis) {
    return new Manager(client.client().method(CHANNEL_ID, SERVICE + "/Read"),
        client.client().method(CHANNEL_ID, SERVICE + "/Write"),
        Runnable::run, // Run the job immediately on the same thread
        transferTimeoutMillis,
        initialTransferTimeoutMillis,
        MAX_RETRIES,
        () -> this.shouldAbortFlag);
  }

  private void setPlatformTransferDisabled() {
    this.shouldAbortFlag = true;
  }

  private void setPlatformTransferEnabled() {
    this.shouldAbortFlag = false;
  }
}
