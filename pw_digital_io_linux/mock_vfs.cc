// Copyright 2024 The Pigweed Authors
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
#include "mock_vfs.h"

#include <sys/eventfd.h>
#include <unistd.h>

#include <cinttypes>

#include "log_errno.h"
#include "pw_assert/check.h"

extern "C" {

decltype(close) __real_close;
decltype(read) __real_read;
#define __real_write ::write  // Not (yet) wraapped

}  // extern "C"

// TODO(b/328262654): Move this to a more appropriate module.
namespace pw::digital_io {

MockVfs& GetMockVfs() {
  static MockVfs vfs;
  return vfs;
}

// MockVfs

MockFile* MockVfs::GetFile(const int fd) const {
  if (auto it = open_files_.find(fd); it != open_files_.end()) {
    return it->second.get();
  }
  return nullptr;
}

bool MockVfs::IsMockFd(const int fd) { return GetFile(fd) != nullptr; }

int MockVfs::GetEventFd() {
  // All files are backed by a real (kernel) eventfd.
  const int fd = ::eventfd(0, EFD_SEMAPHORE | EFD_CLOEXEC | EFD_NONBLOCK);
  PW_CHECK_INT_GE(fd,
                  0,
                  "eventfd() failed: " ERRNO_FORMAT_STRING,
                  ERRNO_FORMAT_ARGS(errno));

  // There should be no existing file registered with this eventfd.
  PW_CHECK_PTR_EQ(GetFile(fd), nullptr);

  return fd;
}

int MockVfs::InstallFile(std::unique_ptr<MockFile>&& file) {
  // All files are backed by a real (kernel) eventfd.
  const int fd = file->eventfd();

  PW_LOG_DEBUG("Installing fd %d: \"%s\"", fd, file->name().c_str());

  auto [_, inserted] = open_files_.try_emplace(fd, std::move(file));
  PW_CHECK(inserted);

  return fd;
}

void MockVfs::Reset() {
  for (const auto& [fd, file] : open_files_) {
    file->Close();
  }
  open_files_.clear();
}

bool MockVfs::AllFdsClosed() const { return open_files_.empty(); }

int MockVfs::mock_close(int fd) {
  // Attempt to remove the file from the map.
  auto node = open_files_.extract(fd);
  if (!node) {
    // TODO: https://pwbug.dev/338269682 - The return value of close(2) is
    // frequently ignored, so provide a hook here to let consumers handle this
    // error.
    errno = EBADF;
    return -1;
  }
  return node.mapped()->Close();
}

int MockVfs::mock_ioctl(int fd, unsigned long request, void* arg) {
  auto* file = GetFile(fd);
  if (!file) {
    errno = EBADF;
    return -1;
  }
  return file->Ioctl(request, arg);
}

ssize_t MockVfs::mock_read(int fd, void* buf, size_t count) {
  auto* file = GetFile(fd);
  if (!file) {
    errno = EBADF;
    return -1;
  }
  return file->Read(buf, count);
}

// MockFile

int MockFile::Close() {
  int result = DoClose();

  // Close the real eventfd
  PW_CHECK_INT_NE(eventfd_, kInvalidFd);
  PW_CHECK_INT_EQ(__real_close(eventfd_), 0);
  eventfd_ = kInvalidFd;

  return result;
}

void MockFile::WriteEventfd(uint64_t add) {
  const ssize_t ret = __real_write(eventfd_, &add, sizeof(add));
  PW_CHECK(ret == sizeof(add));
}

uint64_t MockFile::ReadEventfd() {
  uint64_t val;
  ssize_t nread = __real_read(eventfd_, &val, sizeof(val));
  if (nread == -1 && errno == EAGAIN) {
    return 0;
  }
  PW_CHECK_INT_EQ(nread, sizeof(val));
  return val;
}

////////////////////////////////////////////////////////////////////////////////
// Syscalls wrapped via --wrap

#include <unistd.h>

extern "C" {

// close()

decltype(close) __wrap_close;

int __wrap_close(int fd) {
  auto& vfs = GetMockVfs();
  if (vfs.IsMockFd(fd)) {
    return vfs.mock_close(fd);
  }
  return __real_close(fd);
}

// ioctl()

// ioctl() is actually variadic (third arg is ...), but there's no way
// (https://c-faq.com/varargs/handoff.html) to forward the args when invoked
// that way, so we lie and use void*.
int __real_ioctl(int fd, unsigned long request, void* arg);
int __wrap_ioctl(int fd, unsigned long request, void* arg);

int __wrap_ioctl(int fd, unsigned long request, void* arg) {
  auto& vfs = GetMockVfs();
  if (vfs.IsMockFd(fd)) {
    return vfs.mock_ioctl(fd, request, arg);
  }
  return __real_ioctl(fd, request, arg);
}

// read()

decltype(read) __wrap_read;

ssize_t __wrap_read(int fd, void* buf, size_t nbytes) {
  auto& vfs = GetMockVfs();
  if (vfs.IsMockFd(fd)) {
    return vfs.mock_read(fd, buf, nbytes);
  }
  return __real_read(fd, buf, nbytes);
}

}  // extern "C"

}  // namespace pw::digital_io
