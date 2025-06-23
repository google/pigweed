// Copyright 2025 The Pigweed Authors
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

#include "pw_multibuf/multibuf_v2.h"

#include <array>

#include "public/pw_multibuf/multibuf_v2.h"
#include "pw_allocator/chunk_pool.h"
#include "pw_allocator/testing.h"
#include "pw_assert/check.h"
#include "pw_bytes/span.h"
#include "pw_compilation_testing/negative_compilation.h"
#include "pw_result/result.h"
#include "pw_status/try.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures. //////////////////////////////////////////////////////////////

using ::pw::allocator::test::AllocatorForTest;
using ::pw::multibuf::ConstMultiBuf;
using ::pw::multibuf::ConstMultiBufInstance;
using ::pw::multibuf::MultiBuf;
using ::pw::multibuf::MultiBufInstance;
using ::pw::multibuf::TrackedMultiBuf;
using ::pw::multibuf::TrackedMultiBufInstance;
using Event = ::pw::multibuf::Observer::Event;

constexpr size_t kN = 32;

/// Test fixture that includes helper methods to set up structures used to test
/// MultiBufs
class MultiBufTest : public ::testing::Test {
 protected:
  MultiBufTest() {
    for (size_t i = 0; i < unowned_chunk_.size(); ++i) {
      unowned_chunk_[i] = static_cast<std::byte>(i);
    }
    owned_chunk_ = allocator_.MakeUnique<std::byte[]>(kN);
    PW_CHECK_NOTNULL(owned_chunk_.get());
    for (size_t i = 0; i < kN; ++i) {
      owned_chunk_[i] = static_cast<std::byte>(i);
    }
    owned_bytes_ = pw::ByteSpan(owned_chunk_.get(), owned_chunk_.size());
  }

  /// Helper to make a MultiBuf with non-contiguous buffers.
  void MakeNonContiguous(ConstMultiBuf& out, size_t n, uint8_t value) {
    auto bytes1 = allocator_.MakeUnique<std::byte[]>(n / 2);
    auto bytes2 = allocator_.MakeUnique<std::byte[]>(n / 2);
    auto bytes3 = allocator_.MakeUnique<std::byte[]>(n / 2);
    PW_CHECK_NOTNULL(bytes1.get());
    PW_CHECK_NOTNULL(bytes2.get());
    PW_CHECK_NOTNULL(bytes3.get());
    PW_CHECK_PTR_NE(bytes1.get() + bytes1.size(), bytes3.get());
    std::memset(bytes1.get(), value, bytes1.size());
    std::memset(bytes3.get(), value, bytes3.size());
    out.PushBack(std::move(bytes1));
    out.PushBack(std::move(bytes3));
  }

  /// Helper method to instantiate a layered MultiBuf that resembles the entries
  /// used by `multibuf::internal::IteratorTest`.
  ///
  /// The created sequence represents 4 chunks with three layers, i.e.
  ///
  /// layer 3: <[0x3]={4, 8}>  [0x7]={0, 0}  <[0xB]={8, 8}  [0xF]={0,16}>
  /// layer 2: <[0x2]={2,12}> <[0x6]={0, 8}> <[0xA]={4,12}  [0xE]={0,16}>
  /// layer 1: <[0x1]={0,16}> <[0x5]={0,16}> <[0x9]={0,16}><[0xD]={0,16}>
  /// layer 0:  [0x0].data     [0x4].data     [0x8].data    [0xC].data
  ///
  /// where "<...>" represents a fragment.
  void AddLayers(ConstMultiBuf& mb) {
    MultiBufInstance fragment(allocator_);
    auto chunk = allocator_.MakeUnique<std::byte[]>(16);
    fragment->PushBack(std::move(chunk));
    PW_CHECK(fragment->AddLayer(2, 12));
    PW_CHECK(fragment->AddLayer(2, 8));
    mb.PushBack(std::move(*fragment));

    fragment = MultiBufInstance(allocator_);
    chunk = allocator_.MakeUnique<std::byte[]>(16);
    fragment->PushBack(std::move(chunk));
    PW_CHECK(fragment->AddLayer(0, 8));
    PW_CHECK(fragment->AddLayer(0, 0));
    mb.PushBack(std::move(*fragment));

    fragment = MultiBufInstance(allocator_);
    chunk = allocator_.MakeUnique<std::byte[]>(16);
    fragment->PushBack(std::move(chunk));
    chunk = allocator_.MakeUnique<std::byte[]>(16);
    fragment->PushBack(std::move(chunk));
    PW_CHECK(fragment->AddLayer(4));
    PW_CHECK(fragment->AddLayer(4));
    mb.PushBack(std::move(*fragment));
  }

  std::array<std::byte, kN / 2> unowned_chunk_;

  AllocatorForTest<1024> allocator_;
  pw::UniquePtr<std::byte[]> owned_chunk_;

  pw::ByteSpan owned_bytes_;
};

// A test fixture that receives events when a MultiBuf changes.
struct TestObserver : public pw::multibuf::Observer {
  std::optional<Event> event;
  size_t value = 0;

 private:
  void DoNotify(Event event_, size_t value_) override {
    event = event_;
    value = value_;
  }
};

// Unit tests. /////////////////////////////////////////////////////////////////

// TODO(b/425740614): Add tests of correct Properties for each type alias.
// TODO(b/425740614): Add tests of CanConvertFrom for each possible conversion
// TODO(b/425740614): Add tests of each possible user-defined conversion

TEST_F(MultiBufTest, DefaultConstructedIsEmpty) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = *mbi;
  EXPECT_TRUE(mb.empty());
  EXPECT_EQ(mb.size(), 0u);
}

TEST_F(MultiBufTest, InstancesAreMovable) {
  // The allocators must outlive their allocations.
  AllocatorForTest<128> allocator1;
  AllocatorForTest<128> allocator2;

  auto& metrics1 = allocator1.metrics();
  auto& metrics2 = allocator2.metrics();

  // Nothing is initially allocated.
  ConstMultiBufInstance mbi1(allocator1);
  ConstMultiBuf& mb1 = mbi1;
  EXPECT_EQ(metrics1.allocated_bytes.value(), 0u);

  auto chunk = allocator_.MakeUnique<std::byte[]>(kN);
  ASSERT_TRUE(mb1.TryReserveForPushBack(chunk));
  mb1.PushBack(std::move(chunk));
  size_t allocated_bytes = metrics1.allocated_bytes.value();
  EXPECT_NE(allocated_bytes, 0u);

  // Moving clears the destination MultiBuf, and does not allocate any new
  // memory.
  ConstMultiBufInstance mbi2(allocator2);
  ConstMultiBuf& mb2 = mbi2;

  chunk = allocator_.MakeUnique<std::byte[]>(kN);
  EXPECT_EQ(metrics2.allocated_bytes.value(), 0u);
  ASSERT_TRUE(mb2.TryReserveForPushBack(chunk));
  mb2.PushBack(std::move(chunk));
  EXPECT_NE(metrics2.allocated_bytes.value(), 0u);
  mbi2 = std::move(mbi1);
  EXPECT_EQ(metrics2.allocated_bytes.value(), 0u);
  EXPECT_EQ(metrics1.allocated_bytes.value(), allocated_bytes);

  // Allocator gets passed along with move and is used when freeing.
  {
    ConstMultiBufInstance mbi3(std::move(mbi2));
    EXPECT_EQ(metrics1.allocated_bytes.value(), allocated_bytes);
  }
  EXPECT_EQ(metrics1.allocated_bytes.value(), 0u);
}

#if PW_NC_TEST(CopyConstructSameProperties)
PW_NC_EXPECT_CLANG("call to deleted constructor");
PW_NC_EXPECT_GCC("use of deleted function");
[[maybe_unused]] ConstMultiBuf DeletedMove(const ConstMultiBuf& mb1) {
  ConstMultiBuf mb2(mb1);
  return mb2;
}

#elif PW_NC_TEST(CopyAssignSameProperties)
PW_NC_EXPECT_CLANG("call to deleted constructor");
PW_NC_EXPECT_GCC("use of deleted function");
[[maybe_unused]] ConstMultiBuf DeletedMove(const ConstMultiBuf& mb1) {
  ConstMultiBuf mb2 = mb1;
  return mb2;
}

#elif PW_NC_TEST(CopyConstructDifferentProperties)
PW_NC_EXPECT_CLANG("call to deleted constructor");
PW_NC_EXPECT_GCC("use of deleted function");
[[maybe_unused]] ConstMultiBuf DeletedMove(const MultiBuf& mb1) {
  ConstMultiBuf mb2(mb1);
  return mb2;
}

#elif PW_NC_TEST(CopyAssignDifferentProperties)
PW_NC_EXPECT_CLANG("call to deleted constructor");
PW_NC_EXPECT_GCC("use of deleted function");
[[maybe_unused]] ConstMultiBuf DeletedMove(const MultiBuf& mb1) {
  ConstMultiBuf mb2 = mb1;
  return mb2;
}

#elif PW_NC_TEST(MoveConstructSameProperties)
PW_NC_EXPECT_CLANG("call to deleted constructor");
PW_NC_EXPECT_GCC("use of deleted function");
[[maybe_unused]] ConstMultiBuf DeletedMove(ConstMultiBuf& mb1) {
  ConstMultiBuf mb2(std::move(mb1));
  return mb2;
}

#elif PW_NC_TEST(MoveAssignSameProperties)
PW_NC_EXPECT_CLANG("call to deleted constructor");
PW_NC_EXPECT_GCC("use of deleted function");
[[maybe_unused]] ConstMultiBuf DeletedMove(ConstMultiBuf& mb1) {
  ConstMultiBuf mb2 = std::move(mb1);
  return mb2;
}

#elif PW_NC_TEST(MoveConstructDifferentProperties)
PW_NC_EXPECT_CLANG("call to deleted constructor");
PW_NC_EXPECT_GCC("use of deleted function");
[[maybe_unused]] ConstMultiBuf DeletedMove(MultiBuf& mb1) {
  ConstMultiBuf mb2(std::move(mb1));
  return mb2;
}

#elif PW_NC_TEST(MoveAssignDifferentProperties)
PW_NC_EXPECT_CLANG("call to deleted constructor");
PW_NC_EXPECT_GCC("use of deleted function");
[[maybe_unused]] ConstMultiBuf DeletedMove(MultiBuf& mb1) {
  ConstMultiBuf mb2 = std::move(mb1);
  return mb2;
}
#endif  // PW_NC_TEST

// TODO(b/425740614): Add test to check size for empty MultiBuf
// TODO(b/425740614): Add test to check size for Multibuf with one chunk
// TODO(b/425740614): Add test to check size for Multibuf with multiple chunk

TEST_F(MultiBufTest, IsDerefencableWithAt) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);
  for (size_t i = 0; i < unowned_chunk_.size(); ++i) {
    EXPECT_EQ(mb.at(i), static_cast<std::byte>(i));
  }
}

// TODO(b/425740614): Add a negative compilation test for mutable dereference.

TEST_F(MultiBufTest, IsDerefencableWithArrayOperator) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);
  for (size_t i = 0; i < unowned_chunk_.size(); ++i) {
    EXPECT_EQ(mb[i], static_cast<std::byte>(i));
  }
}

// TODO(b/425740614): Add a negative compilation test for mutable array access.

TEST_F(MultiBufTest, IterateConstChunksOverEmpty) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  for (auto buffer : mb.ConstChunks()) {
    EXPECT_NE(buffer.data(), buffer.data());
    EXPECT_NE(buffer.size(), buffer.size());
  }
}

// TODO(b/425740614): Add a negative compilation test for mutable iterators.

TEST_F(MultiBufTest, IterateChunksOverEmpty) {
  MultiBufInstance mbi(allocator_);
  MultiBuf& mb = mbi;
  for (auto buffer : mb.Chunks()) {
    EXPECT_NE(buffer.data(), buffer.data());
    EXPECT_NE(buffer.size(), buffer.size());
  }
}

TEST_F(MultiBufTest, IterateConstChunksOverOne) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);

  for (auto buffer : mb.ConstChunks()) {
    EXPECT_EQ(buffer.data(), unowned_chunk_.data());
    EXPECT_EQ(buffer.size(), unowned_chunk_.size());
  }
}

TEST_F(MultiBufTest, IterateChunksOverOne) {
  MultiBufInstance mbi(allocator_);
  MultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);

  for (auto buffer : mb.Chunks()) {
    EXPECT_EQ(buffer.data(), unowned_chunk_.data());
    EXPECT_EQ(buffer.size(), unowned_chunk_.size());
  }
}

TEST_F(MultiBufTest, IterateConstBytesOverEmpty) {
  ConstMultiBufInstance mbi(allocator_);

  const ConstMultiBuf& mb1 = mbi;
  EXPECT_EQ(mb1.begin(), mb1.end());

  ConstMultiBuf& mb2 = mbi;
  EXPECT_EQ(mb2.cbegin(), mb2.cend());
}

TEST_F(MultiBufTest, IterateConstBytesOverContiguous) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);
  uint8_t value = 0;
  for (const std::byte& b : mb) {
    EXPECT_EQ(b, static_cast<std::byte>(value));
    ++value;
  }
  EXPECT_EQ(value, unowned_chunk_.size());
}

TEST_F(MultiBufTest, IterateBytesOverContiguous) {
  MultiBufInstance mbi(allocator_);
  MultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);

  // Iterate and assign.
  uint8_t value = 0;
  for (std::byte& b : mb) {
    b = static_cast<std::byte>(value);
    value += 2;
  }
  EXPECT_EQ(value, unowned_chunk_.size() * 2);

  // Check the underlying bytes.
  value = 0;
  for (std::byte& b : unowned_chunk_) {
    EXPECT_EQ(b, static_cast<std::byte>(value));
    value += 2;
  }
  EXPECT_EQ(value, unowned_chunk_.size() * 2);
}

TEST_F(MultiBufTest, IterateConstBytesOverNonContiguous) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  MakeNonContiguous(mb, kN, 0xFF);
  for (const std::byte& b : mb) {
    EXPECT_EQ(b, static_cast<std::byte>(0xFF));
  }
}

TEST_F(MultiBufTest, IterateBytesOverNonContiguous) {
  MultiBufInstance mbi(allocator_);
  MultiBuf& mb = mbi;
  MakeNonContiguous(mb.as<ConstMultiBuf>(), kN, 0xFF);

  // Iterate and assign.
  uint8_t value = 0;
  for (std::byte& b : mb) {
    b = static_cast<std::byte>(value);
    value += 3;
  }
  EXPECT_EQ(value, kN * 3);

  // Check the underlying bytes.
  value = 0;
  for (auto chunk : mb.ConstChunks()) {
    for (const std::byte& b : chunk) {
      EXPECT_EQ(b, static_cast<std::byte>(value));
      value += 3;
    }
  }
  EXPECT_EQ(value, kN * 3);
}

// TODO(b/425740614): Add test for IsCompatible with compatible MultiBuf
// TODO(b/425740614): Add test for IsCompatible with incompatible MultiBuf
// TODO(b/425740614): Add test for IsCompatible with compatible UniquePtr
// TODO(b/425740614): Add test for IsCompatible with incompatible UniquePtr
// TODO(b/425740614): Add test for IsCompatible with compatible SharedPtr
// TODO(b/425740614): Add test for IsCompatible with incompatible SharedPtr

// TODO(b/425740614): Add test for TryReserveChunks with num_chunks equal to
// zero.
// TODO(b/425740614): Add test for TryReserveChunks with num_chunks less than
// the current chunks.
// TODO(b/425740614): Add test for TryReserveChunks with num_chunks equal to the
// current chunks.
// TODO(b/425740614): Add test for TryReserveChunks with num_chunks more than
// the current chunks.
// TODO(b/425740614): Add test for TryReserveChunks with num_chunks more than
// can be satisfied.

// TODO(b/425740614): Add test where TryReserveForInsert of MultiBuf fails due
// to allocation failure
// TODO(b/425740614): Add test where TryReserveForInsert of unowned fails due to
// excessive size
// TODO(b/425740614): Add test where TryReserveForInsert of unique_ptr fails due
// to read-only
// TODO(b/425740614): Add test where TryReserveForInsert of unique_ptr fails due
// to excessive size
// TODO(b/425740614): Add test where TryReserveForInsert of shared_ptr fails due
// to read-only
// TODO(b/425740614): Add test where TryReserveForInsert of shared_ptr fails due
// to excessive size

// TODO(b/425740614): Add test for Insert of MultiBuf into empty MultiBuf with
// no memory context
// TODO(b/425740614): Add test for Insert of MultiBuf into non-empty MultiBuf
// with no memory context at boundary
// TODO(b/425740614): Add test for Insert of MultiBuf into non-empty MultiBuf
// with no memory context mid-chunk
// TODO(b/425740614): Add test for Insert of MultiBuf into empty MultiBuf with
// deallocator
// TODO(b/425740614): Add test for Insert of MultiBuf into non-empty MultiBuf
// with deallocator at boundary
// TODO(b/425740614): Add test for Insert of MultiBuf into non-empty MultiBuf
// with deallocator mid-chunk
// TODO(b/425740614): Add test for Insert of MultiBuf into empty MultiBuf with
// control block
// TODO(b/425740614): Add test for Insert of MultiBuf into non-empty MultiBuf
// with control block at boundary
// TODO(b/425740614): Add test for Insert of MultiBuf into non-empty MultiBuf
// with control block mid-chunk

// TODO(b/425740614): Add test for Insert of unowned into empty MultiBuf with no
// memory context
// TODO(b/425740614): Add test for Insert of unowned into non-empty MultiBuf
// with no memory context at boundary
// TODO(b/425740614): Add test for Insert of unowned into non-empty MultiBuf
// with no memory context mid-chunk
// TODO(b/425740614): Add test for Insert of unowned into empty MultiBuf with
// deallocator
// TODO(b/425740614): Add test for Insert of unowned into non-empty MultiBuf
// with deallocator at boundary
// TODO(b/425740614): Add test for Insert of unowned into non-empty MultiBuf
// with deallocator mid-chunk
// TODO(b/425740614): Add test for Insert of unowned into empty MultiBuf with
// control block
// TODO(b/425740614): Add test for Insert of unowned into non-empty MultiBuf
// with control block at boundary
// TODO(b/425740614): Add test for Insert of unowned into non-empty MultiBuf
// with control block mid-chunk

// TODO(b/425740614): Add test for Insert of unique_ptr into empty MultiBuf with
// no memory context
// TODO(b/425740614): Add test for Insert of unique_ptr into non-empty MultiBuf
// with no memory context at boundary
// TODO(b/425740614): Add test for Insert of unique_ptr into non-empty MultiBuf
// with no memory context mid-chunk
// TODO(b/425740614): Add test for Insert of unique_ptr into empty MultiBuf with
// deallocator
// TODO(b/425740614): Add test for Insert of unique_ptr into non-empty MultiBuf
// with deallocator at boundary
// TODO(b/425740614): Add test for Insert of unique_ptr into non-empty MultiBuf
// with deallocator mid-chunk
// TODO(b/425740614): Add test for Insert of unique_ptr into empty MultiBuf with
// control block
// TODO(b/425740614): Add test for Insert of unique_ptr into non-empty MultiBuf
// with control block at boundary
// TODO(b/425740614): Add test for Insert of unique_ptr into non-empty MultiBuf
// with control block mid-chunk

// TODO(b/425740614): Add test for Insert of shared_ptr into empty MultiBuf with
// no memory context
// TODO(b/425740614): Add test for Insert of shared_ptr into non-empty MultiBuf
// with no memory context at boundary
// TODO(b/425740614): Add test for Insert of shared_ptr into non-empty MultiBuf
// with no memory context mid-chunk
// TODO(b/425740614): Add test for Insert of shared_ptr into empty MultiBuf with
// deallocator
// TODO(b/425740614): Add test for Insert of shared_ptr into non-empty MultiBuf
// with deallocator at boundary
// TODO(b/425740614): Add test for Insert of shared_ptr into non-empty MultiBuf
// with deallocator mid-chunk
// TODO(b/425740614): Add test for Insert of shared_ptr into empty MultiBuf with
// control block
// TODO(b/425740614): Add test for Insert of shared_ptr into non-empty MultiBuf
// with control block at boundary
// TODO(b/425740614): Add test for Insert of shared_ptr into non-empty MultiBuf
// with control block mid-chunk

// TODO(b/425740614): Add test where TryReserveForPushBack of MultiBuf fails due
// to allocation failure
// TODO(b/425740614): Add test where TryReserveForPushBack of unowned fails due
// to excessive size
// TODO(b/425740614): Add test where TryReserveForPushBack of unique_ptr fails
// due to read-only
// TODO(b/425740614): Add test where TryReserveForPushBack of unique_ptr fails
// due to excessive size
// TODO(b/425740614): Add test where TryReserveForPushBack of shared_ptr fails
// due to read-only
// TODO(b/425740614): Add test where TryReserveForPushBack of shared_ptr fails
// due to excessive size

TEST_F(MultiBufTest, TryReserveForPushBackFailsWhenMemoryExhausted) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;

  allocator_.Exhaust();
  EXPECT_FALSE(mb.TryReserveForPushBack(unowned_chunk_));
}

TEST_F(MultiBufTest, PushBackSucceedsWithMultiBuf) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(owned_chunk_));
  mb.PushBack(std::move(owned_chunk_));

  ConstMultiBufInstance fragment(allocator_);
  auto chunk = allocator_.MakeUnique<std::byte[]>(kN * 2);
  ASSERT_TRUE(fragment->TryReserveForPushBack(chunk));
  fragment->PushBack(std::move(chunk));

  ASSERT_TRUE(mb.TryReserveForPushBack(*fragment));
  mb.PushBack(std::move(*fragment));
  EXPECT_EQ(mb.size(), kN * 3);
  EXPECT_TRUE(fragment->empty());
}

TEST_F(MultiBufTest, PushBackSucceedsWithByteSpan) {
  {
    ConstMultiBufInstance mbi(allocator_);
    ConstMultiBuf& mb = mbi;
    ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
    mb.PushBack(unowned_chunk_);
    EXPECT_FALSE(mb.empty());
    EXPECT_EQ(mb.size(), unowned_chunk_.size());
  }

  // Chunk still valid.
  EXPECT_EQ(unowned_chunk_[0], static_cast<std::byte>(0));
}

// TODO(b/425740614): Add test for PushBack of MultiBuf into empty MultiBuf with
// no memory context
// TODO(b/425740614): Add test for PushBack of MultiBuf into non-empty MultiBuf
// with no memory context
// TODO(b/425740614): Add test for PushBack of MultiBuf into empty MultiBuf with
// deallocator
// TODO(b/425740614): Add test for PushBack of MultiBuf into non-empty MultiBuf
// with deallocator
// TODO(b/425740614): Add test for PushBack of MultiBuf into empty MultiBuf with
// control block
// TODO(b/425740614): Add test for PushBack of MultiBuf into non-empty MultiBuf
// with control block

// TODO(b/425740614): Add test for PushBack of unowned into empty MultiBuf with
// no memory context
// TODO(b/425740614): Add test for PushBack of unowned into non-empty MultiBuf
// with no memory context
// TODO(b/425740614): Add test for PushBack of unowned into empty MultiBuf with
// deallocator
// TODO(b/425740614): Add test for PushBack of unowned into non-empty MultiBuf
// with deallocator
// TODO(b/425740614): Add test for PushBack of unowned into empty MultiBuf with
// control block
// TODO(b/425740614): Add test for PushBack of unowned into non-empty MultiBuf
// with control block

// TODO(b/425740614): Add test for PushBack of unique_ptr into empty MultiBuf
// with no memory context
// TODO(b/425740614): Add test for PushBack of unique_ptr into non-empty
// MultiBuf with no memory context

TEST_F(MultiBufTest, PushBackSucceedsWithUniquePtr) {
  auto& metrics = allocator_.metrics();
  {
    ConstMultiBufInstance mbi(allocator_);
    ConstMultiBuf& mb = mbi;
    ASSERT_TRUE(mb.TryReserveForPushBack(owned_chunk_));
    mb.PushBack(std::move(owned_chunk_));
    EXPECT_FALSE(mb.empty());
    EXPECT_EQ(mb.size(), kN);
    EXPECT_NE(metrics.allocated_bytes.value(), 0u);
  }

  // Chunk and deque automatically freed.
  EXPECT_EQ(metrics.allocated_bytes.value(), 0u);
}
// TODO(b/425740614): Add test for PushBack of unique_ptr into non-empty
// MultiBuf with deallocator
// TODO(b/425740614): Add test for PushBack of unique_ptr into empty MultiBuf
// with control block
// TODO(b/425740614): Add test for PushBack of unique_ptr into non-empty
// MultiBuf with control block

// TODO(b/425740614): Add test for PushBack of shared_ptr into empty MultiBuf
// with no memory context
// TODO(b/425740614): Add test for PushBack of shared_ptr into non-empty
// MultiBuf with no memory context
// TODO(b/425740614): Add test for PushBack of shared_ptr into empty MultiBuf
// with deallocator
// TODO(b/425740614): Add test for PushBack of shared_ptr into non-empty
// MultiBuf with deallocator
// TODO(b/425740614): Add test for PushBack of shared_ptr into empty MultiBuf
// with control block
// TODO(b/425740614): Add test for PushBack of shared_ptr into non-empty
// MultiBuf with control block

// TODO(b/425740614): Add test for Remove of the only unowned chunk.
// TODO(b/425740614): Add test for Remove of a complete unowned chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Remove of a partial unowned chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Remove of the only owned chunk.
// TODO(b/425740614): Add test for Remove of a complete owned chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Remove of a partial owned chunk from MultiBuf
// with other chunks.
// TODO(b/425740614): Add test for Remove of the only shared chunk.
// TODO(b/425740614): Add test for Remove of a complete shared chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Remove of a partial shared chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Remove of a multiple unowned chunks.
// TODO(b/425740614): Add test for Remove of a multiple owned chunks.
// TODO(b/425740614): Add test for Remove of a multiple shared chunks.
// TODO(b/425740614): Add test for Remove of a multiple chunk of mixed
// ownership.

TEST_F(MultiBufTest, PopFrontFragmentSucceedsWhenNotEmpty) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;

  auto chunk = allocator_.MakeUnique<std::byte[]>(kN);
  ASSERT_TRUE(mb.TryReserveForPushBack(chunk));
  mb.PushBack(std::move(chunk));

  chunk = allocator_.MakeUnique<std::byte[]>(kN * 2);
  ASSERT_TRUE(mb.TryReserveForPushBack(chunk));
  mb.PushBack(std::move(chunk));

  pw::Result<ConstMultiBufInstance> result = mb.PopFrontFragment();
  ASSERT_EQ(result.status(), pw::OkStatus());
  ConstMultiBufInstance fragment = std::move(*result);
  EXPECT_EQ(fragment->size(), kN);
  EXPECT_EQ(mb.size(), kN * 2);
}

// TODO(b/425740614): Add test for PopFrontFragment with multiple layers

// TODO(b/425740614): Add test for Discard of the only unowned chunk.
// TODO(b/425740614): Add test for Discard of a complete unowned chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Discard of a partial unowned chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Discard of the only owned chunk.
// TODO(b/425740614): Add test for Discard of a complete owned chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Discard of a partial owned chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Discard of the only shared chunk.
// TODO(b/425740614): Add test for Discard of a complete shared chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Discard of a partial shared chunk from
// MultiBuf with other chunks.
// TODO(b/425740614): Add test for Discard of a multiple unowned chunks.
// TODO(b/425740614): Add test for Discard of a multiple owned chunks.
// TODO(b/425740614): Add test for Discard of a multiple shared chunks.
// TODO(b/425740614): Add test for Discard of a multiple chunk of mixed
// ownership.

// TODO(b/425740614): Add test where Release fails for not matching a chunk
// boundary.
// TODO(b/425740614): Add test where Release fails for not being owned.

TEST_F(MultiBufTest, ReleaseSucceedsWhenNotEmptyAndOwned) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(owned_chunk_));
  mb.PushBack(std::move(owned_chunk_));

  auto chunk = allocator_.MakeUnique<std::byte[]>(kN * 2);
  ASSERT_TRUE(mb.TryReserveForPushBack(chunk));
  mb.PushBack(std::move(chunk));

  pw::UniquePtr<const std::byte[]> released = mb.Release(mb.begin());
  EXPECT_EQ(released.get(), owned_bytes_.data());
  EXPECT_EQ(released.size(), owned_bytes_.size());
  EXPECT_EQ(mb.size(), kN * 2);
}

// TODO(b/425740614): Test Release failure

TEST_F(MultiBufTest, CopyToWithContiguousChunks) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  MakeNonContiguous(mb, kN, 0xAA);

  std::array<std::byte, kN> out;
  pw::ByteSpan bytes(out);
  for (size_t offset = 0; offset < kN; ++offset) {
    // Reset the destination.
    std::memset(bytes.data(), 0xBB, bytes.size());

    // Perform the copy.
    pw::ByteSpan dst = bytes.subspan(offset);
    EXPECT_EQ(mb.CopyTo(dst, offset), dst.size());

    // Check the destination.
    for (size_t i = 0; i < offset; ++i) {
      EXPECT_EQ(bytes[i], static_cast<std::byte>(0xBB));
    }
    for (size_t i = offset; i < kN; ++i) {
      EXPECT_EQ(bytes[i], static_cast<std::byte>(0xAA));
    }
  }
}

TEST_F(MultiBufTest, CopyToWithNonContiguousChunks) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  MakeNonContiguous(mb, kN, 0xAA);

  std::array<std::byte, kN> out;
  pw::ByteSpan bytes(out);
  for (size_t offset = 0; offset < kN; ++offset) {
    // Reset the destination.
    std::memset(bytes.data(), 0xBB, bytes.size());

    // Perform the copy.
    pw::ByteSpan dst = bytes.subspan(offset);
    EXPECT_EQ(mb.CopyTo(dst, offset), dst.size());

    // Check the destination.
    for (size_t i = 0; i < offset; ++i) {
      EXPECT_EQ(bytes[i], static_cast<std::byte>(0xBB));
    }
    for (size_t i = offset; i < kN; ++i) {
      EXPECT_EQ(bytes[i], static_cast<std::byte>(0xAA));
    }
  }
}

TEST_F(MultiBufTest, CopyFromWithContiguousChunks) {
  MultiBufInstance mbi(allocator_);
  MultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);

  std::array<std::byte, kN / 2> in;
  pw::ByteSpan bytes(in);
  ASSERT_EQ(bytes.size(), unowned_chunk_.size());
  std::memset(bytes.data(), 0xAA, bytes.size());
  for (size_t offset = 0; offset < unowned_chunk_.size(); ++offset) {
    // Reset the destination.
    std::memset(unowned_chunk_.data(), 0xBB, unowned_chunk_.size());

    // Perform the copy.
    pw::ByteSpan src = bytes.subspan(offset);
    EXPECT_EQ(mb.CopyFrom(src, offset), src.size());

    // Check the destination.
    for (size_t i = 0; i < offset; ++i) {
      EXPECT_EQ(unowned_chunk_[i], static_cast<std::byte>(0xBB));
    }
    for (size_t i = offset; i < unowned_chunk_.size(); ++i) {
      EXPECT_EQ(unowned_chunk_[i], static_cast<std::byte>(0xAA));
    }
  }
}

TEST_F(MultiBufTest, CopyFromWithNonContiguousChunks) {
  MultiBufInstance mbi(allocator_);
  MultiBuf& mb = mbi;
  MakeNonContiguous(mb.as<ConstMultiBuf>(), kN, 0xAA);

  std::array<std::byte, kN> in;
  pw::ByteSpan bytes(in);
  std::memset(bytes.data(), 0xBB, bytes.size());
  for (size_t offset = 0; offset < kN; ++offset) {
    // Reset the destination.
    for (auto chunk : mb.Chunks()) {
      std::memset(chunk.data(), 0xAA, chunk.size());
    }

    // Perform the copy.
    pw::ByteSpan src = bytes.subspan(offset);
    EXPECT_EQ(mb.CopyFrom(src, offset), src.size());

    // Check the destination.
    for (size_t i = 0; i < offset; ++i) {
      EXPECT_EQ(mb[i], static_cast<std::byte>(0xAA));
    }
    for (size_t i = offset; i < kN; ++i) {
      EXPECT_EQ(mb[i], static_cast<std::byte>(0xBB));
    }
  }
}

TEST_F(MultiBufTest, GetContiguousDoesNotCopy) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);
  std::memset(unowned_chunk_.data(), 0xAA, unowned_chunk_.size());

  std::array<std::byte, kN / 2> tmp;
  ASSERT_EQ(tmp.size(), unowned_chunk_.size());
  std::memset(tmp.data(), 0xBB, tmp.size());

  for (size_t offset = 0; offset < unowned_chunk_.size(); ++offset) {
    pw::ConstByteSpan bytes = mb.Get(tmp, offset);
    EXPECT_NE(bytes.data(), tmp.data());
    EXPECT_EQ(offset + bytes.size(), unowned_chunk_.size());

    // Returned span has correct data.
    for (size_t i = 0; i < bytes.size(); ++i) {
      EXPECT_EQ(bytes[i], static_cast<std::byte>(0xAA));
    }

    // Provided span is untouched.
    for (size_t i = 0; i < tmp.size(); ++i) {
      EXPECT_EQ(tmp[i], static_cast<std::byte>(0xBB));
    }
  }
}

TEST_F(MultiBufTest, GetNonContiguousCopies) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  MakeNonContiguous(mb, kN, 0xAA);

  std::array<std::byte, kN> tmp;
  std::memset(tmp.data(), 0xBB, tmp.size());

  for (size_t offset = 0; offset < unowned_chunk_.size(); ++offset) {
    pw::ConstByteSpan bytes = mb.Get(tmp, offset);
    EXPECT_EQ(bytes.data(), tmp.data());
    EXPECT_EQ(offset + bytes.size(), kN);

    // Returned span has correct data.
    for (size_t i = 0; i < bytes.size(); ++i) {
      EXPECT_EQ(bytes[i], static_cast<std::byte>(0xAA));
    }
  }
}

TEST_F(MultiBufTest, GetMoreThanAvailableTruncates) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);

  std::array<std::byte, kN> tmp;
  std::memset(tmp.data(), 0xBB, tmp.size());
  EXPECT_LT(unowned_chunk_.size(), tmp.size());

  for (size_t offset = 0; offset < unowned_chunk_.size(); ++offset) {
    pw::ConstByteSpan bytes = mb.Get(tmp, offset);
    EXPECT_EQ(offset + bytes.size(), unowned_chunk_.size());
  }
}

TEST_F(MultiBufTest, GetPastTheEndReturnsEmpty) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);
  std::array<std::byte, kN> tmp;
  pw::ConstByteSpan bytes = mb.Get(tmp, unowned_chunk_.size());
  EXPECT_EQ(bytes.data(), nullptr);
  EXPECT_EQ(bytes.size(), 0u);
}

TEST_F(MultiBufTest, VisitContiguousDoesNotCopy) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(unowned_chunk_));
  mb.PushBack(unowned_chunk_);
  std::memset(unowned_chunk_.data(), 0x01, unowned_chunk_.size());

  std::array<std::byte, kN / 2> tmp;
  ASSERT_EQ(tmp.size(), unowned_chunk_.size());
  std::memset(tmp.data(), 0x02, tmp.size());

  for (size_t offset = 0; offset < unowned_chunk_.size(); ++offset) {
    size_t total = 0;
    mb.Visit(
        [&total](pw::ConstByteSpan bytes) {
          for (const std::byte& b : bytes) {
            total += static_cast<size_t>(b);
          }
        },
        tmp,
        offset);
    EXPECT_EQ(total, unowned_chunk_.size() - offset);

    // Provided span is untouched.
    for (size_t i = 0; i < tmp.size(); ++i) {
      EXPECT_EQ(tmp[i], static_cast<std::byte>(0x02));
    }
  }
}

TEST_F(MultiBufTest, VisitNonContiguousCopies) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  MakeNonContiguous(mb, kN, 0x01);

  std::array<std::byte, kN> tmp;
  std::memset(tmp.data(), 0x02, tmp.size());

  for (size_t offset = 0; offset < kN; ++offset) {
    size_t total = 0;
    mb.Visit(
        [&total](pw::ConstByteSpan bytes) {
          for (const std::byte& b : bytes) {
            total += static_cast<size_t>(b);
          }
        },
        tmp,
        offset);
    EXPECT_EQ(total, kN - offset);

    // Provided span is modified.
    bool modified = false;
    for (size_t i = 0; i < tmp.size(); ++i) {
      modified |= tmp[i] != static_cast<std::byte>(0x02);
    }
    EXPECT_TRUE(modified);
  }
}

TEST_F(MultiBufTest, ClearFreesChunks) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  ASSERT_TRUE(mb.TryReserveForPushBack(owned_chunk_));
  mb.PushBack(std::move(owned_chunk_));

  mb.Clear();
  EXPECT_EQ(allocator_.deallocate_ptr(), owned_bytes_.data());
  EXPECT_EQ(allocator_.deallocate_size(), owned_bytes_.size());
}

TEST_F(MultiBufTest, IsReusableAfterClear) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  auto chunk = allocator_.MakeUnique<std::byte[]>(kN);
  ASSERT_TRUE(mb.TryReserveForPushBack(chunk));
  mb.PushBack(std::move(chunk));
  mb.Clear();

  chunk = allocator_.MakeUnique<std::byte[]>(kN);
  ASSERT_TRUE(mb.TryReserveForPushBack(chunk));
  mb.PushBack(std::move(chunk));
}

// TODO(b/425740614): Add a negative compilation test for setting an observer
// when unobservable

// TODO(b/425740614): Adapt and enable
// TODO(b/425740614): Add AddChunkNotifiesObserver
// TODO(b/425740614): Add TakeUnownedChunkNotifiesObserver
// TODO(b/425740614): Add TakeOwnedChunkNotifiesObserver
// TODO(b/425740614): Add AppendNotifiesObserver
// TODO(b/425740614): Add MoveFragmentToNotifiesObserver
// TODO(b/425740614): Add AddLayerNotifiesObserver
// TODO(b/425740614): Add PopLayerNotifiesObserver
// TODO(b/425740614): Add ClearNotifiesObserver

// TODO(b/425740614): Add a negative compilation test for enumerating fragments
// when unlayered

// TODO(b/425740614): Add NumFragmentsIsZeroWhenEmpty
// TODO(b/425740614): Add NumFragmentsMatchesAppendedMultiBufs

// TODO(b/425740614): Add a negative compilation test for enumerating layers
// when unlayered

// TODO(b/425740614): Add NumLayersIsZeroWhenEmpty
// TODO(b/425740614): Add IterateChunksOverLayers
// TODO(b/425740614): Add IterateBytesOverLayers

// TODO(b/425740614): Add a negative compilation test for adding layers when
// unlayered

// TODO(b/425740614): Add AddLayerFailsWhenEmpty
// TODO(b/425740614): Add AddLayerFailsUnableToGrowQueue
// TODO(b/425740614): Add AddLayerSucceedsWithZeroOffset
// TODO(b/425740614): Add AddLayerFailsWithZeroLength
// TODO(b/425740614): Add AddLayerFailsWithOutOfRangeOffset
// TODO(b/425740614): Add AddLayerFailsWithOutOfRangeLength

// TODO(b/425740614): Add a test where AddLayer fails if added MultiBuf is too
// deep.

// TODO(b/425740614): Add AddLayerSucceedsWithNonzeroOffset
// TODO(b/425740614): Add AddLayerSucceedsWithNonzeroLength
// TODO(b/425740614): Add AddLayerCreatesNewFragment

// TODO(b/425740614): Add a test of AddLayer where added MultiBuf is shallower.

// TODO(b/425740614): Add test where AddLayer fails when sealed
// TODO(b/425740614): Add test where AddLayer succeeds after Unseal

// TODO(b/425740614): Add a negative compilation test for resizing layers when
// unlayered

// TODO(b/425740614): Add SetLayerFailsWhenEmpty
// TODO(b/425740614): Add SetLayerFailsWhenDepthLessThan3
// TODO(b/425740614): Add SetLayerFailsWithZeroLength
// TODO(b/425740614): Add SetLayerSucceedsWithNonzeroLength

// TODO(b/425740614): Add SetLayerFailsWithOutOfRangeOffset
// TODO(b/425740614): Add SetLayerFailsWithOutOfRangeLength

// TODO(b/425740614): Add test where ResizeTopLayer fails when sealed
// TODO(b/425740614): Add test where ResizeTopLayer succeeds after Unseal

// TODO(b/425740614): Add a negative compilation test for popping layers when
// unlayered

// TODO(b/425740614): Add PopLayerFailsWhenEmpty
// TODO(b/425740614): Add PopLayerSucceedsWhenDeepEnough
// TODO(b/425740614): Add test where PopLayer fails when sealed
// TODO(b/425740614): Add test where PopLayer succeeds after Unseal
// TODO(b/425740614): Add GetReturnsDataFromTopLayerOnly

// TODO(b/425740614): InsertMultiBufNotifiesObserver
// TODO(b/425740614): InsertUnownedNotifiesObserver
// TODO(b/425740614): InsertUniquePtrNotifiesObserver
// TODO(b/425740614): InsertSharedPtrNotifiesObserver

// TODO(b/425740614): PushBackMultiBufNotifiesObserver

TEST_F(MultiBufTest, PushBackMultiBufNotifiesObserver) {
  TestObserver observer;

  TrackedMultiBufInstance mbi1(allocator_);
  TrackedMultiBuf& mb1 = mbi1;
  mb1.set_observer(&observer);

  TrackedMultiBufInstance mbi2(allocator_);
  TrackedMultiBuf& mb2 = mbi2;
  auto chunk = allocator_.MakeUnique<std::byte[]>(kN * 2);
  mb2.PushBack(std::move(chunk));

  EXPECT_FALSE(observer.event.has_value());
  mb1.PushBack(std::move(mb2));
  ASSERT_TRUE(observer.event.has_value());
  EXPECT_EQ(observer.event.value(), Event::kBytesAdded);
  EXPECT_EQ(observer.value, kN * 2);
}

TEST_F(MultiBufTest, PushBackUnownedNotifiesObserver) {
  TestObserver observer;

  TrackedMultiBufInstance mbi(allocator_);
  TrackedMultiBuf& mb = mbi;
  mb.set_observer(&observer);

  EXPECT_FALSE(observer.event.has_value());
  mb.PushBack(unowned_chunk_);
  ASSERT_TRUE(observer.event.has_value());
  EXPECT_EQ(observer.event.value(), Event::kBytesAdded);
  EXPECT_EQ(observer.value, unowned_chunk_.size());
}

// TODO(b/425740614): PushBackUniquePtrNotifiesObserver
// TODO(b/425740614): PushBackSharedPtrNotifiesObserver

TEST_F(MultiBufTest, RemoveNotifiesObserver) {
  TestObserver observer;

  TrackedMultiBufInstance mbi(allocator_);
  TrackedMultiBuf& mb = mbi;
  auto chunk = allocator_.MakeUnique<std::byte[]>(kN);
  mb.PushBack(std::move(chunk));

  mb.set_observer(&observer);
  EXPECT_FALSE(observer.event.has_value());
  auto result = mb.Remove(mb.begin(), kN);
  ASSERT_TRUE(observer.event.has_value());
  EXPECT_EQ(observer.event.value(), Event::kBytesRemoved);
  EXPECT_EQ(observer.value, kN);
}

// TODO(b/425740614): DiscardNotifiesObserver
// TODO(b/425740614): ReleaseNotifiesObserver
// TODO(b/425740614): PopFrontFragmentNotifiesObserver

// TODO(b/425740614): Add a negative compilation test for enumerating fragments
// when unlayered

TEST_F(MultiBufTest, NumFragmentsIsZeroWhenEmpty) {
  ConstMultiBufInstance mbi(allocator_);
  EXPECT_EQ(mbi->NumFragments(), 0u);
}

TEST_F(MultiBufTest, NumFragmentsWithoutLayersMatchesChunks) {
  ConstMultiBufInstance mbi1(allocator_);
  ConstMultiBuf& mb1 = mbi1;

  auto chunk = allocator_.MakeUnique<std::byte[]>(kN * 2);
  mb1.PushBack(std::move(chunk));
  EXPECT_EQ(mb1.NumFragments(), 1u);

  chunk = allocator_.MakeUnique<std::byte[]>(kN);
  mb1.PushBack(std::move(chunk));
  EXPECT_EQ(mb1.NumFragments(), 2u);

  chunk = allocator_.MakeUnique<std::byte[]>(kN / 2);
  mb1.PushBack(std::move(chunk));
  EXPECT_EQ(mb1.NumFragments(), 3u);

  auto result = mb1.PopFrontFragment();
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(mb1.NumFragments(), 2u);

  result = mb1.PopFrontFragment();
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(mb1.NumFragments(), 1u);

  result = mb1.PopFrontFragment();
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(mb1.NumFragments(), 0u);
}

// TODO(b/425740614): Add NumFragmentsWithLayersMatchesAddedFragments

// TODO(b/425740614): Add a negative compilation test for enumerating layers
// when unlayered

TEST_F(MultiBufTest, NumLayersIsOneWhenEmpty) {
  ConstMultiBufInstance mbi(allocator_);
  ConstMultiBuf& mb = mbi;
  EXPECT_EQ(mb.NumLayers(), 1u);
}

// TODO(b/425740614): Add NumLayersMatchesAddedLayers

TEST_F(MultiBufTest, IterateChunksOverLayers) {
  ConstMultiBufInstance mbi(allocator_);
  AddLayers(mbi);
  size_t i = 0;
  size_t total = 0;
  for (auto chunk : mbi->Chunks()) {
    i++;
    total += chunk.size();
  }
  // See `AddLayers`. Span lengths should be [8, 8, 16].
  EXPECT_EQ(i, 3u);
  EXPECT_EQ(total, 32u);
}

TEST_F(MultiBufTest, IterateBytesOverLayers) {
  ConstMultiBufInstance mbi(allocator_);
  AddLayers(mbi);
  // See `AddLayers`. Span lengths should be [8, 8, 16].
  EXPECT_EQ(mbi->end() - mbi->begin(), 32);
}

// TODO(b/425740614): Add a negative compilation test for adding layers when
// unlayered

TEST_F(MultiBufTest, AddLayerFailsUnableToGrowQueue) {
  ConstMultiBufInstance mbi(allocator_);
  auto chunk = allocator_.MakeUnique<std::byte[]>(kN * 2);
  mbi->PushBack(std::move(chunk));
  allocator_.Exhaust();

  EXPECT_EQ(mbi->NumLayers(), 1u);
  EXPECT_FALSE(mbi->AddLayer(0, 0));
  EXPECT_EQ(mbi->NumLayers(), 1u);
}

TEST_F(MultiBufTest, AddLayerSucceedsWithZeroOffset) {
  ConstMultiBufInstance mbi(allocator_);
  auto chunk = allocator_.MakeUnique<std::byte[]>(kN);
  mbi->PushBack(std::move(chunk));

  EXPECT_TRUE(mbi->AddLayer(0));
  EXPECT_EQ(mbi->size(), kN);
  EXPECT_EQ(mbi->NumLayers(), 2u);

  EXPECT_TRUE(mbi->AddLayer(0));
  EXPECT_EQ(mbi->size(), kN);
  EXPECT_EQ(mbi->NumLayers(), 3u);

  EXPECT_TRUE(mbi->AddLayer(0));
  EXPECT_EQ(mbi->size(), kN);
  EXPECT_EQ(mbi->NumLayers(), 4u);
}

TEST_F(MultiBufTest, AddLayerSucceedsWithNonzeroOffset) {
  ConstMultiBufInstance mbi(allocator_);
  auto chunk = allocator_.MakeUnique<std::byte[]>(kN);
  mbi->PushBack(std::move(chunk));

  EXPECT_TRUE(mbi->AddLayer(2));
  EXPECT_EQ(mbi->size(), kN - 2);
  EXPECT_EQ(mbi->NumLayers(), 2u);

  EXPECT_TRUE(mbi->AddLayer(4));
  EXPECT_EQ(mbi->size(), kN - 6);
  EXPECT_EQ(mbi->NumLayers(), 3u);

  EXPECT_TRUE(mbi->AddLayer(8));
  EXPECT_EQ(mbi->size(), kN - 14);
  EXPECT_EQ(mbi->NumLayers(), 4u);
}

TEST_F(MultiBufTest, AddLayerSucceedsWithNonzeroLength) {
  ConstMultiBufInstance mbi(allocator_);
  auto chunk = allocator_.MakeUnique<std::byte[]>(kN);
  mbi->PushBack(std::move(chunk));

  EXPECT_TRUE(mbi->AddLayer(0, kN - 3));
  EXPECT_EQ(mbi->size(), kN - 3);
  EXPECT_EQ(mbi->NumLayers(), 2u);

  EXPECT_TRUE(mbi->AddLayer(0, kN - 7));
  EXPECT_EQ(mbi->size(), kN - 7);
  EXPECT_EQ(mbi->NumLayers(), 3u);

  EXPECT_TRUE(mbi->AddLayer(0, kN - 11));
  EXPECT_EQ(mbi->size(), kN - 11);
  EXPECT_EQ(mbi->NumLayers(), 4u);
}

TEST_F(MultiBufTest, AddLayerCreatesNewFragment) {
  ConstMultiBufInstance mbi(allocator_);
  auto chunk = allocator_.MakeUnique<std::byte[]>(kN);
  mbi->PushBack(std::move(chunk));
  chunk = allocator_.MakeUnique<std::byte[]>(kN);
  mbi->PushBack(std::move(chunk));
  chunk = allocator_.MakeUnique<std::byte[]>(kN);
  mbi->PushBack(std::move(chunk));

  EXPECT_EQ(mbi->NumFragments(), 3u);
  EXPECT_TRUE(mbi->AddLayer(0));
  EXPECT_EQ(mbi->NumFragments(), 1u);
  mbi->PopLayer();
  EXPECT_EQ(mbi->NumFragments(), 3u);
}

TEST_F(MultiBufTest, ResizeTopLayerSucceedsWithZeroLength) {
  ConstMultiBufInstance mbi(allocator_);
  AddLayers(*mbi);
  EXPECT_EQ(mbi->size(), 32u);
  mbi->ResizeTopLayer(0, 0);
  EXPECT_EQ(mbi->size(), 0u);
}

TEST_F(MultiBufTest, SetLayerSucceedsWithNonzeroLength) {
  ConstMultiBufInstance mbi(allocator_);
  AddLayers(*mbi);
  EXPECT_EQ(mbi->size(), 32u);
  mbi->ResizeTopLayer(6, 12);
  EXPECT_EQ(mbi->size(), 12u);
}

// TODO(b/425740614): Add test where ResizeTopLayer fails when sealed
// TODO(b/425740614): Add test where ResizeTopLayer succeeds after Unseal

// TODO(b/425740614): Add a negative compilation test for popping layers when
// unlayered

TEST_F(MultiBufTest, PopLayerSucceedsWithLayers) {
  ConstMultiBufInstance mbi(allocator_);
  AddLayers(*mbi);

  // See `AddLayers`.
  EXPECT_EQ(mbi->NumFragments(), 2u);
  EXPECT_EQ(mbi->NumLayers(), 3u);
  EXPECT_EQ(mbi->size(), 32u);

  mbi->PopLayer();
  EXPECT_EQ(mbi->NumFragments(), 3u);
  EXPECT_EQ(mbi->NumLayers(), 2u);
  EXPECT_EQ(mbi->size(), 48u);

  mbi->PopLayer();
  EXPECT_EQ(mbi->NumFragments(), 4u);
  EXPECT_EQ(mbi->NumLayers(), 1u);
  EXPECT_EQ(mbi->size(), 64u);
}

// TODO(b/425740614): Add test where PopLayer fails when sealed
// TODO(b/425740614): Add test where PopLayer succeeds after Unseal

TEST_F(MultiBufTest, GetReturnsDataFromTopLayerOnly) {
  ConstMultiBufInstance mbi(allocator_);
  mbi->PushBack(unowned_chunk_);
  for (uint8_t i = 0; i < unowned_chunk_.size(); ++i) {
    unowned_chunk_[i] = static_cast<std::byte>(i);
  }
  ASSERT_TRUE(mbi->AddLayer(3));
  ASSERT_TRUE(mbi->AddLayer(1));
  ASSERT_TRUE(mbi->AddLayer(4));

  std::array<std::byte, kN> tmp;
  pw::ConstByteSpan bytes = mbi->Get(tmp);
  EXPECT_EQ(bytes.size(), unowned_chunk_.size() - 8);
  for (uint8_t i = 0; i < bytes.size(); ++i) {
    EXPECT_EQ(bytes[i], static_cast<std::byte>(i));
  }
}

}  // namespace
