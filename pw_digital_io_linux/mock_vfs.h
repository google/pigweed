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
#pragma once

#include <unistd.h>

#include <map>
#include <string>

#include "pw_log/log.h"

// TODO(b/328262654): Move this to a more appropriate module.
namespace pw::digital_io {

class MockVfs;

// A mocked representation of an open file in the Linux kernel.
// Consumers are expected to inherit from this class and implement their own
// custom test file behaviors.
class MockFile {
 public:
  MockFile(MockVfs& vfs, int eventfd, const std::string& name)
      : vfs_(vfs), eventfd_(eventfd), name_(name) {}
  virtual ~MockFile() = default;

  MockVfs& vfs() const { return vfs_; }
  int eventfd() const { return eventfd_; }
  const std::string& name() { return name_; }

  // Public interface, intended for use by tests.
  void WriteEventfd(uint64_t add = 1);
  uint64_t ReadEventfd();

  // Public interface, intended for use by MockVfs.
  // These methods conform closely to syscalls of the same name.
  int Close();
  int Ioctl(unsigned long request, void* arg) { return DoIoctl(request, arg); }
  ssize_t Read(void* buf, size_t count) { return DoRead(buf, count); }

 private:
  MockVfs& vfs_;
  // NOTE: We can't use OwnedFd here because it calls close(), which is wrapped
  // in mock_vfs, which would lead to recursion.
  static constexpr int kInvalidFd = -1;
  int eventfd_ = kInvalidFd;
  const std::string name_;

  // Derived class interface
  // These methods conform closely to syscalls of the same name.
  virtual int DoClose() { return 0; }

  virtual int DoIoctl(unsigned long /* request */, void* /* arg */) {
    PW_LOG_ERROR("[%s] Ioctl unimplemented", name_.c_str());
    return -1;
  }

  virtual ssize_t DoRead(void* /* buf */, size_t /* count */) {
    PW_LOG_ERROR("[%s] Read unimplemented", name_.c_str());
    return -1;
  }
};

// A mocked representation of the Linux kernel's Virtual File System (VFS).
// Tracks the association of (fake) file descriptors -> open MockFile objects
// and provides a subset of mocked system calls which are handled by invoking
// methods on said MockFile objects.
class MockVfs {
 public:
  MockVfs() {}
  MockVfs(const MockVfs& other) = delete;
  MockVfs(const MockVfs&& other) = delete;
  MockVfs operator=(const MockVfs& other) = delete;
  MockVfs operator=(const MockVfs&& other) = delete;

  // Returns true if the fd is in the range of mocked fds, not if it is open.
  bool IsMockFd(const int fd);

  // Creates a new MockFile object associated with this vfs.
  // The FileType template argument must be MockFile or a derived class.
  // Arguments are forwarded to the FileType constructor.
  template <class FileType, typename... Args>
  std::unique_ptr<FileType> MakeFile(Args&&... args) {
    return std::make_unique<FileType>(
        *this, GetEventFd(), std::forward<Args>(args)...);
  }

  // Installs a MockFile object into this vfs, and returns a newly-assigned fd.
  int InstallFile(std::unique_ptr<MockFile>&& file);

  // Creates and installs a new MockFile object into this vfs and returns a
  // newly-assigned fd. See MakeFile() and InstallFile().
  template <class FileType, typename... Args>
  int InstallNewFile(Args&&... args) {
    return InstallFile(MakeFile<FileType>(std::forward<Args>(args)...));
  }

  // Returns true if there are no open files.
  bool AllFdsClosed() const;

  // Resets the vfs to its default state, closing any open files.
  void Reset();

  // Mocked syscalls
  int mock_close(int fd);
  int mock_ioctl(int fd, unsigned long request, void* arg);
  ssize_t mock_read(int fd, void* buf, size_t count);

 private:
  // All open files. Key is (real) fd number.
  std::map<int, std::unique_ptr<MockFile>> open_files_;

  int GetEventFd();

  // Gets the MockFile object associated with the given fd.
  // If fd refers to an open MockFile, a pointer to that file is returned (and
  // file continues to be owned by the vfs). Otherwise, returns nullptr.
  MockFile* GetFile(const int fd) const;
};

// Get a reference to a global MockVfs singleton object.
// This object is the one referenced by the (necessarily) global system call
// wrappers enabled via the --wrap= linker option.
MockVfs& GetMockVfs();

}  // namespace pw::digital_io
