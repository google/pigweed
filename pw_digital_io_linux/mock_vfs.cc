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

// TODO(b/328262654): Move this to a more appropriate module.
namespace pw::digital_io {

MockVfs& GetMockVfs() {
  static MockVfs vfs;
  return vfs;
}

MockFile* MockVfs::GetFile(const int fd) const {
  if (fd < kFakeFdBase) {
    return nullptr;
  }
  const size_t index = fd - kFakeFdBase;
  if (index >= open_files_.size()) {
    return nullptr;
  }
  return open_files_[index].get();
}

int MockVfs::InstallFile(std::unique_ptr<MockFile>&& file) {
  // Find the lowest unused index.
  size_t index;
  for (index = 0; index < open_files_.size(); ++index) {
    if (open_files_[index] == nullptr) {
      break;
    }
  }

  const int fd = kFakeFdBase + index;
  PW_LOG_DEBUG("Installing fd %d: \"%s\"", fd, file->name().c_str());

  const size_t min_size = index + 1;
  if (min_size > open_files_.size()) {
    open_files_.resize(min_size);
  }
  open_files_[index] = std::move(file);

  return fd;
}

bool MockVfs::AllFdsClosed() const {
  for (const auto& f : open_files_) {
    if (f != nullptr) {
      return false;
    }
  }
  return true;
}

int MockVfs::mock_close(int fd) {
  auto* file = GetFile(fd);
  if (!file) {
    // TODO: https://pwbug.dev/338269682 - The return value of close(2) is
    // frequently ignored, so provide a hook here to let consumers handle this
    // error.
    errno = EBADF;
    return -1;
  }

  int result = file->Close();

  const int index = fd - kFakeFdBase;
  open_files_[index] = nullptr;

  return result;
}

int MockVfs::mock_ioctl(int fd, unsigned long request, void* arg) {
  auto* file = GetFile(fd);
  if (!file) {
    errno = EBADF;
    return -1;
  }
  return file->Ioctl(request, arg);
}

////////////////////////////////////////////////////////////////////////////////
// Syscalls wrapped via --wrap

#include <unistd.h>

extern "C" {

// close()

decltype(close) __real_close;
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

}  // extern "C"

}  // namespace pw::digital_io
