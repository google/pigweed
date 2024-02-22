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

// This size report uses pw::string::Format and std::snprintf to write a single
// printf-style string to a buffer. The number of bytes written is returned.
//
// This compares the overhead of using pw::string::Format to directly calling
// std::snprintf and determining the number of bytes written. It demonstrates
// that the code for using pw::string::Format is much simpler.

#include "pw_allocator/split_free_list_allocator.h"

#include "pw_bloat/bloat_this_binary.h"

#ifdef SIZE_REPORT_WITH_METRICS
#include "pw_allocator/tracking_allocator.h"
#endif  // SIZE_REPORT_WITH_METRICS

namespace {

pw::allocator::SplitFreeListAllocator allocator;

#ifdef SIZE_REPORT_WITH_METRICS
pw::allocator::TrackingAllocator tracker(0);
#endif  // SIZE_REPORT_WITH_METRICS

constexpr void* kFakeMemoryRegionStart = &allocator;
constexpr size_t kFakeMemoryRegionSize = 4096;

constexpr size_t kSplitFreeListThreshold = 128;

}  // namespace

int main() {
  pw::bloat::BloatThisBinary();

  allocator.Init(
      pw::ByteSpan(reinterpret_cast<std::byte*>(kFakeMemoryRegionStart),
                   kFakeMemoryRegionSize),
      kSplitFreeListThreshold);

  struct Foo {
    char name[16];
  };
  struct Bar : public Foo {
    int number;
  };

  // Small allocation.
  Foo* foo =
      static_cast<Foo*>(allocator.Allocate(pw::allocator::Layout::Of<Foo>()));
  if (foo == nullptr) {
    return 1;
  }

  foo->name[0] = '\0';

  // Reallocate.
  Bar* bar = static_cast<Bar*>(
      allocator.Reallocate(foo, pw::allocator::Layout::Of<Foo>(), sizeof(Bar)));
  if (bar == nullptr) {
    return 1;
  }

  bar->number = 4;

  // Large allocation.
  struct Baz {
    std::byte data[kSplitFreeListThreshold * 2];
  };
  Baz* baz =
      static_cast<Baz*>(allocator.Allocate(pw::allocator::Layout::Of<Baz>()));
  if (baz == nullptr) {
    return 1;
  }

  baz->data[kSplitFreeListThreshold] = std::byte(0xf1);

  // Deallocate.
  allocator.Deallocate(bar, pw::allocator::Layout::Of<Bar>());
  allocator.Deallocate(baz, pw::allocator::Layout::Of<Baz>());

#ifdef SIZE_REPORT_UNIQUE_PTR

  struct Point {
    int x;
    int y;

    Point(int xx, int yy) : x(xx), y(yy) {}
  };

  {
    std::optional<pw::allocator::UniquePtr<Point>> maybe_point =
        allocator.MakeUnique<Point>(3, 4);
    if (!maybe_point.has_value()) {
      return 1;
    }

    pw::allocator::UniquePtr<Point> point = *maybe_point;
    point->x = point->y * 2;
  }

#endif  // SIZE_REPORT_UNIQUE_PTR

#ifdef SIZE_REPORT_WITH_METRICS
  tracker.Init(allocator);

  Foo* foo2 =
      static_cast<Foo*>(tracker.Allocate(pw::allocator::Layout::Of<Foo>()));
  if (foo2 == nullptr) {
    return 1;
  }

  foo2->name[1] = 'a';

  tracker.Deallocate(foo2, pw::allocator::Layout::Of<Foo>());
#endif  // SIZE_REPORT_WITH_METRICS

  return 0;
}
