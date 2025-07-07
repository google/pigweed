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

#include "pw_multibuf/chunk_iterator.h"

#include "pw_multibuf/internal/iterator_testing.h"
#include "pw_unit_test/framework.h"

// Test fixtures.

namespace {

using ::pw::multibuf_impl::Chunks;
using ::pw::multibuf_impl::IteratorTest;

template <typename IteratorType>
class ChunkIteratorTestImpl : public IteratorTest {
 private:
  using FlippedType =
      std::conditional_t<std::is_same_v<IteratorType, Chunks<>::iterator>,
                         Chunks<>::const_iterator,
                         Chunks<>::iterator>;

 protected:
  ChunkIteratorTestImpl() {
    second_ = chunks().begin();
    flipped_ = chunks().begin();
    first_ = second_++;
    last_ = chunks().end();
    past_the_end_ = last_--;
  }

  // Unit tests.
  void IndirectionOperatorDereferencesToByteSpan();
  void MemberOfOperatorDereferencesToByteSpan();
  void CanIterateUsingPrefixIncrement();
  void CanIterateUsingPostfixIncrement();
  void CanIterateUsingPrefixDecrement();
  void CanIterateUsingPostfixDecrement();
  void CanCompareIteratorsUsingEqual();
  void CanCompareIteratorsUsingNotEqual();

  Chunks<> chunks_;
  IteratorType first_;
  FlippedType flipped_;
  IteratorType second_;
  IteratorType last_;
  IteratorType past_the_end_;
};

using ChunkIteratorTest = ChunkIteratorTestImpl<Chunks<>::iterator>;
using ChunkConstIteratorTest = ChunkIteratorTestImpl<Chunks<>::const_iterator>;
using ChunksTest = IteratorTest;

// Template method implementations.

TEST_F(ChunkIteratorTest, CheckFixture) {}
TEST_F(ChunkConstIteratorTest, CheckFixture) {}

static_assert(sizeof(pw::multibuf_impl::Entry) == sizeof(std::byte*));

template <typename IteratorType>
void ChunkIteratorTestImpl<
    IteratorType>::IndirectionOperatorDereferencesToByteSpan() {
  pw::ConstByteSpan actual = *first_;
  pw::ConstByteSpan expected = GetContiguous(0);
  EXPECT_EQ(actual.data(), expected.data());
  EXPECT_EQ(actual.size(), expected.size());
}
TEST_F(ChunkIteratorTest, IndirectionOperatorDereferencesToByteSpan) {
  IndirectionOperatorDereferencesToByteSpan();
}
TEST_F(ChunkConstIteratorTest, IndirectionOperatorDereferencesToByteSpan) {
  IndirectionOperatorDereferencesToByteSpan();
}

template <typename IteratorType>
void ChunkIteratorTestImpl<
    IteratorType>::MemberOfOperatorDereferencesToByteSpan() {
  pw::ConstByteSpan expected = GetContiguous(0);
  EXPECT_EQ(first_->data(), expected.data());
  EXPECT_EQ(first_->size(), expected.size());
}
TEST_F(ChunkIteratorTest, MemberOfOperatorDereferencesToByteSpan) {
  MemberOfOperatorDereferencesToByteSpan();
}
TEST_F(ChunkConstIteratorTest, MemberOfOperatorDereferencesToByteSpan) {
  MemberOfOperatorDereferencesToByteSpan();
}

template <typename IteratorType>
void ChunkIteratorTestImpl<IteratorType>::CanIterateUsingPrefixIncrement() {
  IteratorType iter = first_;
  for (size_t i = 0; i < kNumContiguous; ++i) {
    pw::ConstByteSpan expected = GetContiguous(i);
    EXPECT_EQ(iter->data(), expected.data());
    EXPECT_EQ(iter->size(), expected.size());
    ++iter;
  }
  EXPECT_EQ(iter, past_the_end_);
}
TEST_F(ChunkIteratorTest, CanIterateUsingPrefixIncrement) {
  CanIterateUsingPrefixIncrement();
}
TEST_F(ChunkConstIteratorTest, CanIterateUsingPrefixIncrement) {
  CanIterateUsingPrefixIncrement();
}

template <typename IteratorType>
void ChunkIteratorTestImpl<IteratorType>::CanIterateUsingPostfixIncrement() {
  IteratorType iter = first_;
  IteratorType copy;
  for (size_t i = 0; i < kNumContiguous; ++i) {
    pw::ConstByteSpan expected = GetContiguous(i);
    copy = iter++;
    EXPECT_EQ(copy->data(), expected.data());
    EXPECT_EQ(copy->size(), expected.size());
  }
  EXPECT_EQ(copy, last_);
  EXPECT_EQ(iter, past_the_end_);
}
TEST_F(ChunkIteratorTest, CanIterateUsingPostfixIncrement) {
  CanIterateUsingPostfixIncrement();
}
TEST_F(ChunkConstIteratorTest, CanIterateUsingPostfixIncrement) {
  CanIterateUsingPostfixIncrement();
}

template <typename IteratorType>
void ChunkIteratorTestImpl<IteratorType>::CanIterateUsingPrefixDecrement() {
  IteratorType iter = past_the_end_;
  for (size_t i = 1; i <= kNumContiguous; ++i) {
    pw::ConstByteSpan expected = GetContiguous(kNumContiguous - i);
    --iter;
    EXPECT_EQ(iter->data(), expected.data());
    EXPECT_EQ(iter->size(), expected.size());
  }
  EXPECT_EQ(iter, first_);
}
TEST_F(ChunkIteratorTest, CanIterateUsingPrefixDecrement) {
  CanIterateUsingPrefixDecrement();
}
TEST_F(ChunkConstIteratorTest, CanIterateUsingPrefixDecrement) {
  CanIterateUsingPrefixDecrement();
}

template <typename IteratorType>
void ChunkIteratorTestImpl<IteratorType>::CanIterateUsingPostfixDecrement() {
  IteratorType iter = last_;
  for (size_t i = 1; i < kNumContiguous; ++i) {
    pw::ConstByteSpan expected = GetContiguous(kNumContiguous - i);
    auto copy = iter--;
    EXPECT_EQ(copy->data(), expected.data());
    EXPECT_EQ(copy->size(), expected.size());
  }
  EXPECT_EQ(iter, first_);
}
TEST_F(ChunkIteratorTest, CanIterateUsingPostfixDecrement) {
  CanIterateUsingPostfixDecrement();
}
TEST_F(ChunkConstIteratorTest, CanIterateUsingPostfixDecrement) {
  CanIterateUsingPostfixDecrement();
}

template <typename IteratorType>
void ChunkIteratorTestImpl<IteratorType>::CanCompareIteratorsUsingEqual() {
  EXPECT_EQ(first_, first_);
  EXPECT_EQ(first_, flipped_);
  EXPECT_EQ(past_the_end_, past_the_end_);
}
TEST_F(ChunkIteratorTest, CanCompareIteratorsUsingEqual) {
  CanCompareIteratorsUsingEqual();
}
TEST_F(ChunkConstIteratorTest, CanCompareIteratorsUsingEqual) {
  CanCompareIteratorsUsingEqual();
}

template <typename IteratorType>
void ChunkIteratorTestImpl<IteratorType>::CanCompareIteratorsUsingNotEqual() {
  EXPECT_NE(first_, second_);
  EXPECT_NE(flipped_, second_);
  EXPECT_NE(first_, past_the_end_);
}
TEST_F(ChunkIteratorTest, CanCompareIteratorsUsingNotEqual) {
  CanCompareIteratorsUsingNotEqual();
}
TEST_F(ChunkConstIteratorTest, CanCompareIteratorsUsingNotEqual) {
  CanCompareIteratorsUsingNotEqual();
}

TEST_F(ChunksTest, CanIterateUsingRangeBasedForLoop) {
  size_t i = 0;
  for (auto actual : chunks()) {
    pw::ConstByteSpan expected = GetContiguous(i++);
    EXPECT_EQ(actual.data(), expected.data());
    EXPECT_EQ(actual.size(), expected.size());
  }
}

}  // namespace
