// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_FIT_INCLUDE_LIB_FIT_FUNCTION_INTERNAL_H_
#define LIB_FIT_INCLUDE_LIB_FIT_FUNCTION_INTERNAL_H_

#include <stddef.h>
#include <stdlib.h>

#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "nullable.h"
#include "pw_assert/assert.h"
#include "pw_preprocessor/compiler.h"

namespace fit {
namespace internal {

template <typename Result, typename... Args>
struct target_ops final {
  const void* (*target_type_id)(void* bits, const void* impl_ops);
  void* (*get)(void* bits);
  Result (*invoke)(void* bits, Args... args);
  void (*move)(void* from_bits, void* to_bits);
  void (*destroy)(void* bits);
};

template <typename Callable, bool is_inline, bool is_shared, typename Result, typename... Args>
struct target;

inline const void* unshared_target_type_id(void* /*bits*/, const void* impl_ops) {
  return impl_ops;
}

// vtable for nullptr (empty target function)

template <typename Result, typename... Args>
struct target<decltype(nullptr),
              /*is_inline=*/true, /*is_shared=*/false, Result, Args...>
    final {
  static Result invoke(void* /*bits*/, Args... /*args*/) { PW_ASSERT(false); }

  static const target_ops<Result, Args...> ops;
};

inline void* null_target_get(void* /*bits*/) { return nullptr; }
inline void null_target_move(void* /*from_bits*/, void* /*to_bits*/) {}
inline void null_target_destroy(void* /*bits*/) {}

template <typename Result, typename... Args>
constexpr target_ops<Result, Args...> target<decltype(nullptr),
                                             /*is_inline=*/true,
                                             /*is_shared=*/false, Result, Args...>::ops = {
    &unshared_target_type_id, &null_target_get, &target::invoke, &null_target_move,
    &null_target_destroy};

// vtable for inline target function

template <typename Callable, typename Result, typename... Args>
struct target<Callable,
              /*is_inline=*/true, /*is_shared=*/false, Result, Args...>
    final {
  template <typename Callable_>
  static void initialize(void* bits, Callable_&& target) {
    new (bits) Callable(std::forward<Callable_>(target));
  }
  static Result invoke(void* bits, Args... args) {
    auto& target = *static_cast<Callable*>(bits);
    return target(std::forward<Args>(args)...);
  }
  static void move(void* from_bits, void* to_bits) {
    auto& from_target = *static_cast<Callable*>(from_bits);
    new (to_bits) Callable(std::move(from_target));
    from_target.~Callable();  // NOLINT(bugprone-use-after-move)
  }
  static void destroy(void* bits) {
    auto& target = *static_cast<Callable*>(bits);
    target.~Callable();
  }

  static const target_ops<Result, Args...> ops;
};

inline void* inline_target_get(void* bits) { return bits; }

template <typename Callable, typename Result, typename... Args>
constexpr target_ops<Result, Args...> target<Callable,
                                             /*is_inline=*/true,
                                             /*is_shared=*/false, Result, Args...>::ops = {
    &unshared_target_type_id, &inline_target_get, &target::invoke, &target::move, &target::destroy};

// vtable for pointer to target function

template <typename Callable, typename Result, typename... Args>
struct target<Callable,
              /*is_inline=*/false, /*is_shared=*/false, Result, Args...>
    final {
  template <typename Callable_>
  static void initialize(void* bits, Callable_&& target) {
    auto ptr = static_cast<Callable**>(bits);
    *ptr = new Callable(std::forward<Callable_>(target));
  }
  static Result invoke(void* bits, Args... args) {
    auto& target = **static_cast<Callable**>(bits);
    return target(std::forward<Args>(args)...);
  }
  static void move(void* from_bits, void* to_bits) {
    auto from_ptr = static_cast<Callable**>(from_bits);
    auto to_ptr = static_cast<Callable**>(to_bits);
    *to_ptr = *from_ptr;
  }
  static void destroy(void* bits) {
    auto ptr = static_cast<Callable**>(bits);
    delete *ptr;
  }

  static const target_ops<Result, Args...> ops;
};

inline void* heap_target_get(void* bits) { return *static_cast<void**>(bits); }

template <typename Callable, typename Result, typename... Args>
constexpr target_ops<Result, Args...> target<Callable,
                                             /*is_inline=*/false,
                                             /*is_shared=*/false, Result, Args...>::ops = {
    &unshared_target_type_id, &heap_target_get, &target::invoke, &target::move, &target::destroy};

// vtable for fit::function std::shared_ptr to target function

template <typename SharedFunction>
const void* get_target_type_id(const SharedFunction& function_or_callback) {
  return function_or_callback.target_type_id();
}

// For this vtable,
// Callable by definition will be either a fit::function or fit::callback
template <typename SharedFunction, typename Result, typename... Args>
struct target<SharedFunction,
              /*is_inline=*/false, /*is_shared=*/true, Result, Args...>
    final {
  static void initialize(void* bits, SharedFunction target) {
    new (bits) std::shared_ptr<SharedFunction>(
        std::move(std::make_shared<SharedFunction>(std::move(target))));
  }
  static void copy_shared_ptr(void* from_bits, void* to_bits) {
    auto& from_shared_ptr = *static_cast<std::shared_ptr<SharedFunction>*>(from_bits);
    new (to_bits) std::shared_ptr<SharedFunction>(from_shared_ptr);
  }
  static const void* target_type_id(void* bits, const void* /*impl_ops*/) {
    auto& function_or_callback = **static_cast<std::shared_ptr<SharedFunction>*>(bits);
    return ::fit::internal::get_target_type_id(function_or_callback);
  }
  static void* get(void* bits) {
    auto& function_or_callback = **static_cast<std::shared_ptr<SharedFunction>*>(bits);
    return function_or_callback.template target<SharedFunction>(
        /*check=*/false);  // void* will fail the check
  }
  static Result invoke(void* bits, Args... args) {
    auto& function_or_callback = **static_cast<std::shared_ptr<SharedFunction>*>(bits);
    return function_or_callback(std::forward<Args>(args)...);
  }
  static void move(void* from_bits, void* to_bits) {
    auto from_shared_ptr = std::move(*static_cast<std::shared_ptr<SharedFunction>*>(from_bits));
    new (to_bits) std::shared_ptr<SharedFunction>(std::move(from_shared_ptr));
  }
  static void destroy(void* bits) { static_cast<std::shared_ptr<SharedFunction>*>(bits)->reset(); }

  static const target_ops<Result, Args...> ops;
};

template <typename SharedFunction, typename Result, typename... Args>
constexpr target_ops<Result, Args...> target<SharedFunction,
                                             /*is_inline=*/false,
                                             /*is_shared=*/true, Result, Args...>::ops = {
    &target::target_type_id, &target::get, &target::invoke, &target::move, &target::destroy};

template <size_t inline_target_size, bool require_inline, typename Callable>
class function_base;

// Function implementation details.
// See |fit::function| and |fit::callback| documentation for more information.
template <size_t inline_target_size, bool require_inline, typename Result, typename... Args>
class function_base<inline_target_size, require_inline, Result(Args...)> {
  static constexpr size_t storage_size =
      (inline_target_size >= sizeof(void*) ? inline_target_size : sizeof(void*));
  // avoid including <algorithm> for max

  using ops_type = const target_ops<Result, Args...>*;
  struct alignas(max_align_t) storage_type {
    union {
      // Function context data, placed first in the struct since is has a more
      // strict alignment requirement than ops.
      mutable uint8_t bits[storage_size];

      // Empty struct used when initializing the storage in the constrexpr
      // constructor.
      struct {
      } null_bits;
    };

    // The target_ops pointer for this function. This field has lower alignment
    // requirement than bits, so placing ops after bits allows for better
    // packing reducing the padding needed in some cases.
    ops_type ops;
  };

  // bits field should have a max_align_t alignment, but adding the alignas()
  // at the field declaration increases the padding. Make sure the alignment is
  // correct nevertheless.
  static_assert(offsetof(storage_type, bits) % std::alignment_of<max_align_t>::value == 0);

  // Check that there's no unexpected extra padding.
  static_assert(sizeof(storage_type) ==
                    (storage_size + sizeof(ops_type) + std::alignment_of<max_align_t>::value - 1u) /
                        std::alignment_of<max_align_t>::value *
                        std::alignment_of<max_align_t>::value,
                "storage_type is not minimal in size");

  template <typename Callable>
  using target_type = target<Callable, (sizeof(Callable) <= storage_size),
                             /*is_shared=*/false, Result, Args...>;
  template <typename SharedFunction>
  using shared_target_type = target<SharedFunction,
                                    /*is_inline=*/false,
                                    /*is_shared=*/true, Result, Args...>;
  using null_target_type = target_type<decltype(nullptr)>;

 public:
  // Deleted copy constructor and assign. |function_base| implementations are
  // move-only.
  function_base(const function_base& other) = delete;
  function_base& operator=(const function_base& other) = delete;

  // Move assignment must be provided by subclasses.
  function_base& operator=(function_base&& other) = delete;

 protected:
  using result_type = Result;

  constexpr function_base() : storage_({.null_bits = {}, .ops = &null_target_type::ops}) {}

  constexpr function_base(decltype(nullptr)) : function_base() {}

  function_base(Result (*function_target)(Args...)) { initialize_target(function_target); }

  template <typename Callable,
            typename = std::enable_if_t<std::is_convertible<
                decltype(std::declval<Callable&>()(std::declval<Args>()...)), result_type>::value>>
  function_base(Callable&& target) {
    initialize_target(std::forward<Callable>(target));
  }

  function_base(function_base&& other) { move_target_from(std::move(other)); }

  ~function_base() { destroy_target(); }

  // Returns true if the function has a non-empty target.
  explicit operator bool() const { return storage_.ops->get(&storage_.bits) != nullptr; }

  // Returns a pointer to the function's target.
  // If |check| is true (the default), the function _may_ abort if the
  // caller tries to assign the target to a varible of the wrong type. (This
  // check is currently skipped for share()d objects.)
  // Note the shared pointer vtable must set |check| to false to assign the
  // target to |void*|.
  template <typename Callable>
  Callable* target(bool check = true) {
    if (check)
      check_target_type<Callable>();
    return static_cast<Callable*>(storage_.ops->get(&storage_.bits));
  }

  // Returns a pointer to the function's target (const version).
  // If |check| is true (the default), the function _may_ abort if the
  // caller tries to assign the target to a varible of the wrong type. (This
  // check is currently skipped for share()d objects.)
  // Note the shared pointer vtable must set |check| to false to assign the
  // target to |void*|.
  template <typename Callable>
  const Callable* target(bool check = true) const {
    if (check)
      check_target_type<Callable>();
    return static_cast<Callable*>(storage_.ops->get(&storage_.bits));
  }

  // Used by the derived "impl" classes to implement share().
  //
  // The caller creates a new object of the same type as itself, and passes in
  // the empty object. This function first checks if |this| is already shared,
  // and if not, creates a new version of itself containing a
  // |std::shared_ptr| to its original self, and updates |storage_.ops| to the
  // vtable for the shared version.
  //
  // Then it copies its |shared_ptr| to the |storage_.bits| of the given |copy|,
  // and assigns the same shared pointer vtable to the copy's |storage_.ops|.
  //
  // The target itself is not copied; it is moved to the heap and its
  // lifetime is extended until all references have been released.
  //
  // Note: This method is not supported on |fit::inline_function<>|
  //       because it may incur a heap allocation which is contrary to
  //       the stated purpose of |fit::inline_function<>|.
  template <typename SharedFunction>
  void share_with(SharedFunction& copy) {
    static_assert(!require_inline, "Inline functions cannot be shared.");
    if (storage_.ops->get(&storage_.bits) != nullptr) {
      if (storage_.ops != &shared_target_type<SharedFunction>::ops) {
        convert_to_shared_target<SharedFunction>();
      }
      copy_shared_target_to(copy);
    }
  }

  // Used by derived "impl" classes to implement operator()().
  // Invokes the function's target.
  // Note that fit::callback will release the target immediately after
  // invoke() (also affecting any share()d copies).
  // Aborts if the function's target is empty.
  // TODO(b/241567321): Remove "no sanitize" after pw_protobuf is fixed.
  Result invoke(Args... args) const PW_NO_SANITIZE("function") {
    return storage_.ops->invoke(&storage_.bits, std::forward<Args>(args)...);
  }

  // Used by derived "impl" classes to implement operator=().
  // Assigns an empty target.
  void assign(decltype(nullptr)) {
    destroy_target();
    initialize_null_target();
  }

  // Used by derived "impl" classes to implement operator=().
  // Assigns the function's target.
  // If target == nullptr, assigns an empty target.
  template <typename Callable,
            typename = std::enable_if_t<std::is_convertible<
                decltype(std::declval<Callable&>()(std::declval<Args>()...)), result_type>::value>>
  void assign(Callable&& target) {
    destroy_target();
    initialize_target(std::forward<Callable>(target));
  }

  // Used by derived "impl" classes to implement operator=().
  // Assigns the function with a target moved from another function,
  // leaving the other function with an empty target.
  void assign(function_base&& other) {
    destroy_target();
    move_target_from(std::move(other));
  }

  void swap(function_base& other) {
    if (&other == this)
      return;
    storage_type temp_storage;
    move_storage(storage_, temp_storage);
    move_storage(other.storage_, storage_);
    move_storage(temp_storage, other.storage_);
  }

  // returns an opaque ID unique to the |Callable| type of the target.
  // Used by check_target_type.
  const void* target_type_id() const {
    return storage_.ops->target_type_id(&storage_.bits, storage_.ops);
  }

 private:
  // Moves the storage_type from one to another using the source's ops move()
  // operation. The source storage is unnafected.
  static void move_storage(const storage_type& from_storage, storage_type& to_storage) {
    to_storage.ops = from_storage.ops;
    from_storage.ops->move(&from_storage.bits, &to_storage.bits);
  }

  // Implements the move operation, used by move construction and move
  // assignment. Leaves other target initialized to null.
  void move_target_from(function_base&& other) {
    move_storage(other.storage_, storage_);
    other.initialize_null_target();
  }

  // fit::function and fit::callback are not directly copyable, but share()
  // will create shared references to the original object. This method
  // implements the copy operation for the |std::shared_ptr| wrapper.
  template <typename SharedFunction>
  void copy_shared_target_to(SharedFunction& copy) {
    copy.destroy_target();
    assert(storage_.ops == &shared_target_type<SharedFunction>::ops);
    shared_target_type<SharedFunction>::copy_shared_ptr(&storage_.bits, &copy.storage_.bits);
    copy.storage_.ops = storage_.ops;
  }

  // assumes target is uninitialized
  void initialize_null_target() { storage_.ops = &null_target_type::ops; }

  // target may or may not be initialized.
  template <typename Callable>
  void initialize_target(Callable&& target) {
    // Convert function or function references to function pointer.
    using DecayedCallable = std::decay_t<Callable>;
    static_assert(
        std::alignment_of<DecayedCallable>::value <= std::alignment_of<max_align_t>::value,
        "Alignment of Callable must be <= alignment of max_align_t.");
    static_assert(!require_inline || sizeof(DecayedCallable) <= inline_target_size,
                  "Callable too large to store inline as requested.");
    if (is_null(target)) {
      initialize_null_target();
    } else {
      storage_.ops = &target_type<DecayedCallable>::ops;
      target_type<DecayedCallable>::initialize(&storage_.bits, std::forward<Callable>(target));
    }
  }

  // assumes target is uninitialized
  template <typename SharedFunction>
  void convert_to_shared_target() {
    shared_target_type<SharedFunction>::initialize(&storage_.bits,
                                                   std::move(*static_cast<SharedFunction*>(this)));
    storage_.ops = &shared_target_type<SharedFunction>::ops;
  }

  // leaves target uninitialized
  void destroy_target() { storage_.ops->destroy(&storage_.bits); }

  // Called by target() if |check| is true.
  // Checks the template parameter, usually inferred from the context of
  // the call to target(), and aborts the program if it can determine that
  // the Callable type is not compatible with the function's Result and Args.
  template <typename Callable>
  void check_target_type() const {
    if (target_type<Callable>::ops.target_type_id(nullptr, &target_type<Callable>::ops) !=
        target_type_id()) {
      PW_ASSERT(false);
    }
  }

  // The combined context data and target_ops storage.
  storage_type storage_;
};

}  // namespace internal

}  // namespace fit

#endif  // LIB_FIT_INCLUDE_LIB_FIT_FUNCTION_INTERNAL_H_
