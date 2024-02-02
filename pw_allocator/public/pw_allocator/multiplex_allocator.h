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

#include <cstdint>

#include "pw_allocator/allocator.h"
#include "pw_allocator/metrics.h"
#include "pw_containers/flat_map.h"
#include "pw_tokenizer/tokenize.h"

namespace pw::allocator {

/// Allocator multiplexer that dispatches to a sub-allocator based on a token.
///
/// The tokens are used to identify groups of memory requests and are
/// application-specific. This class returns an (optionally null) pointer to
/// a platform-specific allocator for each token.
///
/// The utility of this class is that it encapsulates the mapping of tokens to
/// allocators and abstracts the platform-specific details away from the
/// application. This allows the application to be more platform-agnostic with
/// respect to dynamic memory allocation, provided the application and set of
/// platforms agree on the set of tokens. More concretely, this allows creating
/// application logic that may work across several generations of a product,
/// even though the dynamic memory details vary between the boards used for each
/// generation.
///
/// This specific class is meant to be a generalized base class. It should only
/// be used if custom logic is needed to map tokens to allocators, e.g. if the
/// allocator choice depends on additional external conditions. In most cases,
/// applications should be able to simply map tokens to allocator pointers, in
/// which case they should prefer `FlatMapMultiplexAllocatorImpl<metrics, size>`
/// or `MultiplexAllocator<size>`.
///
/// Additionally, this class should only be used directly if the metrics type
/// needs to be specified explicitly, i.e. in unit tests. In any other case,
/// callers should use `MultiplexAllocatorBase`, which selects the metrics type
/// based on build arguments.
template <typename MetricsType>
class MultiplexAllocatorImpl : public WithMetrics<MetricsType> {
 public:
  using metrics_type = MetricsType;
  using allocator_type = AllocatorWithMetrics<metrics_type>;
  using Token = tokenizer::Token;

  constexpr MultiplexAllocatorImpl(Token token) : metrics_(token) {}

  metrics_type& metric_group() override { return metrics_; }
  const metrics_type& metric_group() const override { return metrics_; }

  /// Returns the allocator for a given application-specific type identifier.
  allocator_type* GetAllocator(Token token) { return DoGetAllocator(token); }

  /// Returns the result of calling `Allocate` on the allocator associated with
  /// the given `type_id`, if any; otherwise returns null.
  void* Allocate(Token token, Layout layout) {
    allocator_type* allocator = GetAllocator(token);
    void* ptr = allocator == nullptr ? nullptr : allocator->Allocate(layout);
    if (ptr != nullptr) {
      metrics_.RecordAllocation(layout.size());
    }
    return ptr;
  }

  /// Dispatches to the `Deallocate` method on the allocator associated with
  /// the given `type_id`, if any.
  void Deallocate(Token token, void* ptr, Layout layout) {
    allocator_type* allocator = ptr == nullptr ? nullptr : GetAllocator(token);
    if (allocator != nullptr) {
      allocator->Deallocate(ptr, layout);
      metrics_.RecordDeallocation(layout.size());
    }
  }

  /// Returns the result of calling `Resize` on the allocator associated with
  /// the given `type_id`, if any; otherwise returns false.
  bool Resize(Token token, void* ptr, Layout old_layout, size_t new_size) {
    allocator_type* allocator = GetAllocator(token);
    bool resized = allocator == nullptr
                       ? false
                       : allocator->Resize(ptr, old_layout, new_size);
    if (resized) {
      metrics_.RecordResize(old_layout.size(), new_size);
    }
    return resized;
  }

  /// Returns the result of calling `Reallocate` on the allocator associated
  /// with the given `type_id`, if any; otherwise returns null.
  void* Reallocate(Token token, void* ptr, Layout old_layout, size_t new_size) {
    allocator_type* allocator = GetAllocator(token);
    void* new_ptr = allocator == nullptr
                        ? nullptr
                        : allocator->Reallocate(ptr, old_layout, new_size);
    if (new_ptr != nullptr) {
      metrics_.RecordReallocation(old_layout.size(), new_size, new_ptr != ptr);
    }
    return new_ptr;
  }

 protected:
  /// Includes the metrics group for the given allocator in this object.
  void AddMetrics(allocator_type* allocator) {
    metrics_.Add(allocator->metric_group());
  }

 private:
  /// Implementation of `GetAllocator`.
  ///
  /// Applications may provide derived classes which override this method to
  /// return application-specific allocators for application-specific type
  /// identifiers.
  ///
  /// If the requested type identifier is unrecognized, this method should
  /// return null.
  virtual allocator_type* DoGetAllocator(Token token) = 0;

  metrics_type metrics_;
};

/// Allocator multiplexer backed by a flat map.
///
/// This class provides for the simple construction of multiplexed allocators
/// that simply map from token to allocator. The mapping does not need to be
/// one-to-one, and some tokens may map to `nullptr`. Initialization is
/// accomplished by constructing an array from an initializer list of pairs.
/// For example:
///
/// @code{.cpp}
///    FlatMapMultiplexAllocatorImpl<internal::Metrics, 4> allocator(kToken,
///      {{{kFoo, &foo}, {kBar, &bar}, {kBaz, &bar}, {kQux, nullptr}}});
/// @endcode
///
/// This class should only be used directly if the metrics type needs to be
/// specified explicitly, i.e. in unit tests. In any other case, callers should
/// use `MultiplexAllocator<size>`, which selects the metrics type based
/// on build arguments.
template <typename MetricsType, size_t kArraySize>
class FlatMapMultiplexAllocatorImpl
    : public MultiplexAllocatorImpl<MetricsType> {
 public:
  using Base = MultiplexAllocatorImpl<MetricsType>;
  using Token = typename Base::Token;
  using allocator_type = typename Base::allocator_type;
  using pair_type = containers::Pair<Token, allocator_type*>;
  using map_type = containers::FlatMap<Token, allocator_type*, kArraySize>;
  using const_iterator = typename map_type::const_iterator;

  FlatMapMultiplexAllocatorImpl(Token token,
                                const std::array<pair_type, kArraySize>& pairs)
      : Base(token), map_(pairs) {
    for (const_iterator i = map_.begin(); i != map_.end(); ++i) {
      // Automatically add metrics from mapped allocators. Multiple tokens may
      // map to the same allocators, and some tokens may map to no allocator at
      // all. Scan the remainder of the array and only add those that point to
      // a valid allocator that is not subsequently duplicated.
      allocator_type* allocator = i->second;
      for (const_iterator j = i; allocator != nullptr && j != map_.end(); ++j) {
        if (j->second == allocator) {
          allocator = nullptr;
        }
      }
      if (allocator != nullptr) {
        Base::AddMetrics(allocator);
      }
    }
  }

 private:
  allocator_type* DoGetAllocator(Token token) override {
    auto pair = map_.find(token);
    return pair == map_.end() ? nullptr : pair->second;
  }

  map_type map_;
};

/// Multiplexed allocator that uses the default metrics implementation.
///
/// This class can be used as the base class for a multiplexed allocator that
/// requires custom allocator selection logic.
///
/// Depending on the value of the `pw_allocator_COLLECT_METRICS` build argument,
/// the `internal::DefaultMetrics` type is an alias for either the real or stub
/// metrics implementation.
using MultiplexAllocator = MultiplexAllocatorImpl<internal::DefaultMetrics>;

/// Multiplexed allocator that uses the default metrics implementation.
///
/// This class can be used as the base class for a multiplexed allocator that
/// uses a simple mapping of tokens to allocator pointers.
///
/// Depending on the value of the `pw_allocator_COLLECT_METRICS` build argument,
/// the `internal::DefaultMetrics` type is an alias for either the real or stub
/// metrics implementation.
template <size_t kArraySize>
using FlatMapMultiplexAllocator =
    FlatMapMultiplexAllocatorImpl<internal::DefaultMetrics, kArraySize>;

}  // namespace pw::allocator
