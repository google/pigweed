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
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

public final class TransferClientTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  private static final int CHANNEL_ID = 1;
  private static final String SERVICE = "pw.transfer.Transfer";

  private static final ByteString TEST_DATA_SHORT = ByteString.copyFromUtf8("O_o");
  private static final ByteString TEST_DATA_100B = range(0, 100);

  private static final TransferParameters TRANSFER_PARAMETERS =
      TransferParameters.create(50, 30, 0);
  private static final int MAX_RETRIES = 2;

  private boolean shouldAbortFlag = false;
  private TestClient rpcClient;
  private TransferClient transferClient;

  @Mock private Consumer<TransferProgress> progressCallback;
  @Captor private ArgumentCaptor<TransferProgress> progress;

  @Before
  public void setup() {
    rpcClient = new TestClient(ImmutableList.of(TransferService.get()));
  }

  @After
  public void tearDown() {
    try {
      transferClient.close();
    } catch (InterruptedException e) {
      throw new AssertionError(e);
    }
  }

  @Test
  public void legacy_read_singleChunk_successful() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(1);
    assertThat(future.isDone()).isFalse();

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 1)
                          .setOffset(0)
                          .setData(TEST_DATA_SHORT)
                          .setRemainingBytes(0));

    assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
  }

  @Test
  public void legacy_read_failedPreconditionError_retriesInitialPacket() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(1, TRANSFER_PARAMETERS);

    assertThat(lastChunks()).containsExactly(initialLegacyReadChunk(1));

    receiveReadServerError(Status.FAILED_PRECONDITION);

    assertThat(lastChunks()).containsExactly(initialLegacyReadChunk(1));

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 1)
                          .setOffset(0)
                          .setData(TEST_DATA_SHORT)
                          .setRemainingBytes(0));

    assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
  }

  @Test
  public void legacy_read_failedPreconditionError_abortsAfterInitialPacket() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    TransferParameters params = TransferParameters.create(50, 50, 0);
    ListenableFuture<byte[]> future = transferClient.read(1, params);

    assertThat(lastChunks()).containsExactly(initialLegacyReadChunk(1, params));

    receiveReadChunks(legacyDataChunk(1, TEST_DATA_100B, 0, 50));

    assertThat(lastChunks())
        .containsExactly(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 1)
                             .setOffset(50)
                             .setPendingBytes(50)
                             .setWindowEndOffset(100)
                             .setMaxChunkSizeBytes(50)
                             .build());

    receiveReadServerError(Status.FAILED_PRECONDITION);

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void legacy_read_failedPreconditionErrorMaxRetriesTimes_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(1, TRANSFER_PARAMETERS);

    for (int i = 0; i < MAX_RETRIES; ++i) {
      receiveReadServerError(Status.FAILED_PRECONDITION);
    }

    Chunk initialChunk = initialLegacyReadChunk(1);
    assertThat(lastChunks())
        .containsExactlyElementsIn(Collections.nCopies(1 + MAX_RETRIES, initialChunk));

    receiveReadServerError(Status.FAILED_PRECONDITION);

    assertThat(lastChunks()).isEmpty();

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void legacy_read_singleChunk_ignoresUnknownIdOrWriteChunks() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(1);
    assertThat(future.isDone()).isFalse();

    receiveReadChunks(legacyFinalChunk(2, Status.OK),
        newLegacyChunk(Chunk.Type.DATA, 0)
            .setOffset(0)
            .setData(TEST_DATA_100B)
            .setRemainingBytes(0),
        newLegacyChunk(Chunk.Type.DATA, 3)
            .setOffset(0)
            .setData(TEST_DATA_100B)
            .setRemainingBytes(0));
    receiveWriteChunks(legacyFinalChunk(1, Status.OK),
        newLegacyChunk(Chunk.Type.DATA, 1)
            .setOffset(0)
            .setData(TEST_DATA_100B)
            .setRemainingBytes(0),
        newLegacyChunk(Chunk.Type.DATA, 2)
            .setOffset(0)
            .setData(TEST_DATA_100B)
            .setRemainingBytes(0));

    assertThat(future.isDone()).isFalse();

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 1)
                          .setOffset(0)
                          .setData(TEST_DATA_SHORT)
                          .setRemainingBytes(0));

    assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
  }

  @Test
  public void legacy_read_empty() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(2);
    lastChunks(); // Discard initial chunk (tested elsewhere)

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 2).setRemainingBytes(0));

    assertThat(lastChunks()).containsExactly(legacyFinalChunk(2, Status.OK));

    assertThat(future.get()).isEqualTo(new byte[] {});
  }

  @Test
  public void legacy_read_sendsTransferParametersFirst() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    TransferParameters params = TransferParameters.create(3, 2, 1);
    ListenableFuture<byte[]> future = transferClient.read(99, params);

    assertThat(lastChunks()).containsExactly(initialLegacyReadChunk(99, params));
    assertThat(future.cancel(true)).isTrue();
  }

  @Test
  public void legacy_read_severalChunks() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(123, TRANSFER_PARAMETERS);

    assertThat(lastChunks()).containsExactly(initialLegacyReadChunk(123));

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123)
                          .setOffset(0)
                          .setData(range(0, 20))
                          .setRemainingBytes(70),
        newLegacyChunk(Chunk.Type.DATA, 123).setOffset(20).setData(range(20, 40)));

    assertThat(lastChunks())
        .containsExactly(newLegacyChunk(Chunk.Type.PARAMETERS_CONTINUE, 123)
                             .setOffset(40)
                             .setPendingBytes(50)
                             .setMaxChunkSizeBytes(30)
                             .setWindowEndOffset(90)
                             .build());

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123).setOffset(40).setData(range(40, 70)));

    assertThat(lastChunks())
        .containsExactly(newLegacyChunk(Chunk.Type.PARAMETERS_CONTINUE, 123)
                             .setOffset(70)
                             .setPendingBytes(50)
                             .setMaxChunkSizeBytes(30)
                             .setWindowEndOffset(120)
                             .build());

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123)
                          .setOffset(70)
                          .setData(range(70, 100))
                          .setRemainingBytes(0));

    assertThat(lastChunks()).containsExactly(legacyFinalChunk(123, Status.OK));

    assertThat(future.get()).isEqualTo(TEST_DATA_100B.toByteArray());
  }

  @Test
  public void legacy_read_progressCallbackIsCalled() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future =
        transferClient.read(123, TRANSFER_PARAMETERS, progressCallback);

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123).setOffset(0).setData(range(0, 30)),
        newLegacyChunk(Chunk.Type.DATA, 123).setOffset(30).setData(range(30, 50)),
        newLegacyChunk(Chunk.Type.DATA, 123)
            .setOffset(50)
            .setData(range(50, 60))
            .setRemainingBytes(5),
        newLegacyChunk(Chunk.Type.DATA, 123).setOffset(60).setData(range(60, 70)),
        newLegacyChunk(Chunk.Type.DATA, 123)
            .setOffset(70)
            .setData(range(70, 80))
            .setRemainingBytes(20),
        newLegacyChunk(Chunk.Type.DATA, 123).setOffset(90).setData(range(90, 100)),
        newLegacyChunk(Chunk.Type.DATA, 123)
            .setOffset(80)
            .setData(range(80, 100))
            .setRemainingBytes(0));

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
  public void legacy_read_rewindWhenPacketsSkipped() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(123, TRANSFER_PARAMETERS);

    assertThat(lastChunks()).containsExactly(initialLegacyReadChunk(123));

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123).setOffset(50).setData(range(30, 50)));

    assertThat(lastChunks())
        .containsExactly(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                             .setPendingBytes(50)
                             .setWindowEndOffset(50)
                             .setMaxChunkSizeBytes(30)
                             .setOffset(0)
                             .build());

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123).setOffset(0).setData(range(0, 30)),
        newLegacyChunk(Chunk.Type.DATA, 123).setOffset(30).setData(range(30, 50)));

    assertThat(lastChunks())
        .containsExactly(newLegacyChunk(Chunk.Type.PARAMETERS_CONTINUE, 123)
                             .setOffset(30)
                             .setPendingBytes(50)
                             .setWindowEndOffset(80)
                             .setMaxChunkSizeBytes(30)
                             .build());

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123)
                          .setOffset(80)
                          .setData(range(80, 100))
                          .setRemainingBytes(0));

    assertThat(lastChunks())
        .containsExactly(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                             .setOffset(50)
                             .setPendingBytes(50)
                             .setWindowEndOffset(100)
                             .setMaxChunkSizeBytes(30)
                             .build());

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123).setOffset(50).setData(range(50, 80)),
        newLegacyChunk(Chunk.Type.DATA, 123)
            .setOffset(80)
            .setData(range(80, 100))
            .setRemainingBytes(0));

    assertThat(lastChunks())
        .containsExactly(newLegacyChunk(Chunk.Type.PARAMETERS_CONTINUE, 123)
                             .setOffset(80)
                             .setPendingBytes(50)
                             .setWindowEndOffset(130)
                             .setMaxChunkSizeBytes(30)
                             .build(),
            legacyFinalChunk(123, Status.OK));

    assertThat(future.get()).isEqualTo(TEST_DATA_100B.toByteArray());
  }

  @Test
  public void legacy_read_multipleWithSameId_sequentially_successful() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    for (int i = 0; i < 3; ++i) {
      ListenableFuture<byte[]> future = transferClient.read(1);

      receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 1)
                            .setOffset(0)
                            .setData(TEST_DATA_SHORT)
                            .setRemainingBytes(0));

      assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
    }
  }

  @Test
  public void legacy_read_multipleWithSameId_atSameTime_failsWithAlreadyExistsError() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> first = transferClient.read(123);
    ListenableFuture<byte[]> second = transferClient.read(123);

    assertThat(first.isDone()).isFalse();

    ExecutionException thrown = assertThrows(ExecutionException.class, second::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ALREADY_EXISTS);
  }

  @Test
  public void legacy_read_sendErrorOnFirstPacket_fails() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ChannelOutputException exception = new ChannelOutputException("blah");
    rpcClient.setChannelOutputException(exception);
    ListenableFuture<byte[]> future = transferClient.read(123);

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void legacy_read_sendErrorOnLaterPacket_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(123, TRANSFER_PARAMETERS);

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123).setOffset(0).setData(range(0, 20)));

    ChannelOutputException exception = new ChannelOutputException("blah");
    rpcClient.setChannelOutputException(exception);

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123).setOffset(20).setData(range(20, 50)));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void legacy_read_cancelFuture_abortsTransfer() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(123, TRANSFER_PARAMETERS);

    assertThat(future.cancel(true)).isTrue();

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 123).setOffset(30).setData(range(30, 50)));
    assertThat(lastChunks()).contains(legacyFinalChunk(123, Status.CANCELLED));
  }

  @Test
  public void legacy_read_transferProtocolError_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(123);

    receiveReadChunks(legacyFinalChunk(123, Status.ALREADY_EXISTS));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);

    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ALREADY_EXISTS);
  }

  @Test
  public void legacy_read_rpcError_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(2);

    receiveReadServerError(Status.NOT_FOUND);

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void legacy_read_timeout() {
    createTransferClientThatMayTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<byte[]> future = transferClient.read(123, TRANSFER_PARAMETERS);

    // Call future.get() without sending any server-side packets.
    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.DEADLINE_EXCEEDED);

    // read should have retried sending the transfer parameters 2 times, for a total of 3
    assertThat(lastChunks())
        .containsExactly(
            initialLegacyReadChunk(123), initialLegacyReadChunk(123), initialLegacyReadChunk(123));
  }

  @Test
  public void legacy_write_singleChunk() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());
    assertThat(future.isDone()).isFalse();

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 2)
                           .setOffset(0)
                           .setPendingBytes(1024)
                           .setMaxChunkSizeBytes(128),
        legacyFinalChunk(2, Status.OK));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void legacy_write_platformTransferDisabled_aborted() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());
    assertThat(future.isDone()).isFalse();

    shouldAbortFlag = true;
    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 2)
                           .setOffset(0)
                           .setPendingBytes(1024)
                           .setMaxChunkSizeBytes(128),
        legacyFinalChunk(2, Status.OK));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ABORTED);
  }

  @Test
  public void legacy_write_failedPreconditionError_retriesInitialPacket() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());

    assertThat(lastChunks()).containsExactly(initialLegacyWriteChunk(2, TEST_DATA_SHORT.size()));

    receiveWriteServerError(Status.FAILED_PRECONDITION);

    assertThat(lastChunks()).containsExactly(initialLegacyWriteChunk(2, TEST_DATA_SHORT.size()));

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 2)
                           .setOffset(0)
                           .setPendingBytes(1024)
                           .setMaxChunkSizeBytes(128),
        legacyFinalChunk(2, Status.OK));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void legacy_write_failedPreconditionError_abortsAfterInitialPacket() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_100B.toByteArray());

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 2)
                           .setOffset(0)
                           .setPendingBytes(50)
                           .setMaxChunkSizeBytes(50));

    assertThat(lastChunks())
        .containsExactly(initialLegacyWriteChunk(2, TEST_DATA_100B.size()),
            legacyDataChunk(2, TEST_DATA_100B, 0, 50));

    receiveWriteServerError(Status.FAILED_PRECONDITION);

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void legacy_write_failedPreconditionErrorMaxRetriesTimes_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());

    for (int i = 0; i < MAX_RETRIES; ++i) {
      receiveWriteServerError(Status.FAILED_PRECONDITION);
    }

    Chunk initialChunk = initialLegacyWriteChunk(2, TEST_DATA_SHORT.size());
    assertThat(lastChunks())
        .containsExactlyElementsIn(Collections.nCopies(1 + MAX_RETRIES, initialChunk));

    receiveWriteServerError(Status.FAILED_PRECONDITION);

    assertThat(lastChunks()).isEmpty();

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void legacy_write_empty() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(2, new byte[] {});
    assertThat(future.isDone()).isFalse();

    receiveWriteChunks(legacyFinalChunk(2, Status.OK));

    assertThat(lastChunks()).containsExactly(initialLegacyWriteChunk(2, 0));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void legacy_write_severalChunks() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_100B.toByteArray());

    assertThat(lastChunks()).containsExactly(initialLegacyWriteChunk(123, TEST_DATA_100B.size()));

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                           .setOffset(0)
                           .setPendingBytes(50)
                           .setMaxChunkSizeBytes(30)
                           .setMinDelayMicroseconds(1));

    assertThat(lastChunks())
        .containsExactly(
            newLegacyChunk(Chunk.Type.DATA, 123).setOffset(0).setData(range(0, 30)).build(),
            newLegacyChunk(Chunk.Type.DATA, 123).setOffset(30).setData(range(30, 50)).build());

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                           .setOffset(50)
                           .setPendingBytes(40)
                           .setMaxChunkSizeBytes(25));

    assertThat(lastChunks())
        .containsExactly(
            newLegacyChunk(Chunk.Type.DATA, 123).setOffset(50).setData(range(50, 75)).build(),
            newLegacyChunk(Chunk.Type.DATA, 123).setOffset(75).setData(range(75, 90)).build());

    receiveWriteChunks(
        newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123).setOffset(90).setPendingBytes(50));

    assertThat(lastChunks())
        .containsExactly(newLegacyChunk(Chunk.Type.DATA, 123)
                             .setOffset(90)
                             .setData(range(90, 100))
                             .setRemainingBytes(0)
                             .build());

    assertThat(future.isDone()).isFalse();

    receiveWriteChunks(legacyFinalChunk(123, Status.OK));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void legacy_write_parametersContinue() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_100B.toByteArray());

    assertThat(lastChunks()).containsExactly(initialLegacyWriteChunk(123, TEST_DATA_100B.size()));

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                           .setOffset(0)
                           .setPendingBytes(50)
                           .setWindowEndOffset(50)
                           .setMaxChunkSizeBytes(30)
                           .setMinDelayMicroseconds(1));

    assertThat(lastChunks())
        .containsExactly(
            newLegacyChunk(Chunk.Type.DATA, 123).setOffset(0).setData(range(0, 30)).build(),
            newLegacyChunk(Chunk.Type.DATA, 123).setOffset(30).setData(range(30, 50)).build());

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_CONTINUE, 123)
                           .setOffset(30)
                           .setPendingBytes(50)
                           .setWindowEndOffset(80));

    // Transfer doesn't roll back to offset 30 but instead continues sending up to 80.
    assertThat(lastChunks())
        .containsExactly(
            newLegacyChunk(Chunk.Type.DATA, 123).setOffset(50).setData(range(50, 80)).build());

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_CONTINUE, 123)
                           .setOffset(80)
                           .setPendingBytes(50)
                           .setWindowEndOffset(130));

    assertThat(lastChunks())
        .containsExactly(newLegacyChunk(Chunk.Type.DATA, 123)
                             .setOffset(80)
                             .setData(range(80, 100))
                             .setRemainingBytes(0)
                             .build());

    assertThat(future.isDone()).isFalse();

    receiveWriteChunks(legacyFinalChunk(123, Status.OK));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void legacy_write_continuePacketWithWindowEndBeforeOffsetIsIgnored() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_100B.toByteArray());

    assertThat(lastChunks()).containsExactly(initialLegacyWriteChunk(123, TEST_DATA_100B.size()));

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                           .setOffset(0)
                           .setPendingBytes(90)
                           .setWindowEndOffset(90)
                           .setMaxChunkSizeBytes(90)
                           .setMinDelayMicroseconds(1));

    assertThat(lastChunks())
        .containsExactly(
            newLegacyChunk(Chunk.Type.DATA, 123).setOffset(0).setData(range(0, 90)).build());

    receiveWriteChunks(
        // This stale packet with a window end before the offset should be ignored.
        newLegacyChunk(Chunk.Type.PARAMETERS_CONTINUE, 123)
            .setOffset(25)
            .setPendingBytes(25)
            .setWindowEndOffset(50),
        // Start from an arbitrary offset before the current, but extend the window to the end.
        newLegacyChunk(Chunk.Type.PARAMETERS_CONTINUE, 123).setOffset(80).setWindowEndOffset(100));

    assertThat(lastChunks())
        .containsExactly(newLegacyChunk(Chunk.Type.DATA, 123)
                             .setOffset(90)
                             .setData(range(90, 100))
                             .setRemainingBytes(0)
                             .build());

    receiveWriteChunks(legacyFinalChunk(123, Status.OK));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void legacy_write_progressCallbackIsCalled() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future =
        transferClient.write(123, TEST_DATA_100B.toByteArray(), progressCallback);

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                           .setOffset(0)
                           .setPendingBytes(90)
                           .setMaxChunkSizeBytes(30),
        newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123).setOffset(50).setPendingBytes(50),
        legacyFinalChunk(123, Status.OK));

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
  public void legacy_write_asksForFinalOffset_sendsFinalPacket() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_100B.toByteArray());

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                           .setOffset(100)
                           .setPendingBytes(40)
                           .setMaxChunkSizeBytes(25));

    assertThat(lastChunks())
        .containsExactly(initialLegacyWriteChunk(123, TEST_DATA_100B.size()),
            newLegacyChunk(Chunk.Type.DATA, 123).setOffset(100).setRemainingBytes(0).build());
    assertThat(future.isDone()).isFalse();
  }

  @Test
  public void legacy_write_multipleWithSameId_sequentially_successful() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    for (int i = 0; i < 3; ++i) {
      ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());

      receiveWriteChunks(
          newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123).setOffset(0).setPendingBytes(50),
          legacyFinalChunk(123, Status.OK));

      future.get();
    }
  }

  @Test
  public void legacy_write_multipleWithSameId_atSameTime_failsWithAlreadyExistsError() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> first = transferClient.write(123, TEST_DATA_SHORT.toByteArray());
    ListenableFuture<Void> second = transferClient.write(123, TEST_DATA_SHORT.toByteArray());

    assertThat(first.isDone()).isFalse();

    ExecutionException thrown = assertThrows(ExecutionException.class, second::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ALREADY_EXISTS);
  }

  @Test
  public void legacy_write_sendErrorOnFirstPacket_failsImmediately() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ChannelOutputException exception = new ChannelOutputException("blah");
    rpcClient.setChannelOutputException(exception);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void legacy_write_serviceRequestsNoData_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123).setOffset(0));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INVALID_ARGUMENT);
  }

  @Test
  public void legacy_write_invalidOffset_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_100B.toByteArray());

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                           .setOffset(101)
                           .setPendingBytes(40)
                           .setMaxChunkSizeBytes(25));

    assertThat(lastChunks())
        .containsExactly(initialLegacyWriteChunk(123, TEST_DATA_100B.size()),
            legacyFinalChunk(123, Status.OUT_OF_RANGE));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.OUT_OF_RANGE);
  }

  @Test
  public void legacy_write_sendErrorOnLaterPacket_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());

    ChannelOutputException exception = new ChannelOutputException("blah");
    rpcClient.setChannelOutputException(exception);

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                           .setOffset(0)
                           .setPendingBytes(50)
                           .setMaxChunkSizeBytes(30));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void legacy_write_cancelFuture_abortsTransfer() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_100B.toByteArray());

    assertThat(future.cancel(true)).isTrue();

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
                           .setOffset(0)
                           .setPendingBytes(50)
                           .setMaxChunkSizeBytes(50));
    assertThat(lastChunks()).contains(legacyFinalChunk(123, Status.CANCELLED));
  }

  @Test
  public void legacy_write_transferProtocolError_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());

    receiveWriteChunks(legacyFinalChunk(123, Status.NOT_FOUND));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);

    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.NOT_FOUND);
  }

  @Test
  public void legacy_write_rpcError_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());

    receiveWriteServerError(Status.NOT_FOUND);

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void legacy_write_timeoutAfterInitialChunk() {
    createTransferClientThatMayTimeOut(ProtocolVersion.LEGACY);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());

    // Call future.get() without sending any server-side packets.
    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.DEADLINE_EXCEEDED);

    // Client should have resent the last chunk (the initial chunk in this case) for each timeout.
    assertThat(lastChunks())
        .containsExactly(initialLegacyWriteChunk(123, TEST_DATA_SHORT.size()), // initial
            initialLegacyWriteChunk(123, TEST_DATA_SHORT.size()), // retry 1
            initialLegacyWriteChunk(123, TEST_DATA_SHORT.size())); // retry 2
  }

  @Test
  public void legacy_write_timeoutAfterSingleChunk() {
    createTransferClientThatMayTimeOut(ProtocolVersion.LEGACY);

    // Wait for two outgoing packets (Write RPC request and first chunk), then send the parameters.
    enqueueWriteChunks(2,
        newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
            .setOffset(0)
            .setPendingBytes(90)
            .setMaxChunkSizeBytes(30));
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.DEADLINE_EXCEEDED);

    Chunk data = newLegacyChunk(Chunk.Type.DATA, 123)
                     .setOffset(0)
                     .setData(TEST_DATA_SHORT)
                     .setRemainingBytes(0)
                     .build();
    assertThat(lastChunks())
        .containsExactly(initialLegacyWriteChunk(123, TEST_DATA_SHORT.size()), // initial
            data, // data chunk
            data, // retry 1
            data); // retry 2
  }

  @Test
  public void legacy_write_multipleTimeoutsAndRecoveries() throws Exception {
    createTransferClientThatMayTimeOut(ProtocolVersion.LEGACY);

    // Wait for two outgoing packets (Write RPC request and first chunk), then send the parameters.
    enqueueWriteChunks(2,
        newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
            .setOffset(0)
            .setWindowEndOffset(40)
            .setMaxChunkSizeBytes(20));

    // After the second retry, send more transfer parameters
    enqueueWriteChunks(4,
        newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
            .setOffset(40)
            .setWindowEndOffset(120)
            .setMaxChunkSizeBytes(40));

    // After the first retry, send more transfer parameters
    enqueueWriteChunks(3,
        newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123)
            .setOffset(80)
            .setWindowEndOffset(160)
            .setMaxChunkSizeBytes(10));

    // After the second retry, confirm completed
    enqueueWriteChunks(
        4, newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 123).setStatus(Status.OK.code()));

    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_100B.toByteArray());

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.

    assertThat(lastChunks())
        .containsExactly(
            // initial
            initialLegacyWriteChunk(123, TEST_DATA_100B.size()),
            // after 2, receive parameters: 40 from 0 by 20
            legacyDataChunk(123, TEST_DATA_100B, 0, 20), // data 0-20
            legacyDataChunk(123, TEST_DATA_100B, 20, 40), // data 20-40
            legacyDataChunk(123, TEST_DATA_100B, 20, 40), // retry 1
            legacyDataChunk(123, TEST_DATA_100B, 20, 40), // retry 2
            // after 4, receive parameters: 80 from 40 by 40
            legacyDataChunk(123, TEST_DATA_100B, 40, 80), // data 40-80
            legacyDataChunk(123, TEST_DATA_100B, 80, 100), // data 80-100
            legacyDataChunk(123, TEST_DATA_100B, 80, 100), // retry 1
            // after 3, receive parameters: 80 from 80 by 10
            legacyDataChunk(123, TEST_DATA_100B, 80, 90), // data 80-90
            legacyDataChunk(123, TEST_DATA_100B, 90, 100), // data 90-100
            legacyDataChunk(123, TEST_DATA_100B, 90, 100), // retry 1
            legacyDataChunk(123, TEST_DATA_100B, 90, 100)); // retry 2
    // after 4, receive final OK
  }

  @Test
  public void read_singleChunk_successful() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(3, TRANSFER_PARAMETERS);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);
    assertThat(future.isDone()).isFalse();

    assertThat(lastChunks()).containsExactly(initialReadChunk(transfer));

    receiveReadChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                          .setResourceId(3)
                          .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    assertThat(lastChunks())
        .containsExactly(readStartAckConfirmation(transfer.getSessionId(), TRANSFER_PARAMETERS));

    receiveReadChunks(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                          .setOffset(0)
                          .setData(TEST_DATA_SHORT)
                          .setRemainingBytes(0));

    assertThat(lastChunks())
        .containsExactly(Chunk.newBuilder()
                             .setType(Chunk.Type.COMPLETION)
                             .setSessionId(transfer.getSessionId())
                             .setStatus(Status.OK.ordinal())
                             .build());

    assertThat(future.isDone()).isFalse();

    receiveReadChunks(newChunk(Chunk.Type.COMPLETION_ACK, transfer.getSessionId()));

    assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
  }

  @Test
  public void read_requestV2ReceiveLegacy() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(1, TRANSFER_PARAMETERS);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);
    assertThat(future.isDone()).isFalse();

    assertThat(lastChunks()).containsExactly(initialReadChunk(transfer));

    receiveReadChunks(newLegacyChunk(Chunk.Type.DATA, 1)
                          .setOffset(0)
                          .setData(TEST_DATA_SHORT)
                          .setRemainingBytes(0));

    // No handshake packets since the server responded as legacy.
    assertThat(lastChunks()).containsExactly(legacyFinalChunk(1, Status.OK));

    assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
  }

  @Test
  public void read_failedPreconditionError_retriesInitialPacket() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(1, TRANSFER_PARAMETERS);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    assertThat(lastChunks()).containsExactly(initialReadChunk(transfer));
    for (int i = 0; i < MAX_RETRIES; ++i) {
      receiveReadServerError(Status.FAILED_PRECONDITION);

      assertThat(lastChunks()).containsExactly(initialReadChunk(transfer));
    }

    receiveReadChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                          .setResourceId(1)
                          .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    assertThat(lastChunks())
        .containsExactly(readStartAckConfirmation(transfer.getSessionId(), TRANSFER_PARAMETERS));
  }

  @Test
  public void read_failedPreconditionError_abortsAfterInitial() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    TransferParameters params = TransferParameters.create(50, 50, 0);
    ListenableFuture<byte[]> future = transferClient.read(1, params);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    assertThat(lastChunks()).containsExactly(initialReadChunk(transfer));

    receiveReadChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                          .setResourceId(1)
                          .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    receiveReadServerError(Status.FAILED_PRECONDITION);

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void read_failedPreconditionError_abortsAfterHandshake() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    TransferParameters params = TransferParameters.create(50, 50, 0);
    ListenableFuture<byte[]> future = transferClient.read(1, params);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    assertThat(lastChunks()).containsExactly(initialReadChunk(transfer));

    receiveReadChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                          .setResourceId(1)
                          .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    assertThat(lastChunks())
        .containsExactly(readStartAckConfirmation(transfer.getSessionId(), params));

    receiveReadChunks(dataChunk(transfer.getSessionId(), TEST_DATA_100B, 0, 50));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                             .setOffset(50)
                             .setWindowEndOffset(100)
                             .setMaxChunkSizeBytes(50)
                             .build());

    receiveReadServerError(Status.FAILED_PRECONDITION);

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void read_failedPreconditionErrorMaxRetriesTimes_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(1, TRANSFER_PARAMETERS);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    for (int i = 0; i < MAX_RETRIES; ++i) {
      receiveReadServerError(Status.FAILED_PRECONDITION);
    }

    Chunk initialChunk = initialReadChunk(transfer);
    assertThat(lastChunks())
        .containsExactlyElementsIn(Collections.nCopies(1 + MAX_RETRIES, initialChunk));

    receiveReadServerError(Status.FAILED_PRECONDITION);

    assertThat(lastChunks()).isEmpty();

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void read_singleChunk_ignoresUnknownIdOrWriteChunks() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(1);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);
    assertThat(future.isDone()).isFalse();

    performReadStartHandshake(transfer);

    receiveReadChunks(finalChunk(transfer.getSessionId() + 1, Status.OK),
        newChunk(Chunk.Type.DATA, transfer.getSessionId() + 2)
            .setOffset(0)
            .setData(TEST_DATA_100B)
            .setRemainingBytes(0),
        newChunk(Chunk.Type.DATA, transfer.getSessionId() + 3)
            .setOffset(0)
            .setData(TEST_DATA_100B)
            .setRemainingBytes(0));
    receiveWriteChunks(finalChunk(transfer.getSessionId(), Status.INVALID_ARGUMENT),
        newChunk(Chunk.Type.DATA, transfer.getSessionId())
            .setOffset(0)
            .setData(TEST_DATA_100B)
            .setRemainingBytes(0),
        newChunk(Chunk.Type.DATA, transfer.getSessionId() + 1)
            .setOffset(0)
            .setData(TEST_DATA_100B)
            .setRemainingBytes(0));

    assertThat(future.isDone()).isFalse();

    receiveReadChunks(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                          .setOffset(0)
                          .setData(TEST_DATA_SHORT)
                          .setRemainingBytes(0));

    performReadCompletionHandshake(transfer.getSessionId(), Status.OK);

    assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
  }

  @Test
  public void read_empty() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(2);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);
    performReadStartHandshake(transfer);
    receiveReadChunks(newChunk(Chunk.Type.DATA, transfer.getSessionId()).setRemainingBytes(0));

    performReadCompletionHandshake(transfer.getSessionId(), Status.OK);

    assertThat(future.get()).isEqualTo(new byte[] {});
  }

  @Test
  public void read_sendsTransferParametersFirst() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    TransferParameters params = TransferParameters.create(3, 2, 1);
    ListenableFuture<byte[]> future = transferClient.read(99, params);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    assertThat(lastChunks()).containsExactly(initialReadChunk(transfer));
    assertThat(future.cancel(true)).isTrue();
  }

  @Test
  public void read_severalChunks() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(7, TRANSFER_PARAMETERS);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    performReadStartHandshake(transfer);

    receiveReadChunks(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                          .setOffset(0)
                          .setData(range(0, 20))
                          .setRemainingBytes(70),
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(20).setData(range(20, 40)));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.PARAMETERS_CONTINUE, transfer.getSessionId())
                             .setOffset(40)
                             .setMaxChunkSizeBytes(30)
                             .setWindowEndOffset(90)
                             .build());

    receiveReadChunks(
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(40).setData(range(40, 70)));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.PARAMETERS_CONTINUE, transfer.getSessionId())
                             .setOffset(70)
                             .setMaxChunkSizeBytes(30)
                             .setWindowEndOffset(120)
                             .build());

    receiveReadChunks(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                          .setOffset(70)
                          .setData(range(70, 100))
                          .setRemainingBytes(0));

    performReadCompletionHandshake(transfer.getSessionId(), Status.OK);

    assertThat(future.get()).isEqualTo(TEST_DATA_100B.toByteArray());
  }

  @Test
  public void read_onlySendsOneUpdateAfterDrops() throws Exception {
    createTransferClientThatMayTimeOut(ProtocolVersion.VERSION_TWO);
    TransferParameters params = TransferParameters.create(50, 10, 0);

    final int id = transferClient.getNextSessionIdForTest();

    // Handshake
    enqueueReadChunks(2, // Wait for read RPC open & START packet
        newChunk(Chunk.Type.START_ACK, id)
            .setResourceId(7)
            .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));
    enqueueReadChunks(1, // Ignore the first START_ACK_CONFIRMATION
        newChunk(Chunk.Type.START_ACK, id)
            .setResourceId(7)
            .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    // Window 1: server waits for START_ACK_CONFIRMATION, drops 2nd packet
    enqueueReadChunks(1,
        newChunk(Chunk.Type.DATA, id).setOffset(0).setData(range(0, 10)),
        newChunk(Chunk.Type.DATA, id).setOffset(20).setData(range(20, 30)),
        newChunk(Chunk.Type.DATA, id).setOffset(30).setData(range(30, 40)),
        newChunk(Chunk.Type.DATA, id).setOffset(40).setData(range(40, 50)));

    // Window 2: server waits for retransmit, drops 1st packet
    enqueueReadChunks(1,
        newChunk(Chunk.Type.DATA, id).setOffset(20).setData(range(20, 30)),
        newChunk(Chunk.Type.DATA, id).setOffset(30).setData(range(30, 40)),
        newChunk(Chunk.Type.DATA, id).setOffset(40).setData(range(40, 50)),
        newChunk(Chunk.Type.DATA, id).setOffset(50).setData(range(50, 60)));

    // Window 3: server waits for retransmit, drops last packet
    enqueueReadChunks(1,
        newChunk(Chunk.Type.DATA, id).setOffset(10).setData(range(10, 20)),
        newChunk(Chunk.Type.DATA, id).setOffset(20).setData(range(20, 30)),
        newChunk(Chunk.Type.DATA, id).setOffset(30).setData(range(30, 40)),
        newChunk(Chunk.Type.DATA, id).setOffset(40).setData(range(40, 50)));

    // Window 4: server waits for continue and retransmit, normal window.
    enqueueReadChunks(2,
        newChunk(Chunk.Type.DATA, id).setOffset(50).setData(range(50, 60)),
        newChunk(Chunk.Type.DATA, id).setOffset(60).setData(range(60, 70)),
        newChunk(Chunk.Type.DATA, id).setOffset(70).setData(range(70, 80)),
        newChunk(Chunk.Type.DATA, id).setOffset(80).setData(range(80, 90)),
        newChunk(Chunk.Type.DATA, id).setOffset(90).setData(range(90, 100)));
    enqueueReadChunks(2, // Ignore continue and retransmit chunks, retry last packet in window
        newChunk(Chunk.Type.DATA, id).setOffset(90).setData(range(90, 100)),
        newChunk(Chunk.Type.DATA, id).setOffset(90).setData(range(90, 100)));

    // Window 5: Final packet
    enqueueReadChunks(2, // Receive two retries, then send final packet
        newChunk(Chunk.Type.DATA, id).setOffset(100).setData(range(100, 110)).setRemainingBytes(0));
    enqueueReadChunks(1, // Ignore first COMPLETION packet
        newChunk(Chunk.Type.DATA, id).setOffset(100).setData(range(100, 110)).setRemainingBytes(0));
    enqueueReadChunks(1, newChunk(Chunk.Type.COMPLETION_ACK, id));

    ListenableFuture<byte[]> future = transferClient.read(7, params);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);
    assertThat(transfer.getSessionId()).isEqualTo(id);
    assertThat(future.get()).isEqualTo(range(0, 110).toByteArray());

    assertThat(lastChunks())
        .containsExactly(
            // Handshake
            initialReadChunk(transfer),
            readStartAckConfirmation(id, params),
            readStartAckConfirmation(id, params),
            // Window 1: send one transfer parameters update after the drop
            newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id)
                .setOffset(10)
                .setWindowEndOffset(60)
                .setMaxChunkSizeBytes(10)
                .build(),
            // Window 2: send one transfer parameters update after the drop
            newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id)
                .setOffset(10)
                .setWindowEndOffset(60)
                .setMaxChunkSizeBytes(10)
                .build(),
            // Window 3: send one transfer parameters update after the drop, then continue packet
            newChunk(Chunk.Type.PARAMETERS_CONTINUE, id) // Not seen by server
                .setOffset(40)
                .setWindowEndOffset(90)
                .setMaxChunkSizeBytes(10)
                .build(),
            newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id) // Sent after timeout
                .setOffset(50)
                .setWindowEndOffset(100)
                .setMaxChunkSizeBytes(10)
                .build(),
            // Window 4: send one transfer parameters update after the drop, then continue packet
            newChunk(Chunk.Type.PARAMETERS_CONTINUE, id) // Ignored by server
                .setOffset(80)
                .setWindowEndOffset(130)
                .setMaxChunkSizeBytes(10)
                .build(),
            newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id) // Sent after last packet
                .setOffset(100)
                .setWindowEndOffset(150)
                .setMaxChunkSizeBytes(10)
                .build(),
            newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id) // Sent due to repeated packet
                .setOffset(100)
                .setWindowEndOffset(150)
                .setMaxChunkSizeBytes(10)
                .build(),
            newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id) // Sent due to repeated packet
                .setOffset(100)
                .setWindowEndOffset(150)
                .setMaxChunkSizeBytes(10)
                .build(),
            // Window 5: final packet and closing handshake
            newChunk(Chunk.Type.COMPLETION, id).setStatus(Status.OK.ordinal()).build(),
            newChunk(Chunk.Type.COMPLETION, id).setStatus(Status.OK.ordinal()).build());
  }

  @Test
  public void read_progressCallbackIsCalled() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future =
        transferClient.read(123, TRANSFER_PARAMETERS, progressCallback);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    performReadStartHandshake(transfer);

    receiveReadChunks(
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(0).setData(range(0, 30)),
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(30).setData(range(30, 50)),
        newChunk(Chunk.Type.DATA, transfer.getSessionId())
            .setOffset(50)
            .setData(range(50, 60))
            .setRemainingBytes(5),
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(60).setData(range(60, 70)),
        newChunk(Chunk.Type.DATA, transfer.getSessionId())
            .setOffset(70)
            .setData(range(70, 80))
            .setRemainingBytes(20),
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(90).setData(range(90, 100)),
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(0).setData(range(0, 30)));
    lastChunks(); // Discard chunks; no need to inspect for this test

    receiveReadChunks(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                          .setOffset(80)
                          .setData(range(80, 100))
                          .setRemainingBytes(0));
    performReadCompletionHandshake(transfer.getSessionId(), Status.OK);

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
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(123, TRANSFER_PARAMETERS);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    performReadStartHandshake(transfer);

    receiveReadChunks(
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(50).setData(range(30, 50)));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                             .setWindowEndOffset(50)
                             .setMaxChunkSizeBytes(30)
                             .setOffset(0)
                             .build());

    receiveReadChunks(
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(0).setData(range(0, 30)),
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(30).setData(range(30, 50)));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.PARAMETERS_CONTINUE, transfer.getSessionId())
                             .setOffset(30)
                             .setWindowEndOffset(80)
                             .setMaxChunkSizeBytes(30)
                             .build());

    receiveReadChunks(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                          .setOffset(80)
                          .setData(range(80, 100))
                          .setRemainingBytes(0));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                             .setOffset(50)
                             .setWindowEndOffset(100)
                             .setMaxChunkSizeBytes(30)
                             .build());

    receiveReadChunks(
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(50).setData(range(50, 80)));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.PARAMETERS_CONTINUE, transfer.getSessionId())
                             .setOffset(80)
                             .setWindowEndOffset(130)
                             .setMaxChunkSizeBytes(30)
                             .build());

    receiveReadChunks(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                          .setOffset(80)
                          .setData(range(80, 100))
                          .setRemainingBytes(0));

    performReadCompletionHandshake(transfer.getSessionId(), Status.OK);

    assertThat(future.get()).isEqualTo(TEST_DATA_100B.toByteArray());
  }

  @Test
  public void read_multipleWithSameId_sequentially_successful() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    for (int i = 0; i < 3; ++i) {
      ListenableFuture<byte[]> future = transferClient.read(1);
      ReadTransfer transfer = transferClient.getReadTransferForTest(future);

      performReadStartHandshake(transfer);

      receiveReadChunks(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                            .setOffset(0)
                            .setData(TEST_DATA_SHORT)
                            .setRemainingBytes(0));

      performReadCompletionHandshake(transfer.getSessionId(), Status.OK);

      assertThat(future.get()).isEqualTo(TEST_DATA_SHORT.toByteArray());
    }
  }

  @Test
  public void read_multipleWithSameId_atSameTime_failsWithAlreadyExistsError() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> first = transferClient.read(123);
    ListenableFuture<byte[]> second = transferClient.read(123);

    assertThat(first.isDone()).isFalse();

    ExecutionException thrown = assertThrows(ExecutionException.class, second::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ALREADY_EXISTS);
  }

  @Test
  public void read_sendErrorOnFirstPacket_fails() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ChannelOutputException exception = new ChannelOutputException("blah");
    rpcClient.setChannelOutputException(exception);
    ListenableFuture<byte[]> future = transferClient.read(123);

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void read_sendErrorOnLaterPacket_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(1024, TRANSFER_PARAMETERS);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    performReadStartHandshake(transfer);

    receiveReadChunks(
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(0).setData(range(0, 20)));

    ChannelOutputException exception = new ChannelOutputException("blah");
    rpcClient.setChannelOutputException(exception);

    receiveReadChunks(
        newChunk(Chunk.Type.DATA, transfer.getSessionId()).setOffset(20).setData(range(20, 50)));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void read_cancelFuture_abortsTransfer() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(1, TRANSFER_PARAMETERS);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    performReadStartHandshake(transfer);

    assertThat(future.cancel(true)).isTrue();

    assertThat(lastChunks()).contains(finalChunk(transfer.getSessionId(), Status.CANCELLED));
  }

  @Test
  public void read_immediateTransferProtocolError_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(123);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    receiveReadChunks(newChunk(Chunk.Type.COMPLETION, transfer.getSessionId())
                          .setResourceId(123)
                          .setStatus(Status.ALREADY_EXISTS.ordinal()));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);

    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ALREADY_EXISTS);
  }

  @Test
  public void read_laterTransferProtocolError_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(123);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    performReadStartHandshake(transfer);

    receiveReadChunks(finalChunk(transfer.getSessionId(), Status.ALREADY_EXISTS));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);

    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ALREADY_EXISTS);
  }

  @Test
  public void read_rpcError_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(2);

    receiveReadServerError(Status.NOT_FOUND);

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void read_serverRespondsWithUnknownVersion_invalidArgument() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(2, TRANSFER_PARAMETERS);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    assertThat(lastChunks()).containsExactly(initialReadChunk(transfer));

    receiveReadChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                          .setResourceId(2)
                          .setProtocolVersion(600613));

    assertThat(lastChunks())
        .containsExactly(finalChunk(transfer.getSessionId(), Status.INVALID_ARGUMENT));

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.INVALID_ARGUMENT);
  }

  @Test
  public void read_timeout() {
    createTransferClientThatMayTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<byte[]> future = transferClient.read(123, TRANSFER_PARAMETERS);
    ReadTransfer transfer = transferClient.getReadTransferForTest(future);

    // Call future.get() without sending any server-side packets.
    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.DEADLINE_EXCEEDED);

    // read should have retried sending the transfer parameters 2 times, for a total of 3
    assertThat(lastChunks())
        .containsExactly(
            initialReadChunk(transfer), initialReadChunk(transfer), initialReadChunk(transfer));
  }

  @Test
  public void read_generatesUniqueSessionIds() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);

    int sessionId1 = transferClient.getNextSessionIdForTest();
    ReadTransfer transfer1 = transferClient.getReadTransferForTest(transferClient.read(123));
    assertThat(sessionId1).isEqualTo(transfer1.getSessionId());

    int sessionId2 = transferClient.getNextSessionIdForTest();
    ReadTransfer transfer2 = transferClient.getReadTransferForTest(transferClient.read(456));
    assertThat(sessionId2).isEqualTo(transfer2.getSessionId());

    int sessionId3 = transferClient.getNextSessionIdForTest();
    ReadTransfer transfer3 = transferClient.getReadTransferForTest(transferClient.read(789));
    assertThat(sessionId3).isEqualTo(transfer3.getSessionId());

    assertThat(sessionId1).isNotEqualTo(sessionId2);
    assertThat(sessionId1).isNotEqualTo(sessionId3);
  }

  @Test
  public void write_singleChunk() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    // Do the start handshake (equivalent to performWriteStartHandshake()).
    assertThat(lastChunks()).containsExactly(initialWriteChunk(transfer, TEST_DATA_SHORT.size()));

    receiveWriteChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                           .setResourceId(2)
                           .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.START_ACK_CONFIRMATION, transfer.getSessionId())
                             .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal())
                             .setRemainingBytes(TEST_DATA_SHORT.size())
                             .build());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(0)
                           .setWindowEndOffset(1024)
                           .setMaxChunkSizeBytes(128));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                             .setOffset(0)
                             .setData(TEST_DATA_SHORT)
                             .setRemainingBytes(0)
                             .build());

    receiveWriteChunks(finalChunk(transfer.getSessionId(), Status.OK));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.COMPLETION_ACK, transfer.getSessionId()).build());

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_requestV2ReceiveLegacy() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    assertThat(lastChunks()).containsExactly(initialWriteChunk(transfer, TEST_DATA_SHORT.size()));

    receiveWriteChunks(newLegacyChunk(Chunk.Type.PARAMETERS_RETRANSMIT, 2)
                           .setOffset(0)
                           .setWindowEndOffset(1024)
                           .setMaxChunkSizeBytes(128),
        legacyFinalChunk(2, Status.OK));

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_platformTransferDisabled_aborted() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);
    assertThat(future.isDone()).isFalse();

    shouldAbortFlag = true;
    receiveWriteChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId()).setResourceId(2));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ABORTED);
  }

  @Test
  public void write_failedPreconditionError_retriesInitialPacket() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    assertThat(lastChunks()).containsExactly(initialWriteChunk(transfer, TEST_DATA_SHORT.size()));
    for (int i = 0; i < MAX_RETRIES; ++i) {
      receiveWriteServerError(Status.FAILED_PRECONDITION);

      assertThat(lastChunks()).containsExactly(initialWriteChunk(transfer, TEST_DATA_SHORT.size()));
    }

    receiveWriteChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                           .setResourceId(2)
                           .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.START_ACK_CONFIRMATION, transfer.getSessionId())
                             .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal())
                             .setRemainingBytes(TEST_DATA_SHORT.size())
                             .build());
  }

  @Test
  public void write_failedPreconditionError_abortsAfterInitialPacket() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    assertThat(lastChunks()).containsExactly(initialWriteChunk(transfer, TEST_DATA_100B.size()));

    receiveWriteChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                           .setResourceId(2)
                           .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    receiveWriteServerError(Status.FAILED_PRECONDITION);

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void write_failedPreconditionErrorMaxRetriesTimes_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    for (int i = 0; i < MAX_RETRIES; ++i) {
      receiveWriteServerError(Status.FAILED_PRECONDITION);
    }

    Chunk initialChunk = initialWriteChunk(transfer, TEST_DATA_SHORT.size());
    assertThat(lastChunks())
        .containsExactlyElementsIn(Collections.nCopies(1 + MAX_RETRIES, initialChunk));

    receiveWriteServerError(Status.FAILED_PRECONDITION);

    assertThat(lastChunks()).isEmpty();

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void write_empty() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(2, new byte[] {});
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, 0);

    receiveWriteChunks(finalChunk(transfer.getSessionId(), Status.OK));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.COMPLETION_ACK, transfer.getSessionId()).build());

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_severalChunks() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(500, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, TEST_DATA_100B.size());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(0)
                           .setWindowEndOffset(50)
                           .setMaxChunkSizeBytes(30)
                           .setMinDelayMicroseconds(1));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                             .setOffset(0)
                             .setData(range(0, 30))
                             .build(),
            newChunk(Chunk.Type.DATA, transfer.getSessionId())
                .setOffset(30)
                .setData(range(30, 50))
                .build());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(50)
                           .setWindowEndOffset(90)
                           .setMaxChunkSizeBytes(25));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                             .setOffset(50)
                             .setData(range(50, 75))
                             .build(),
            newChunk(Chunk.Type.DATA, transfer.getSessionId())
                .setOffset(75)
                .setData(range(75, 90))
                .build());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(90)
                           .setWindowEndOffset(140));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                             .setOffset(90)
                             .setData(range(90, 100))
                             .setRemainingBytes(0)
                             .build());

    assertThat(future.isDone()).isFalse();

    receiveWriteChunks(finalChunk(transfer.getSessionId(), Status.OK));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.COMPLETION_ACK, transfer.getSessionId()).build());

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_parametersContinue() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(321, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, TEST_DATA_100B.size());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(0)
                           .setWindowEndOffset(50)
                           .setMaxChunkSizeBytes(30)
                           .setMinDelayMicroseconds(1));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                             .setOffset(0)
                             .setData(range(0, 30))
                             .build(),
            newChunk(Chunk.Type.DATA, transfer.getSessionId())
                .setOffset(30)
                .setData(range(30, 50))
                .build());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_CONTINUE, transfer.getSessionId())
                           .setOffset(30)
                           .setWindowEndOffset(80));

    // Transfer doesn't roll back to offset 30 but instead continues sending up to 80.
    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                             .setOffset(50)
                             .setData(range(50, 80))
                             .build());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_CONTINUE, transfer.getSessionId())
                           .setOffset(80)
                           .setWindowEndOffset(130));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                             .setOffset(80)
                             .setData(range(80, 100))
                             .setRemainingBytes(0)
                             .build());

    assertThat(future.isDone()).isFalse();

    receiveWriteChunks(finalChunk(transfer.getSessionId(), Status.OK));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.COMPLETION_ACK, transfer.getSessionId()).build());

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_continuePacketWithWindowEndBeforeOffsetIsIgnored() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, TEST_DATA_100B.size());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(0)
                           .setWindowEndOffset(90)
                           .setMaxChunkSizeBytes(90)
                           .setMinDelayMicroseconds(1));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                             .setOffset(0)
                             .setData(range(0, 90))
                             .build());

    receiveWriteChunks(
        // This stale packet with a window end before the offset should be ignored.
        newChunk(Chunk.Type.PARAMETERS_CONTINUE, transfer.getSessionId())
            .setOffset(25)
            .setWindowEndOffset(50),
        // Start from an arbitrary offset before the current, but extend the window to the end.
        newChunk(Chunk.Type.PARAMETERS_CONTINUE, transfer.getSessionId())
            .setOffset(80)
            .setWindowEndOffset(100));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                             .setOffset(90)
                             .setData(range(90, 100))
                             .setRemainingBytes(0)
                             .build());

    receiveWriteChunks(finalChunk(transfer.getSessionId(), Status.OK));
    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.COMPLETION_ACK, transfer.getSessionId()).build());

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.
  }

  @Test
  public void write_progressCallbackIsCalled() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future =
        transferClient.write(123, TEST_DATA_100B.toByteArray(), progressCallback);
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, TEST_DATA_100B.size());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(0)
                           .setWindowEndOffset(90)
                           .setMaxChunkSizeBytes(30),
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
            .setOffset(50)
            .setWindowEndOffset(100),
        finalChunk(transfer.getSessionId(), Status.OK));

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
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, TEST_DATA_100B.size());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(100)
                           .setWindowEndOffset(140)
                           .setMaxChunkSizeBytes(25));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                             .setOffset(100)
                             .setRemainingBytes(0)
                             .build());
  }

  @Test
  public void write_multipleWithSameId_sequentially_successful() throws Exception {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    for (int i = 0; i < 3; ++i) {
      ListenableFuture<Void> future = transferClient.write(6, TEST_DATA_SHORT.toByteArray());
      WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

      performWriteStartHandshake(transfer, TEST_DATA_SHORT.size());

      receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                             .setOffset(0)
                             .setWindowEndOffset(50),
          finalChunk(transfer.getSessionId(), Status.OK));

      assertThat(lastChunks())
          .containsExactly(newChunk(Chunk.Type.DATA, transfer.getSessionId())
                               .setData(TEST_DATA_SHORT)
                               .setRemainingBytes(0)
                               .build(),
              newChunk(Chunk.Type.COMPLETION_ACK, transfer.getSessionId()).build());

      future.get();
    }
  }

  @Test
  public void write_multipleWithSameId_atSameTime_failsWithAlreadyExistsError() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> first = transferClient.write(123, TEST_DATA_SHORT.toByteArray());
    ListenableFuture<Void> second = transferClient.write(123, TEST_DATA_SHORT.toByteArray());

    assertThat(first.isDone()).isFalse();

    ExecutionException thrown = assertThrows(ExecutionException.class, second::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.ALREADY_EXISTS);
  }

  @Test
  public void write_sendErrorOnFirstPacket_failsImmediately() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ChannelOutputException exception = new ChannelOutputException("blah");
    rpcClient.setChannelOutputException(exception);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void write_serviceRequestsNoData_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(7, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, TEST_DATA_SHORT.size());

    receiveWriteChunks(
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId()).setOffset(0));

    assertThat(lastChunks())
        .containsExactly(finalChunk(transfer.getSessionId(), Status.INVALID_ARGUMENT));
    receiveWriteChunks(newChunk(Chunk.Type.COMPLETION_ACK, transfer.getSessionId()));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.INVALID_ARGUMENT);
  }

  @Test
  public void write_invalidOffset_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(7, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, TEST_DATA_100B.size());

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(101)
                           .setWindowEndOffset(141)
                           .setMaxChunkSizeBytes(25));

    assertThat(lastChunks())
        .containsExactly(finalChunk(transfer.getSessionId(), Status.OUT_OF_RANGE));
    receiveWriteChunks(newChunk(Chunk.Type.COMPLETION_ACK, transfer.getSessionId()));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.OUT_OF_RANGE);
  }

  @Test
  public void write_sendErrorOnLaterPacket_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(7, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, TEST_DATA_SHORT.size());

    ChannelOutputException exception = new ChannelOutputException("blah");
    rpcClient.setChannelOutputException(exception);

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(0)
                           .setWindowEndOffset(50)
                           .setMaxChunkSizeBytes(30));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);
    assertThat(thrown).hasCauseThat().hasCauseThat().isSameInstanceAs(exception);
  }

  @Test
  public void write_cancelFuture_abortsTransfer() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(7, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, TEST_DATA_100B.size());

    assertThat(future.cancel(true)).isTrue();
    assertThat(future.isCancelled()).isTrue();

    receiveWriteChunks(newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transfer.getSessionId())
                           .setOffset(0)
                           .setWindowEndOffset(50)
                           .setMaxChunkSizeBytes(50));

    assertThat(lastChunks()).contains(finalChunk(transfer.getSessionId(), Status.CANCELLED));
    receiveWriteChunks(newChunk(Chunk.Type.COMPLETION_ACK, transfer.getSessionId()));
  }

  @Test
  public void write_immediateTransferProtocolError_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    receiveWriteChunks(newChunk(Chunk.Type.COMPLETION, transfer.getSessionId())
                           .setResourceId(123)
                           .setStatus(Status.NOT_FOUND.ordinal()));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);

    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.NOT_FOUND);
  }

  @Test
  public void write_laterTransferProtocolError_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    performWriteStartHandshake(transfer, TEST_DATA_SHORT.size());

    receiveWriteChunks(finalChunk(transfer.getSessionId(), Status.NOT_FOUND));

    ExecutionException thrown = assertThrows(ExecutionException.class, future::get);
    assertThat(thrown).hasCauseThat().isInstanceOf(TransferError.class);

    assertThat(((TransferError) thrown.getCause()).status()).isEqualTo(Status.NOT_FOUND);
  }

  @Test
  public void write_rpcError_aborts() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());

    receiveWriteServerError(Status.NOT_FOUND);

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.INTERNAL);
  }

  @Test
  public void write_unknownVersion_invalidArgument() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    receiveWriteChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                           .setResourceId(2)
                           .setProtocolVersion(9));

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.INVALID_ARGUMENT);

    assertThat(lastChunks())
        .containsExactly(initialWriteChunk(transfer, TEST_DATA_SHORT.size()),
            finalChunk(transfer.getSessionId(), Status.INVALID_ARGUMENT));
  }

  @Test
  public void write_serverRespondsWithUnknownVersion_invalidArgument() {
    createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(2, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    assertThat(lastChunks()).containsExactly(initialWriteChunk(transfer, 100));

    receiveWriteChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                           .setResourceId(2)
                           .setProtocolVersion(600613));

    assertThat(lastChunks())
        .containsExactly(finalChunk(transfer.getSessionId(), Status.INVALID_ARGUMENT));

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.INVALID_ARGUMENT);
  }

  @Test
  public void write_timeoutAfterInitialChunk() {
    createTransferClientThatMayTimeOut(ProtocolVersion.VERSION_TWO);
    ListenableFuture<Void> future = transferClient.write(123, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    // Call future.get() without sending any server-side packets.
    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.DEADLINE_EXCEEDED);

    // Client should have resent the last chunk (the initial chunk in this case) for each timeout.
    assertThat(lastChunks())
        .containsExactly(initialWriteChunk(transfer, TEST_DATA_SHORT.size()), // initial
            initialWriteChunk(transfer, TEST_DATA_SHORT.size()), // retry 1
            initialWriteChunk(transfer, TEST_DATA_SHORT.size())); // retry 2
  }

  @Test
  public void write_timeoutAfterSingleChunk() {
    createTransferClientThatMayTimeOut(ProtocolVersion.VERSION_TWO);

    // Wait for two outgoing packets (Write RPC request and first chunk), then do the handshake.
    enqueueWriteChunks(2,
        newChunk(Chunk.Type.START_ACK, transferClient.getNextSessionIdForTest()).setResourceId(9),
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, transferClient.getNextSessionIdForTest())
            .setOffset(0)
            .setWindowEndOffset(90)
            .setMaxChunkSizeBytes(30));
    ListenableFuture<Void> future = transferClient.write(9, TEST_DATA_SHORT.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.DEADLINE_EXCEEDED);

    Chunk data = newChunk(Chunk.Type.DATA, transfer.getSessionId())
                     .setOffset(0)
                     .setData(TEST_DATA_SHORT)
                     .setRemainingBytes(0)
                     .build();
    assertThat(lastChunks())
        .containsExactly(initialWriteChunk(transfer, TEST_DATA_SHORT.size()), // initial
            newChunk(Chunk.Type.START_ACK_CONFIRMATION, transfer.getSessionId())
                .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal())
                .setRemainingBytes(TEST_DATA_SHORT.size())
                .build(),
            data, // data chunk
            data, // retry 1
            data); // retry 2
  }

  @Test
  public void write_timeoutAndRecoverDuringHandshakes() throws Exception {
    createTransferClientThatMayTimeOut(ProtocolVersion.VERSION_TWO);
    assertThat(MAX_RETRIES).isEqualTo(2); // This test assumes 2 retries

    int id = transferClient.getNextSessionIdForTest();

    // Wait for four outgoing packets (Write RPC request and START chunk + retry), then handshake.
    enqueueWriteChunks(3,
        newChunk(Chunk.Type.START_ACK, id)
            .setResourceId(5)
            .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    // Wait for start ack confirmation + 2 retries, then request three packets.
    enqueueWriteChunks(3,
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id)
            .setOffset(0)
            .setWindowEndOffset(60)
            .setMaxChunkSizeBytes(20));

    // After two packets, request the remainder of the packets.
    enqueueWriteChunks(
        2, newChunk(Chunk.Type.PARAMETERS_CONTINUE, id).setOffset(20).setWindowEndOffset(200));

    // Wait for last 3 data packets, then 2 final packet retries.
    enqueueWriteChunks(5,
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id)
            .setOffset(80)
            .setWindowEndOffset(200)
            .setMaxChunkSizeBytes(20),
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id)
            .setOffset(80)
            .setWindowEndOffset(200)
            .setMaxChunkSizeBytes(20));

    // After the retry, confirm completed multiple times; additional packets should be dropped
    enqueueWriteChunks(1,
        newChunk(Chunk.Type.COMPLETION, id).setStatus(Status.OK.code()),
        newChunk(Chunk.Type.COMPLETION, id).setStatus(Status.OK.code()),
        newChunk(Chunk.Type.COMPLETION, id).setStatus(Status.OK.code()),
        newChunk(Chunk.Type.COMPLETION, id).setStatus(Status.OK.code()));

    ListenableFuture<Void> future = transferClient.write(5, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.

    final Chunk startAckConfirmation =
        newChunk(Chunk.Type.START_ACK_CONFIRMATION, id)
            .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal())
            .setRemainingBytes(TEST_DATA_100B.size())
            .build();

    assertThat(lastChunks())
        .containsExactly(
            // initial handshake with retries
            initialWriteChunk(transfer, TEST_DATA_100B.size()),
            initialWriteChunk(transfer, TEST_DATA_100B.size()),
            startAckConfirmation,
            startAckConfirmation,
            startAckConfirmation,
            // send all data
            dataChunk(id, TEST_DATA_100B, 0, 20), // data 0-20
            dataChunk(id, TEST_DATA_100B, 20, 40), // data 20-40
            dataChunk(id, TEST_DATA_100B, 40, 60), // data 40-60
            dataChunk(id, TEST_DATA_100B, 60, 80), // data 60-80
            dataChunk(id, TEST_DATA_100B, 80, 100), // data 80-100 (final)
            // retry last packet two times
            dataChunk(id, TEST_DATA_100B, 80, 100), // data 80-100 (final)
            dataChunk(id, TEST_DATA_100B, 80, 100), // data 80-100 (final)
            // respond to two PARAMETERS_RETRANSMIT packets
            dataChunk(id, TEST_DATA_100B, 80, 100), // data 80-100 (final)
            dataChunk(id, TEST_DATA_100B, 80, 100), // data 80-100 (final)
            // respond to OK packet
            newChunk(Chunk.Type.COMPLETION_ACK, id).build());
  }

  @Test
  public void write_multipleTimeoutsAndRecoveries() throws Exception {
    createTransferClientThatMayTimeOut(ProtocolVersion.VERSION_TWO);
    assertThat(MAX_RETRIES).isEqualTo(2); // This test assumes 2 retries

    int id = transferClient.getNextSessionIdForTest();

    // Wait for two outgoing packets (Write RPC request and START chunk), then do the handshake.
    enqueueWriteChunks(2,
        newChunk(Chunk.Type.START_ACK, id)
            .setResourceId(5)
            .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    // Request two packets.
    enqueueWriteChunks(1,
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id)
            .setOffset(0)
            .setWindowEndOffset(40)
            .setMaxChunkSizeBytes(20));

    // After the second retry, send more transfer parameters
    enqueueWriteChunks(4,
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id)
            .setOffset(40)
            .setWindowEndOffset(120)
            .setMaxChunkSizeBytes(40));

    // After the first retry, send more transfer parameters
    enqueueWriteChunks(3,
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id)
            .setOffset(80)
            .setWindowEndOffset(160)
            .setMaxChunkSizeBytes(10));

    // After the second retry, confirm completed
    enqueueWriteChunks(4, newChunk(Chunk.Type.COMPLETION, id).setStatus(Status.OK.code()));
    enqueueWriteChunks(1, newChunk(Chunk.Type.COMPLETION_ACK, id));

    ListenableFuture<Void> future = transferClient.write(5, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    assertThat(future.get()).isNull(); // Ensure that no exceptions are thrown.

    assertThat(lastChunks())
        .containsExactly(
            // initial handshake
            initialWriteChunk(transfer, TEST_DATA_100B.size()),
            newChunk(Chunk.Type.START_ACK_CONFIRMATION, id)
                .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal())
                .setRemainingBytes(TEST_DATA_100B.size())
                .build(),
            // after 2, receive parameters: 40 from 0 by 20
            dataChunk(id, TEST_DATA_100B, 0, 20), // data 0-20
            dataChunk(id, TEST_DATA_100B, 20, 40), // data 20-40
            dataChunk(id, TEST_DATA_100B, 20, 40), // retry 1
            dataChunk(id, TEST_DATA_100B, 20, 40), // retry 2
            // after 4, receive parameters: 80 from 40 by 40
            dataChunk(id, TEST_DATA_100B, 40, 80), // data 40-80
            dataChunk(id, TEST_DATA_100B, 80, 100), // data 80-100
            dataChunk(id, TEST_DATA_100B, 80, 100), // retry 1
            // after 3, receive parameters: 80 from 80 by 10
            dataChunk(id, TEST_DATA_100B, 80, 90), // data 80-90
            dataChunk(id, TEST_DATA_100B, 90, 100), // data 90-100
            dataChunk(id, TEST_DATA_100B, 90, 100), // retry 1
            dataChunk(id, TEST_DATA_100B, 90, 100), // retry 2
            // after 4, receive final OK
            newChunk(Chunk.Type.COMPLETION_ACK, id).build());
  }

  @Test
  public void write_maxLifetimeRetries() {
    createTransferClientThatMayTimeOut(ProtocolVersion.VERSION_TWO, 5);
    assertThat(MAX_RETRIES).isEqualTo(2); // This test assumes 2 retries

    int id = transferClient.getNextSessionIdForTest();

    // Wait for four outgoing packets (Write RPC request and START chunk + 2 retries)
    enqueueWriteChunks(4, // 2 retries
        newChunk(Chunk.Type.START_ACK, id)
            .setResourceId(5)
            .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    // Wait for start ack confirmation + 2 retries, then request three packets.
    enqueueWriteChunks(3, // 2 retries
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id)
            .setOffset(0)
            .setWindowEndOffset(60)
            .setMaxChunkSizeBytes(20));

    // After 3 data packets, wait for two more retries, which should put this over the retry limit.
    enqueueWriteChunks(5, // 2 retries
        newChunk(Chunk.Type.PARAMETERS_RETRANSMIT, id) // This packet should be ignored
            .setOffset(80)
            .setWindowEndOffset(200)
            .setMaxChunkSizeBytes(20));

    ListenableFuture<Void> future = transferClient.write(5, TEST_DATA_100B.toByteArray());
    WriteTransfer transfer = transferClient.getWriteTransferForTest(future);

    ExecutionException exception = assertThrows(ExecutionException.class, future::get);
    assertThat(((TransferError) exception.getCause()).status()).isEqualTo(Status.DEADLINE_EXCEEDED);

    final Chunk startAckConfirmation =
        newChunk(Chunk.Type.START_ACK_CONFIRMATION, id)
            .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal())
            .setRemainingBytes(TEST_DATA_100B.size())
            .build();

    assertThat(lastChunks())
        .containsExactly(
            // initial chunk and 2 retries
            initialWriteChunk(transfer, TEST_DATA_100B.size()),
            initialWriteChunk(transfer, TEST_DATA_100B.size()),
            initialWriteChunk(transfer, TEST_DATA_100B.size()),
            // START_ACK_CONFIRMATION and 2 retries
            startAckConfirmation,
            startAckConfirmation,
            startAckConfirmation,
            // send all data
            dataChunk(id, TEST_DATA_100B, 0, 20), // data 0-20
            dataChunk(id, TEST_DATA_100B, 20, 40), // data 20-40
            dataChunk(id, TEST_DATA_100B, 40, 60), // data 40-60
            // last packet retry, then hit the lifetime retry limit and abort
            dataChunk(id, TEST_DATA_100B, 40, 60)); // data 40-60
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

  private static Chunk.Builder newLegacyChunk(Chunk.Type type, int transferId) {
    return Chunk.newBuilder().setType(type).setTransferId(transferId);
  }

  private static Chunk.Builder newChunk(Chunk.Type type, int sessionId) {
    return Chunk.newBuilder().setType(type).setSessionId(sessionId);
  }

  private static Chunk initialReadChunk(ReadTransfer transfer) {
    Chunk.Builder chunk =
        newLegacyChunk(Chunk.Type.START, transfer.getResourceId())
            .setResourceId(transfer.getResourceId())
            .setPendingBytes(transfer.getParametersForTest().maxPendingBytes())
            .setWindowEndOffset(transfer.getParametersForTest().maxPendingBytes())
            .setMaxChunkSizeBytes(transfer.getParametersForTest().maxChunkSizeBytes())
            .setOffset(0);
    if (transfer.getDesiredProtocolVersion() != ProtocolVersion.LEGACY) {
      chunk.setProtocolVersion(transfer.getDesiredProtocolVersion().ordinal());
      chunk.setDesiredSessionId(transfer.getSessionId());
    }
    if (transfer.getParametersForTest().chunkDelayMicroseconds() > 0) {
      chunk.setMinDelayMicroseconds(transfer.getParametersForTest().chunkDelayMicroseconds());
    }
    return chunk.build();
  }

  private static Chunk initialLegacyReadChunk(int resourceId) {
    return initialLegacyReadChunk(resourceId, TRANSFER_PARAMETERS);
  }

  private static Chunk initialLegacyReadChunk(int resourceId, TransferParameters params) {
    Chunk.Builder chunk = newLegacyChunk(Chunk.Type.START, resourceId)
                              .setResourceId(resourceId)
                              .setPendingBytes(params.maxPendingBytes())
                              .setWindowEndOffset(params.maxPendingBytes())
                              .setMaxChunkSizeBytes(params.maxChunkSizeBytes())
                              .setOffset(0);
    if (params.chunkDelayMicroseconds() > 0) {
      chunk.setMinDelayMicroseconds(params.chunkDelayMicroseconds());
    }
    return chunk.build();
  }

  private static Chunk readStartAckConfirmation(int sessionId, TransferParameters params) {
    Chunk.Builder chunk = newChunk(Chunk.Type.START_ACK_CONFIRMATION, sessionId)
                              .setWindowEndOffset(params.maxPendingBytes())
                              .setMaxChunkSizeBytes(params.maxChunkSizeBytes())
                              .setOffset(0)
                              .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal());
    if (params.chunkDelayMicroseconds() > 0) {
      chunk.setMinDelayMicroseconds(params.chunkDelayMicroseconds());
    }
    return chunk.build();
  }

  private static Chunk initialLegacyWriteChunk(int resourceId, int size) {
    return newLegacyChunk(Chunk.Type.START, resourceId)
        .setResourceId(resourceId)
        .setRemainingBytes(size)
        .build();
  }

  private static Chunk initialWriteChunk(WriteTransfer transfer, int size) {
    Chunk.Builder chunk = newLegacyChunk(Chunk.Type.START, transfer.getResourceId())
                              .setResourceId(transfer.getResourceId())
                              .setRemainingBytes(size);
    if (transfer.getDesiredProtocolVersion() != ProtocolVersion.LEGACY) {
      chunk.setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal());
      chunk.setDesiredSessionId(transfer.getSessionId());
    }
    return chunk.build();
  }

  private static Chunk legacyFinalChunk(int sessionId, Status status) {
    return newLegacyChunk(Chunk.Type.COMPLETION, sessionId).setStatus(status.code()).build();
  }

  private static Chunk finalChunk(int sessionId, Status status) {
    return newChunk(Chunk.Type.COMPLETION, sessionId).setStatus(status.code()).build();
  }

  private static Chunk legacyDataChunk(int sessionId, ByteString data, int start, int end) {
    if (start < 0 || end > data.size()) {
      throw new IndexOutOfBoundsException("Invalid start or end");
    }

    Chunk.Builder chunk = newLegacyChunk(Chunk.Type.DATA, sessionId)
                              .setOffset(start)
                              .setData(data.substring(start, end));
    if (end == data.size()) {
      chunk.setRemainingBytes(0);
    }
    return chunk.build();
  }

  private static Chunk dataChunk(int sessionId, ByteString data, int start, int end) {
    if (start < 0 || end > data.size()) {
      throw new IndexOutOfBoundsException("Invalid start or end");
    }

    Chunk.Builder chunk =
        newChunk(Chunk.Type.DATA, sessionId).setOffset(start).setData(data.substring(start, end));
    if (end == data.size()) {
      chunk.setRemainingBytes(0);
    }
    return chunk.build();
  }

  /** Runs an action */
  private void syncWithTransferThread(Runnable action) {
    transferClient.waitUntilEventsAreProcessedForTest();
    action.run();
    transferClient.waitUntilEventsAreProcessedForTest();
  }

  private void receiveReadServerError(Status status) {
    syncWithTransferThread(() -> rpcClient.receiveServerError(SERVICE, "Read", status));
  }

  private void receiveWriteServerError(Status status) {
    syncWithTransferThread(() -> rpcClient.receiveServerError(SERVICE, "Write", status));
  }

  private void receiveReadChunks(ChunkOrBuilder... chunks) {
    for (ChunkOrBuilder chunk : chunks) {
      syncWithTransferThread(() -> rpcClient.receiveServerStream(SERVICE, "Read", chunk));
    }
  }

  private void receiveWriteChunks(ChunkOrBuilder... chunks) {
    for (ChunkOrBuilder chunk : chunks) {
      syncWithTransferThread(() -> rpcClient.receiveServerStream(SERVICE, "Write", chunk));
    }
  }

  private void performReadStartHandshake(ReadTransfer transfer) {
    assertThat(lastChunks()).containsExactly(initialReadChunk(transfer));

    receiveReadChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                          .setResourceId(transfer.getResourceId())
                          .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    assertThat(lastChunks())
        .containsExactly(
            readStartAckConfirmation(transfer.getSessionId(), transfer.getParametersForTest()));
  }

  private void performReadCompletionHandshake(int sessionId, Status status) {
    assertThat(lastChunks())
        .containsExactly(Chunk.newBuilder()
                             .setType(Chunk.Type.COMPLETION)
                             .setSessionId(sessionId)
                             .setStatus(status.ordinal())
                             .build());

    receiveReadChunks(newChunk(Chunk.Type.COMPLETION_ACK, sessionId));
  }

  private void performWriteStartHandshake(WriteTransfer transfer, int dataSize) {
    assertThat(lastChunks()).containsExactly(initialWriteChunk(transfer, dataSize));

    receiveWriteChunks(newChunk(Chunk.Type.START_ACK, transfer.getSessionId())
                           .setResourceId(transfer.getResourceId())
                           .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal()));

    assertThat(lastChunks())
        .containsExactly(newChunk(Chunk.Type.START_ACK_CONFIRMATION, transfer.getSessionId())
                             .setProtocolVersion(ProtocolVersion.VERSION_TWO.ordinal())
                             .setRemainingBytes(dataSize)
                             .build());
  }

  /** Receive these read chunks after a chunk is sent. */
  private void enqueueReadChunks(int afterPackets, Chunk.Builder... chunks) {
    syncWithTransferThread(
        () -> rpcClient.enqueueServerStream(SERVICE, "Read", afterPackets, chunks));
  }

  /** Receive these write chunks after a chunk is sent. */
  private void enqueueWriteChunks(int afterPackets, Chunk.Builder... chunks) {
    syncWithTransferThread(
        () -> rpcClient.enqueueServerStream(SERVICE, "Write", afterPackets, chunks));
  }

  private List<Chunk> lastChunks() {
    transferClient.waitUntilEventsAreProcessedForTest();
    return rpcClient.lastClientStreams(Chunk.class);
  }

  private void createTransferClientThatMayTimeOut(ProtocolVersion version) {
    createTransferClientThatMayTimeOut(version, Integer.MAX_VALUE);
  }

  private void createTransferClientThatMayTimeOut(ProtocolVersion version, int maxLifetimeRetries) {
    createTransferClient(
        version, 1, 1, maxLifetimeRetries, TransferEventHandler::runForTestsThatMustTimeOut);
  }

  private void createTransferClientForTransferThatWillNotTimeOut(ProtocolVersion version) {
    createTransferClient(version, 60000, 60000, Integer.MAX_VALUE, TransferEventHandler::run);
  }

  private void createTransferClient(ProtocolVersion version,
      int transferTimeoutMillis,
      int initialTransferTimeoutMillis,
      int maxLifetimeRetries,
      Consumer<TransferEventHandler> eventHandlerFunction) {
    if (transferClient != null) {
      throw new AssertionError("createTransferClient must only be called once!");
    }
    transferClient = new TransferClient(rpcClient.client().method(CHANNEL_ID, SERVICE + "/Read"),
        rpcClient.client().method(CHANNEL_ID, SERVICE + "/Write"),
        TransferTimeoutSettings.builder()
            .setTimeoutMillis(transferTimeoutMillis)
            .setInitialTimeoutMillis(initialTransferTimeoutMillis)
            .setMaxRetries(MAX_RETRIES)
            .setMaxLifetimeRetries(maxLifetimeRetries)
            .build(),
        ()
            -> this.shouldAbortFlag,
        eventHandlerFunction);
    transferClient.setProtocolVersion(version);
  }
}
