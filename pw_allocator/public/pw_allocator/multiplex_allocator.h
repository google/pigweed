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
/// allocator choice depends on additional external conditions. In many cases,
/// applications should be able to simply map tokens to allocator pointers, in
/// which case they should prefer `FlatMapMultiplexAllocator<size>`.
class MultiplexAllocator {
 public:
  using Token = tokenizer::Token;

  constexpr explicit MultiplexAllocator() = default;
  virtual ~MultiplexAllocator() = default;

  /// Returns the allocator for a given application-specific type identifier.
  Allocator* GetAllocator(Token token) { return DoGetAllocator(token); }

  /// Returns the result of calling `Allocate` on the allocator associated with
  /// the given `type_id`, if any; otherwise returns null.
  void* Allocate(Token token, Layout layout) {
    Allocator* allocator = GetAllocator(token);
    return allocator == nullptr ? nullptr : allocator->Allocate(layout);
  }

  /// Dispatches to the `Deallocate` method on the allocator associated with
  /// the given `type_id`, if any.
  void Deallocate(Token token, void* ptr, Layout layout) {
    Allocator* allocator = GetAllocator(token);
    if (allocator != nullptr) {
      allocator->Deallocate(ptr, layout);
    }
  }

  /// Returns the result of calling `Resize` on the allocator associated with
  /// the given `type_id`, if any; otherwise returns false.
  bool Resize(Token token, void* ptr, Layout old_layout, size_t new_size) {
    Allocator* allocator = GetAllocator(token);
    return allocator != nullptr && allocator->Resize(ptr, old_layout, new_size);
  }

  /// Returns the result of calling `Reallocate` on the allocator associated
  /// with the given `type_id`, if any; otherwise returns null.
  void* Reallocate(Token token, void* ptr, Layout old_layout, size_t new_size) {
    Allocator* allocator = GetAllocator(token);
    return allocator == nullptr
               ? nullptr
               : allocator->Reallocate(ptr, old_layout, new_size);
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
  virtual Allocator* DoGetAllocator(Token token) = 0;
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
///    FlatMapMultiplexAllocator<4> allocator(kToken,
///      {{{kFoo, &foo}, {kBar, &bar}, {kBaz, &bar}, {kQux, nullptr}}});
/// @endcode
template <size_t kArraySize>
class FlatMapMultiplexAllocator : public MultiplexAllocator {
 public:
  using Token = tokenizer::Token;
  using pair_type = containers::Pair<Token, Allocator*>;

  FlatMapMultiplexAllocator(const std::array<pair_type, kArraySize>& pairs)
      : MultiplexAllocator(), map_(pairs) {}

 private:
  Allocator* DoGetAllocator(Token token) override {
    auto pair = map_.find(token);
    return pair == map_.end() ? nullptr : pair->second;
  }

  containers::FlatMap<Token, Allocator*, kArraySize> map_;
};

}  // namespace pw::allocator
