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

#include "pw_stream/mpsc_stream.h"

#include <cstring>

#include "pw_assert/check.h"

namespace pw::stream {
namespace {

// Wait to receive a thread notification with an optional timeout.
bool Await(sync::TimedThreadNotification& notification,
           const std::optional<chrono::SystemClock::duration>& timeout) {
  if (timeout.has_value()) {
    return notification.try_acquire_for(*timeout);
  }
  // Block indefinitely.
  notification.acquire();
  return true;
}

}  // namespace

void CreateMpscStream(MpscReader& reader, MpscWriter& writer) {
  reader.Close();
  std::lock_guard rlock(reader.mutex_);
  PW_CHECK(reader.writers_.empty());
  std::lock_guard wlock(writer.mutex_);
  writer.CloseLocked();
  reader.writers_.push_front(writer);
  reader.IncreaseLimitLocked(Stream::kUnlimited);
  writer.reader_ = &reader;
}

////////////////////////////////////////////////////////////////////////////////
// MpscWriter methods.

MpscWriter::MpscWriter(const MpscWriter& other) { *this = other; }

MpscWriter& MpscWriter::operator=(const MpscWriter& other) {
  Close();

  // Read the other object's internal state. Avoid holding both locks at once.
  other.mutex_.lock();
  MpscReader* reader = other.reader_;
  duration timeout = other.timeout_;
  size_t limit = other.limit_;
  size_t last_write = other.last_write_;
  other.mutex_.unlock();

  // Now update this object with the other's state.
  mutex_.lock();
  reader_ = reader;
  timeout_ = timeout;
  limit_ = limit;
  last_write_ = last_write;
  mutex_.unlock();

  // Add the writer to the reader outside the lock. If the reader was closed
  // concurrently, this will close the writer.
  if (reader != nullptr) {
    std::lock_guard lock(reader->mutex_);
    reader->writers_.push_front(*this);
    reader->IncreaseLimitLocked(limit);
  }
  return *this;
}

MpscWriter::MpscWriter(MpscWriter&& other) : MpscWriter() {
  *this = std::move(other);
}

MpscWriter& MpscWriter::operator=(MpscWriter&& other) {
  *this = other;
  other.Close();
  return *this;
}

MpscWriter::~MpscWriter() { Close(); }

bool MpscWriter::connected() const {
  std::lock_guard lock(mutex_);
  return reader_ != nullptr;
}

size_t MpscWriter::last_write() const {
  std::lock_guard lock(mutex_);
  return last_write_;
}

void MpscWriter::SetTimeout(const duration& timeout) {
  std::lock_guard lock(mutex_);
  timeout_ = timeout;
}

void MpscWriter::SetLimit(size_t limit) {
  std::lock_guard lock(mutex_);
  if (reader_) {
    reader_->DecreaseLimit(limit_);
    reader_->IncreaseLimit(limit);
  }
  limit_ = limit;
  if (limit_ == 0) {
    CloseLocked();
  }
}

size_t MpscWriter::ConservativeLimit(LimitType type) const {
  std::lock_guard lock(mutex_);
  return reader_ != nullptr && type == LimitType::kWrite ? limit_ : 0;
}

Status MpscWriter::DoWrite(ConstByteSpan data) {
  // Check some conditions to see if an early exit is possible.
  if (data.empty()) {
    return OkStatus();
  }
  std::lock_guard lock(mutex_);
  if (reader_ == nullptr) {
    return Status::OutOfRange();
  }
  if (limit_ < data.size()) {
    return Status::ResourceExhausted();
  }
  if (!write_request_.unlisted()) {
    return Status::FailedPrecondition();
  }
  // Subscribe to the reader. This will enqueue this object's write request,
  // which will be used to notify the writer when the reader has space available
  // or has closed.
  reader_->RequestWrite(write_request_);
  last_write_ = 0;

  Status status;
  while (!data.empty()) {
    // Wait to be notified by the reader.
    // Note: This manually unlocks and relocks the mutex currently held by the
    // lock guard. It must not return while the mutex is not locked.
    duration timeout = timeout_;
    mutex_.unlock();
    bool writeable = Await(write_request_.notification, timeout);
    mutex_.lock();

    // Conditions may have changed while waiting; check again.
    if (reader_ == nullptr) {
      return Status::OutOfRange();
    }
    if (!writeable || limit_ < data.size()) {
      status = Status::ResourceExhausted();
      break;
    }

    // Attempt to write data.
    StatusWithSize result = reader_->WriteData(data, limit_);
    last_write_ += result.size();
    if (limit_ != kUnlimited) {
      limit_ -= result.size();
    }

    // WriteData() only returns an error if the reader is closed. In that case,
    // or if the writer has written all of its data, the writer should close.
    if (!result.ok() || limit_ == 0) {
      CloseLocked();
      return result.status();
    }
    data = data.subspan(result.size());
  }

  // Unsubscribe from the reader.
  reader_->CompleteWrite(write_request_);
  return status;
}

void MpscWriter::Close() {
  std::lock_guard lock(mutex_);
  CloseLocked();
}

void MpscWriter::CloseLocked() {
  if (reader_ != nullptr) {
    std::lock_guard lock(reader_->mutex_);
    reader_->CompleteWriteLocked(write_request_);
    write_request_.notification.release();
    if (reader_->writers_.remove(*this)) {
      reader_->DecreaseLimitLocked(limit_);
    }
    if (reader_->writers_.empty()) {
      reader_->readable_.release();
    }
    reader_ = nullptr;
  }
  limit_ = kUnlimited;
}

////////////////////////////////////////////////////////////////////////////////
// MpscReader methods.

MpscReader::MpscReader() { last_request_ = write_requests_.begin(); }

MpscReader::~MpscReader() { Close(); }

bool MpscReader::connected() const {
  std::lock_guard lock(mutex_);
  return !writers_.empty();
}

void MpscReader::SetBuffer(ByteSpan buffer) {
  std::lock_guard lock(mutex_);
  PW_CHECK(length_ == 0);
  buffer_ = buffer;
  offset_ = 0;
}

void MpscReader::SetTimeout(const duration& timeout) {
  std::lock_guard lock(mutex_);
  timeout_ = timeout;
}

void MpscReader::IncreaseLimit(size_t delta) {
  std::lock_guard lock(mutex_);
  IncreaseLimitLocked(delta);
}

void MpscReader::IncreaseLimitLocked(size_t delta) {
  if (delta == kUnlimited) {
    ++num_unlimited_;
    PW_CHECK_UINT_NE(num_unlimited_, 0);
  } else if (limit_ != kUnlimited) {
    PW_CHECK_UINT_LT(limit_, kUnlimited - delta);
    limit_ += delta;
  }
}

void MpscReader::DecreaseLimit(size_t delta) {
  std::lock_guard lock(mutex_);
  DecreaseLimitLocked(delta);
}

void MpscReader::DecreaseLimitLocked(size_t delta) {
  if (delta == kUnlimited) {
    PW_CHECK_UINT_NE(num_unlimited_, 0);
    --num_unlimited_;
  } else if (limit_ != kUnlimited) {
    PW_CHECK_UINT_LE(delta, limit_);
    limit_ -= delta;
  }
}

size_t MpscReader::ConservativeLimit(LimitType type) const {
  std::lock_guard lock(mutex_);
  if (type != LimitType::kRead) {
    return 0;
  }
  if (writers_.empty()) {
    return length_;
  }
  if (num_unlimited_ != 0) {
    return kUnlimited;
  }
  return limit_;
}

void MpscReader::RequestWrite(MpscWriter::Request& write_request) {
  std::lock_guard lock(mutex_);
  last_request_ = write_requests_.insert_after(last_request_, write_request);
  CheckWriteableLocked();
}

void MpscReader::CheckWriteableLocked() {
  if (write_requests_.empty()) {
    return;
  }
  if (writers_.empty() || written_ < destination_.size() ||
      length_ < buffer_.size()) {
    MpscWriter::Request& write_request = write_requests_.front();
    write_request.notification.release();
  }
}

StatusWithSize MpscReader::WriteData(ConstByteSpan data, size_t limit) {
  std::lock_guard lock(mutex_);
  if (writers_.empty()) {
    return StatusWithSize::OutOfRange(0);
  }
  size_t length = 0;
  size_t available = buffer_.size() - length_;
  if (written_ < destination_.size()) {
    // A read is pending; copy directly into its buffer.
    // Note: this condition is only true when the buffer is empty, so data
    // order is preserved.
    length = std::min(destination_.size() - written_, data.size());
    memcpy(&destination_[written_], &data[0], length);
    written_ += length;
  } else if (available > 0) {
    // The buffer has space for more data.
    length = std::min(available, data.size());
    size_t offset = (offset_ + length_) % buffer_.size();
    size_t contiguous = buffer_.size() - offset;
    if (length <= contiguous) {
      memcpy(&buffer_[offset], &data[0], length);
    } else {
      memcpy(&buffer_[offset], &data[0], contiguous);
      memcpy(&buffer_[0], &data[contiguous], length - contiguous);
    }
    length_ += length;
  } else {
    // If there is no space available, a write request can only be notified when
    // its writer is closing. Do not notify the reader that data is available.
    return StatusWithSize(0);
  }
  data = data.subspan(length);
  // For unlimited writers, increase the read limit as needed.
  // Do this before waking the reader and releasing the lock.
  if (limit == kUnlimited) {
    IncreaseLimitLocked(length);
  }
  readable_.release();
  return StatusWithSize(length);
}

void MpscReader::CompleteWrite(MpscWriter::Request& write_request) {
  std::lock_guard lock(mutex_);
  CompleteWriteLocked(write_request);
}

void MpscReader::CompleteWriteLocked(MpscWriter::Request& write_request) {
  MpscWriter::Request& last_request = *last_request_;
  write_requests_.remove(write_request);

  // If the last request is removed, find the new last request. This is O(n),
  // but the oremoved element is first unless a request is being canceled due to
  // its writer closing. Thus in the typical case of a successful write, this is
  // O(1).
  if (&last_request == &write_request) {
    last_request_ = write_requests_.begin();
    for (size_t i = 1; i < write_requests_.size(); ++i) {
      ++last_request_;
    }
  }

  // The reader may have signaled this writer that it had space between the last
  // call to WriteData() and this call. Check if that signal should be forwarded
  // to the next write request.
  CheckWriteableLocked();
}

StatusWithSize MpscReader::DoRead(ByteSpan destination) {
  if (destination.empty()) {
    return StatusWithSize(0);
  }
  mutex_.lock();
  PW_CHECK(!reading_, "All reads must happen from the same thread.");
  reading_ = true;
  Status status = OkStatus();
  size_t length = 0;

  // Check for buffered data. Do this before checking if the reader is still
  // connected in order to deliver data sent from a now-closed writer.
  if (length_ != 0) {
    length = std::min(length_, destination.size());
    size_t contiguous = buffer_.size() - offset_;
    if (length < contiguous) {
      memcpy(&destination[0], &buffer_[offset_], length);
      offset_ += length;
    } else if (length == contiguous) {
      memcpy(&destination[0], &buffer_[offset_], length);
      offset_ = 0;
    } else {
      memcpy(&destination[0], &buffer_[offset_], contiguous);
      offset_ = length - contiguous;
      memcpy(&destination[contiguous], &buffer_[0], offset_);
    }
    length_ -= length;
    DecreaseLimitLocked(length);
    CheckWriteableLocked();

  } else {
    // Register the output buffer to and wait for Write() to bypass the buffer
    // and write directly into it. Note that the buffer is only bypassed when
    // empty, so data order is preserved.
    PW_CHECK(written_ == 0);
    destination_ = destination;
    CheckWriteableLocked();

    // The reader state may change while waiting, or even between acquiring the
    // notification and acquiring the lock. As an example, the following
    // sequence of events is possible:
    //
    //   1. A writer partially fills the output buffer and releases the
    //      notification.
    //   2. The reader acquires the notification.
    //   3. Another writer fills the remainder of the buffer and releass the
    //      notification *again*.
    //   4. The reader acquires the lock.
    //
    // In this case, on the *next* read, the notification will be acquired
    // immediately even if no data is available. As a result, this code loops
    // until data is available.
    while (status.ok()) {
      bool readable = true;
      if (!writers_.empty()) {
        // Wait for a writer to provide data, or the reader to be closed.
        duration timeout = timeout_;
        mutex_.unlock();
        readable = Await(readable_, timeout);
        mutex_.lock();
      }
      if (!readable) {
        status = Status::ResourceExhausted();
      } else if (written_ != 0) {
        break;
      } else if (writers_.empty()) {
        status = Status::OutOfRange();
      }
    }
    destination_ = ByteSpan();
    length = written_;
    written_ = 0;
    DecreaseLimitLocked(length);
    CheckWriteableLocked();
  }

  reading_ = false;
  if (writers_.empty()) {
    closeable_.release();
  }
  mutex_.unlock();
  return StatusWithSize(status, length);
}

Status MpscReader::ReadAll(ReadAllCallback callback) {
  mutex_.lock();
  if (buffer_.empty()) {
    mutex_.unlock();
    return Status::FailedPrecondition();
  }
  PW_CHECK(!reading_, "All reads must happen from the same thread.");
  reading_ = true;

  Status status = Status::OutOfRange();
  while (true) {
    // Check for buffered data. Do this before checking if the reader still has
    // writers in order to deliver data sent from a now-closed writer.
    if (length_ != 0) {
      size_t length = std::min(buffer_.size() - offset_, length_);
      ConstByteSpan data(&buffer_[offset_], length);
      offset_ = (offset_ + length_) % buffer_.size();
      length_ -= length;
      DecreaseLimitLocked(data.size());
      CheckWriteableLocked();
      status = callback(data);
      if (!status.ok()) {
        break;
      }
    }
    if (writers_.empty()) {
      break;
    }
    // Wait for a writer to provide data.
    duration timeout = timeout_;
    mutex_.unlock();
    bool readable = Await(readable_, timeout);
    mutex_.lock();
    if (!readable) {
      status = Status::ResourceExhausted();
      break;
    }
  }
  reading_ = false;
  if (writers_.empty()) {
    closeable_.release();
  }
  mutex_.unlock();
  return status;
}

void MpscReader::Close() {
  mutex_.lock();
  if (writers_.empty()) {
    mutex_.unlock();
    return;
  }
  IntrusiveList<MpscWriter> writers;
  while (!writers_.empty()) {
    MpscWriter& writer = writers_.front();
    writers_.pop_front();
    writers.push_front(writer);
  }

  // Wait for any pending read to finish.
  if (reading_) {
    mutex_.unlock();
    readable_.release();
    closeable_.acquire();
    mutex_.lock();
  }

  num_unlimited_ = 0;
  limit_ = 0;
  written_ = 0;
  offset_ = 0;
  length_ = 0;
  mutex_.unlock();

  for (auto& writer : writers) {
    writer.Close();
  }
}

}  // namespace pw::stream
