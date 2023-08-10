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
#pragma once

/// @file
/// This file defines types related to a multi-producer, single-consumer stream.
///
/// The single readers must be constructed in place, while writers can be moved.
/// A reader and writer may be connected using `CreateMpscStream()`. Additional
/// writers may be connected by copying a previously connected writer.
///
/// Example:
///
/// @code{.cpp}
///    void WriteThreadRoutine(void* arg) {
///      auto *writer = static_cast<MpscWriter *>(arg);
///      ConstByteSpan data = GenerateSomeData();
///      Status status = writer->Write(data);
///      ...
///    }
///    ...
///    MpscReader reader;
///    MpscWriter writer;
///    CreateMpscStream(reader, writer);
///    thread::Thread t(MakeThreadOptions(), WriteThreadRoutine, &writer);
///    std::byte buffer[kBufSize];
///    if (auto status = reader.Read(ByteSpan(buffer)); status.ok()) {
///      ProcessSomeData(buffer);
///    }
/// @endcode
///
/// See the `MpscReader::ReadAll()` for additional examples.
///
/// The types in the files are designed to be used across different threads,
/// but are not completely thread-safe. Data must only be written by an
/// MpscWriter using a single thread, and data must only be read by an
/// MpscReader using a single thread. In other words, multiple calls to
/// `Write()` must not be made concurrently, and multiple calls to `Read()` and
/// `ReadAll()` must not be made concurrently. Calls to other methods, e.g.
/// `Close()`, are thread-safe and may be made from any thread.

#include <cstddef>

#include "pw_bytes/span.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/intrusive_list.h"
#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"
#include "pw_stream/stream.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_sync/timed_thread_notification.h"

namespace pw::stream {

// Forward declaration.
class MpscReader;
class MpscWriter;

/// Creates a multi-producer, single consumer stream.
///
/// This method creates a stream by associating a reader and writer. Both are
/// reset before being connected. This is the only way to connect a reader.
/// Additional writers may be connected by copying the given writer after it is
/// connected.
///
/// This method is thread-safe with respect to other MpscReader and MpscWriter
/// methods. It is not thread-safe with respect to itself, i.e. callers must
/// not make concurrent calls to `CreateMpscStream()` from different threads
/// with the same objects.
///
/// @param[out]   reader  The reader to connect.
/// @param[out]   writer  The writer to connect.
void CreateMpscStream(MpscReader& reader, MpscWriter& writer);

/// Writer for a multi-producer, single consumer stream.
///
/// This class has a default constructor that only produces disconnected
/// writers. To connect writers, use `CreateMpscStream()`. Additional connected
/// writers can be created by copying an existing one.
///
/// Each thread should have its own dedicated writer. This class is thread-safe
/// with respect to the reader, but not with respect to itself. In particular,
/// attempting to call `Write()` concurrently on different threads may cause
/// result in a failure.
class MpscWriter : public NonSeekableWriter,
                   public IntrusiveList<MpscWriter>::Item {
 public:
  using duration = std::optional<chrono::SystemClock::duration>;

  /// A per-writer thread notification that can be added to a reader's list.
  ///
  /// The reader maintains a list of outstanding requests to write data. As
  /// data is read, and space to write data becomes available, it uses these
  /// requests to signal the waiting the writers.
  struct Request : public IntrusiveList<Request>::Item {
    sync::TimedThreadNotification notification;
    using IntrusiveList<Request>::Item::unlisted;
  };

  MpscWriter() = default;
  MpscWriter(const MpscWriter& other);
  MpscWriter& operator=(const MpscWriter& other);
  MpscWriter(MpscWriter&& other);
  MpscWriter& operator=(MpscWriter&& other);
  ~MpscWriter() override;

  /// Returns whether this object is connected to a reader.
  bool connected() const PW_LOCKS_EXCLUDED(mutex_);

  /// Indicates how much data was sent in the last call to `Write()`.
  size_t last_write() const PW_LOCKS_EXCLUDED(mutex_);

  /// Returns the optional maximum time elapsed before a `Write()` fails.
  const duration& timeout() const PW_LOCKS_EXCLUDED(mutex_);

  /// Set the timeout for writing to this stream.
  ///
  /// After setting a timeout, if the given duration elapses while making a call
  /// to `Write()`, @pw_status{RESOURCE_EXHAUSTED} will be returned. If desired,
  /// a timeout should be set before calling `Write()`. Setting a timeout when a
  /// writer is awaiting notification from a reader will not affect the duration
  /// of that wait.
  ///
  /// Note that setting a write timeout makes partial writes possible. For
  /// example, if a call to `Write()` of some length corresponds to 2 calls to
  /// `Read()` of half that length with an sufficient delay between the calls
  /// will result in the first half being written and read, but not the second.
  /// This differs from `Stream::Write()` which stipulates that no data is
  /// written on failure. If this happens, the length of the data written can be
  /// retrieved using `last_write()`.
  ///
  /// Generally, callers should use one of three approaches:
  ///  1. Do not set a write timeout, and let writers block arbitrarily long
  ///     until space is available or the reader is disconnected.
  ///  2. Use only a single writer, and use `last_write()` to resend data.
  ///  3. Structure the data being sent so that the reader can always read
  ///     complete messages and avoid blocking or performing complex work
  ///     mid-message.
  ///
  /// @param[in]  timeout   The duration to wait before returning an error.
  void SetTimeout(const duration& timeout) PW_LOCKS_EXCLUDED(mutex_);

  /// Sets the maximum amount that can be written by this writer.
  ///
  /// By default, writers can write an unlimited amount of data. This method can
  /// be used to set a limit, or remove it by providing a value of
  /// Stream::kUnlimited.
  ///
  /// If a limit is set, the writer will automatically close once it has written
  /// that much data. The current number of bytes remaining until the limit is
  /// reached can be retrieved using `ConservativeWriteLimit()`.
  ///
  /// @param[in]  limit   The maximum amount that can be written by this writer.
  void SetLimit(size_t limit) PW_LOCKS_EXCLUDED(mutex_);

  /// Disconnects this writer from its reader.
  ///
  /// This method does nothing if the writer is not connected.
  void Close() PW_LOCKS_EXCLUDED(mutex_);

 private:
  // The factory method is allowed to directly modify a writer to connect it
  // to the reader.
  friend void CreateMpscStream(MpscReader&, MpscWriter&);

  /// @copydoc Stream::ConservativeLimit
  size_t ConservativeLimit(LimitType type) const override;

  /// @copydoc Stream::DoWrite
  ///
  /// This method is *not* thread-safe with respect to itself. If multiple
  /// threads attempt to write concurrently using the same writer, those calls
  /// may fail. Instead, each thread should have its own writer.
  ///
  /// @pre No other thread has called `Write()` on this object.
  Status DoWrite(ConstByteSpan data) override;

  /// Locked implementation of `Close()`.
  void CloseLocked() PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable sync::Mutex mutex_;
  MpscReader* reader_ PW_GUARDED_BY(mutex_) = nullptr;
  size_t limit_ PW_GUARDED_BY(mutex_) = kUnlimited;
  Request write_request_;
  duration timeout_ PW_GUARDED_BY(mutex_);
  size_t last_write_ PW_GUARDED_BY(mutex_) = 0;
};

/// Reader of a multi-producer, single-consumer stream.
///
/// The reader manages 3 aspects of the stream:
///   * The storage used to hold written data that is to be read.
///   * The list of connected writers.
///   * Accounting for how much data has and can be written.
///
/// This class has a default constructor that can only produce a disconnected
/// reader. To connect a reader, use `CreateMpscStream()`.
class MpscReader : public NonSeekableReader {
 public:
  using duration = std::optional<chrono::SystemClock::duration>;

  MpscReader();
  ~MpscReader() override;

  /// Returns whether this object has any connected writers.
  bool connected() const PW_LOCKS_EXCLUDED(mutex_);

  /// Set the timeout for reading from this stream.
  ///
  /// After setting a timeout, if the given duration elapses while making a call
  /// to `Read()`, RESOURCE_EXHAUSTED will be returned. If desired, a timeout
  /// should be set before calling `Read()` or `ReadAll()`. Setting a timeout
  /// when a reader is awaiting notification from a writer will not affect the
  /// duration of that wait. `ReadUntilClose()` ignores timeouts entirely.
  ///
  /// @param[in]  timeout   The duration to wait before returning an error.
  void SetTimeout(const duration& timeout) PW_LOCKS_EXCLUDED(mutex_);

  /// Associates the reader with storage to buffer written data to be read.
  ///
  /// If desired, callers can use this method to buffer written data. This can
  /// improve writer performance by allowing calls to `WriteData()` to avoid
  /// waiting for the reader, albeit at the cost of increased memory. This can
  /// be useful when the reader needs time to process the data it reads, or when
  /// the volume of writes varies over time, i.e. is "bursty".
  ///
  /// The reader does not take ownership of the storage, which must be valid
  /// until a call to the destructor or another call to `SetBuffer()`.
  ///
  /// @param[in]  buffer  A view to the storage.
  void SetBuffer(ByteSpan buffer) PW_LOCKS_EXCLUDED(mutex_);

  /// @fn ReadAll
  /// Reads data in a loop and passes it to a provided callback.
  ///
  /// This will read continuously until all connected writers close.
  ///
  /// Example usage:
  ///
  /// @code(.cpp}
  ///    MpscReader reader;
  ///    MpscWriter writer;
  ///    MpscStreamCreate(reader, writer);
  ///    thread::Thread t(MakeThreadOptions(), [] (void*arg) {
  ///      auto *writer = static_cast<MpscWriter *>(arg);
  ///      writer->Write(GenerateSomeData()).IgnoreError();
  ///    }, &writer);
  ///    auto status = reader.ReadAll([] (ConstByteSpan data) {
  ///      return ProcessSomeData();
  ///    });
  ///    t.join();
  /// @endcode
  ///
  /// @param[in]  callback  A callable object to invoke on data as it is read.
  /// @retval     OK                  Successfully read until writers closed.
  /// @retval     FAILED_PRECONDITION The object does not have a buffer.
  /// @retval     RESOURCE_EXHAUSTED  Timed out when reading data. This can only
  ///                                 occur if a timeout has been set.
  /// @retval     Any other error as returned by the callback.
  using ReadAllCallback = Function<Status(ConstByteSpan data)>;
  Status ReadAll(ReadAllCallback callback) PW_LOCKS_EXCLUDED(mutex_);

  /// Disconnects all writers and drops any unread data.
  void Close() PW_LOCKS_EXCLUDED(mutex_);

 private:
  // The factory method is allowed to directly modify the reader to connect it
  // to a writer.
  friend void CreateMpscStream(MpscReader&, MpscWriter&);

  // The writer is allowed to call directly into the reader to:
  //  * Add/remove itself to the reader's list of writer.
  //  * Request space to write data, and to write to that space.
  friend class MpscWriter;

  /// @fn IncreaseLimit
  /// @fn IncreaseLimitLocked
  /// Increases the number of remaining bytes to be written.
  ///
  /// Used by `MpscWriter::SetLimit()` and `MpscWriter::WriteData()`.
  ///
  /// @param[in]  delta   How much to increase the number of remaining bytes.
  void IncreaseLimit(size_t delta) PW_LOCKS_EXCLUDED(mutex_);
  void IncreaseLimitLocked(size_t delta) PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  /// @fn DecreaseLimit
  /// @fn DecreaseLimitLocked
  /// Decreases the number of remaining bytes to be written.
  ///
  /// Used by `MpscWriter::SetLimit()` and `MpscWriter::RemoveWriter()`.
  ///
  /// @param[in]  delta   How much to decrease the number of remaining bytes.
  void DecreaseLimit(size_t delta) PW_LOCKS_EXCLUDED(mutex_);
  void DecreaseLimitLocked(size_t delta) PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  /// @copydoc Stream::ConservativeLimit
  size_t ConservativeLimit(Stream::LimitType type) const override
      PW_LOCKS_EXCLUDED(mutex_);

  /// Adds the write request to the reader's list of pending requests.
  ///
  /// Used by `MpscWriter::WriteData()`.
  ///
  /// @param[in]  write_request   A writer's request object.
  void RequestWrite(MpscWriter::Request& write_request)
      PW_LOCKS_EXCLUDED(mutex_);

  /// Checks if a writer can write data, and signals it if so.
  ///
  /// A reader may signal a writer because:
  ///   * Space to write data has become available.
  ///   * The queue of write requests has changed.
  ///   * The reader is closing. `WriteData()` will return OUT_OF_RANGE.
  void CheckWriteableLocked() PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  /// Adds data from a writer to the buffer to be read.
  ///
  /// @param[in]  data            The data to be written.
  /// @param[in]  limit           The writer's current write limit.
  ///
  /// @retval OK                  Data was written to the buffer.
  /// @retval RESOURCE_EXHAUSTED  Buffer has insufficent space for data.
  /// @retval OUT_OF_RANGE        Stream is shut down or closed.
  StatusWithSize WriteData(ConstByteSpan data, size_t limit)
      PW_LOCKS_EXCLUDED(mutex_);

  /// @fn CompleteWrite
  /// @fn CompleteWriteLocked
  /// Removes the write request from the reader's list of pending requests.
  ///
  /// Used by `MpscWriter::WriteData()` and `MpscWriter::CloseLocked()`.
  ///
  /// @param[in]  write_request   A writer's request object.
  void CompleteWrite(MpscWriter::Request& write_request)
      PW_LOCKS_EXCLUDED(mutex_);
  void CompleteWriteLocked(MpscWriter::Request& write_request)
      PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  /// @copydoc Stream::DoRead
  StatusWithSize DoRead(ByteSpan destination) override
      PW_LOCKS_EXCLUDED(mutex_);

  // Locked implementations.

  mutable sync::Mutex mutex_;
  IntrusiveList<MpscWriter> writers_ PW_GUARDED_BY(mutex_);
  IntrusiveList<MpscWriter::Request> write_requests_ PW_GUARDED_BY(mutex_);
  IntrusiveList<MpscWriter::Request>::iterator last_request_
      PW_GUARDED_BY(mutex_);

  size_t num_unlimited_ PW_GUARDED_BY(mutex_) = 0;
  size_t limit_ PW_GUARDED_BY(mutex_) = 0;

  bool reading_ PW_GUARDED_BY(mutex_) = false;
  sync::TimedThreadNotification readable_;
  sync::ThreadNotification closeable_;
  duration timeout_ PW_GUARDED_BY(mutex_);

  ByteSpan destination_ PW_GUARDED_BY(mutex_);
  size_t written_ PW_GUARDED_BY(mutex_) = 0;

  ByteSpan buffer_ PW_GUARDED_BY(mutex_);
  size_t offset_ PW_GUARDED_BY(mutex_) = 0;
  size_t length_ PW_GUARDED_BY(mutex_) = 0;
};

/// Reader for a multi-producer, single consumer stream.
///
/// This class includes an explicitly-sized buffer. It has a default constructor
/// that can only produce a disconnected reader. To connect a reader, use
/// `CreateMpscStream()`.
template <size_t kCapacity>
class BufferedMpscReader : public MpscReader {
 public:
  BufferedMpscReader() { SetBuffer(buffer_); }

 private:
  std::array<std::byte, kCapacity> buffer_;
};

}  // namespace pw::stream
