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

#include <cstddef>
#include <optional>
#include <utility>

#include "pw_status/status.h"

namespace pw::allocator {
/// Describes the layout of a block of memory.
///
/// Layouts are passed to allocators, and consist of a (possibly padded) size
/// and a power-of-two alignment no larger than the size. Layouts can be
/// constructed for a type `T` using `Layout::Of`.
///
/// Example:
///
/// @code{.cpp}
///    struct MyStruct {
///      uint8_t field1[3];
///      uint32_t field2[3];
///    };
///    constexpr Layout layout_for_struct = Layout::Of<MyStruct>();
/// @endcode
class Layout {
 public:
  constexpr Layout() = default;
  constexpr Layout(size_t size, size_t alignment = alignof(std::max_align_t))
      : size_(size), alignment_(alignment) {}

  /// Creates a Layout for the given type.
  template <typename T>
  static constexpr Layout Of() {
    return Layout(sizeof(T), alignof(T));
  }

  constexpr Layout Extend(size_t size) {
    return Layout(size_ + size, alignment_);
  }

  size_t size() const { return size_; }
  size_t alignment() const { return alignment_; }

 private:
  size_t size_ = 0;
  size_t alignment_ = 1;
};

template <typename T>
class UniquePtr;

/// Abstract interface for memory allocation.
///
/// This is the most generic and fundamental interface provided by the
/// module, representing any object capable of dynamic memory allocation. Other
/// interfaces may inherit from a base generic Allocator and provide different
/// allocator properties.
///
/// The interface makes no guarantees about its implementation. Consumers of the
/// generic interface must not make any assumptions around allocator behavior,
/// thread safety, or performance.
///
/// NOTE: This interface is in development and should not be considered stable.
class Allocator {
 public:
  constexpr Allocator() = default;
  virtual ~Allocator() = default;

  /// Asks the allocator if it is capable of realloating or deallocating a given
  /// pointer.
  ///
  /// NOTE: This method is in development and should not be considered stable.
  /// Do NOT use it in its current form to determine if this allocator can
  /// deallocate pointers. Callers MUST only `Deallocate` memory using the same
  /// `Allocator` they used to `Allocate` it. This method is currently for
  /// internal use only.
  ///
  /// TODO: b/301677395 - Add dynamic type information to support a
  /// `std::pmr`-style `do_is_equal`. Without this information, it is not
  /// possible to determine whether another allocator has applied additional
  /// constraints to memory that otherwise may appear to be associated with this
  /// allocator.
  ///
  /// @param[in]  ptr         The pointer to be queried.
  /// @param[in]  layout      Describes the memory pointed at by `ptr`.
  ///
  /// @retval UNIMPLEMENTED         This object cannot recognize allocated
  ///                               pointers.
  /// @retval OUT_OF_RANGE          Pointer cannot be re/deallocated by this
  ///                               object.
  /// @retval OK                    This object can re/deallocate the pointer.
  Status Query(const void* ptr, Layout layout) const {
    return DoQuery(ptr, layout);
  }

  /// Allocates a block of memory with the specified size and alignment.
  ///
  /// Returns `nullptr` if the allocation cannot be made, or the `layout` has a
  /// size of 0.
  ///
  /// @param[in]  layout      Describes the memory to be allocated.
  void* Allocate(Layout layout) { return DoAllocate(layout); }

  template <typename T, typename... Args>
  std::optional<UniquePtr<T>> MakeUnique(Args&&... args) {
    static constexpr Layout kStaticLayout = Layout::Of<T>();
    void* void_ptr = Allocate(kStaticLayout);
    if (void_ptr == nullptr) {
      return std::nullopt;
    }
    T* ptr = new (void_ptr) T(std::forward<Args>(args)...);
    return std::make_optional<UniquePtr<T>>(
        UniquePtr<T>::kPrivateConstructor, ptr, &kStaticLayout, this);
  }

  /// Releases a previously-allocated block of memory.
  ///
  /// The given pointer must have been previously obtained from a call to either
  /// `Allocate` or `Reallocate`; otherwise the behavior is undefined.
  ///
  /// @param[in]  ptr           Pointer to previously-allocated memory.
  /// @param[in]  layout        Describes the memory to be deallocated.
  void Deallocate(void* ptr, Layout layout) {
    return DoDeallocate(ptr, layout);
  }

  /// Modifies the size of an previously-allocated block of memory without
  /// copying any data.
  ///
  /// Returns true if its size was changed without copying data to a new
  /// allocation; otherwise returns false.
  ///
  /// In particular, it always returns true if the `old_layout.size()` equals
  /// `new_size`, and always returns false if the given pointer is null, the
  /// `old_layout.size()` is 0, or the `new_size` is 0.
  ///
  /// @param[in]  ptr           Pointer to previously-allocated memory.
  /// @param[in]  old_layout    Describes the previously-allocated memory.
  /// @param[in]  new_size      Requested new size for the memory allocation.
  bool Resize(void* ptr, Layout layout, size_t new_size) {
    if (ptr == nullptr || layout.size() == 0 || new_size == 0) {
      return false;
    }
    return DoResize(ptr, layout, new_size);
  }

  /// Modifies the size of a previously-allocated block of memory.
  ///
  /// Returns pointer to the modified block of memory, or `nullptr` if the
  /// memory could not be modified.
  ///
  /// The data stored by the memory being modified must be trivially
  /// copyable. If it is not, callers should themselves attempt to `Resize`,
  /// then `Allocate`, move the data, and `Deallocate` as needed.
  ///
  /// If `nullptr` is returned, the block of memory is unchanged. In particular,
  /// if the `new_layout` has a size of 0, the given pointer will NOT be
  /// deallocated.
  ///
  /// Unlike `Resize`, providing a null pointer or a `old_layout` with a size of
  /// 0 will return a new allocation.
  ///
  /// @param[in]  ptr         Pointer to previously-allocated memory.
  /// @param[in]  layout  Describes the previously-allocated memory.
  /// @param[in]  new_size    Requested new size for the memory allocation.
  void* Reallocate(void* ptr, Layout layout, size_t new_size) {
    return DoReallocate(ptr, layout, new_size);
  }

 private:
  /// Virtual `Query` function that can be overridden by derived classes.
  ///
  /// The default implementation of this method simply returns `UNIMPLEMENTED`.
  /// Allocators which dispatch to other allocators need to override this method
  /// in order to be able to direct reallocations and deallocations to
  /// appropriate allocator.
  virtual Status DoQuery(const void*, Layout) const {
    return Status::Unimplemented();
  }

  /// Virtual `Allocate` function implemented by derived classes.
  virtual void* DoAllocate(Layout layout) = 0;

  /// Virtual `Deallocate` function implemented by derived classes.
  virtual void DoDeallocate(void* ptr, Layout layout) = 0;

  /// Virtual `Resize` function implemented by derived classes.
  ///
  /// The default implementation simply returns `false`, indicating that
  /// resizing is not supported.
  virtual bool DoResize(void* /*ptr*/, Layout /*layout*/, size_t /*new_size*/) {
    return false;
  }

  /// Virtual `Reallocate` function that can be overridden by derived classes.
  ///
  /// The default implementation will first try to `Resize` the data. If that is
  /// unsuccessful, it will allocate an entirely new block, copy existing data,
  /// and deallocate the given block.
  virtual void* DoReallocate(void* ptr, Layout layout, size_t new_size);
};

/// An RAII pointer to a value of type ``T`` stored within an ``Allocator``.
///
/// This is analogous to ``std::unique_ptr``, but includes a few differences
/// in order to support ``Allocator`` and encourage safe usage. Most notably,
/// ``UniquePtr<T>`` cannot be constructed from a ``T*``.
template <typename T>
class UniquePtr {
 public:
  /// Creates an empty (``nullptr``) instance.
  ///
  /// NOTE: Instances of this type are most commonly constructed using
  /// ``Allocator::MakeUnique``.
  constexpr UniquePtr()
      : value_(nullptr), layout_(nullptr), allocator_(nullptr) {}

  /// Creates an empty (``nullptr``) instance.
  ///
  /// NOTE: Instances of this type are most commonly constructed using
  /// ``Allocator::MakeUnique``.
  constexpr UniquePtr(std::nullptr_t)
      : value_(nullptr), layout_(nullptr), allocator_(nullptr) {}

  /// Move-constructs a ``UniquePtr<T>`` from a ``UniquePtr<U>``.
  ///
  /// This allows not only pure move construction where ``T == U``, but also
  /// converting construction where ``T`` is a base class of ``U``, like
  /// ``UniquePtr<Base> base(allocator.MakeUnique<Child>());``.
  template <typename U>
  UniquePtr(UniquePtr<U>&& other) noexcept
      : value_(other.value_),
        layout_(other.layout_),
        allocator_(other.allocator_) {
    static_assert(
        std::is_assignable_v<T*&, U*>,
        "Attempted to construct a UniquePtr<T> from a UniquePtr<U> where "
        "U* is not assignable to T*.");
    other.Release();
  }

  /// Move-assigns a ``UniquePtr<T>`` from a ``UniquePtr<U>``.
  ///
  /// This operation destructs and deallocates any value currently stored in
  /// ``this``.
  ///
  /// This allows not only pure move assignment where ``T == U``, but also
  /// converting assignment where ``T`` is a base class of ``U``, like
  /// ``UniquePtr<Base> base = allocator.MakeUnique<Child>();``.
  template <typename U>
  UniquePtr& operator=(UniquePtr<U>&& other) noexcept {
    static_assert(std::is_assignable_v<T*&, U*>,
                  "Attempted to assign a UniquePtr<U> to a UniquePtr<T> where "
                  "U* is not assignable to T*.");
    Reset();
    value_ = other.value_;
    layout_ = other.layout_;
    allocator_ = other.allocator_;
    other.Release();
  }

  /// Sets this ``UniquePtr`` to null, destructing and deallocating any
  /// currently-held value.
  ///
  /// After this function returns, this ``UniquePtr`` will be in an "empty"
  /// (``nullptr``) state until a new value is assigned.
  UniquePtr& operator=(std::nullptr_t) { Reset(); }

  /// Destructs and deallocates any currently-held value.
  ~UniquePtr() { Reset(); }

  /// Sets this ``UniquePtr`` to an "empty" (``nullptr``) value without
  /// destructing any currently-held value or deallocating any underlying
  /// memory.
  void Release() {
    value_ = nullptr;
    layout_ = nullptr;
    allocator_ = nullptr;
  }

  /// Destructs and deallocates any currently-held value.
  ///
  /// After this function returns, this ``UniquePtr`` will be in an "empty"
  /// (``nullptr``) state until a new value is assigned.
  void Reset() {
    if (value_ != nullptr) {
      value_->~T();
      allocator_->Deallocate(value_, *layout_);
      Release();
    }
  }

  /// ``operator bool`` is not provided in order to ensure that there is no
  /// confusion surrounding ``if (foo)`` vs. ``if (*foo)``.
  ///
  /// ``nullptr`` checking should instead use ``if (foo == nullptr)``.
  explicit operator bool() const = delete;

  /// Returns whether this ``UniquePtr`` is in an "empty" (``nullptr``) state.
  bool operator==(std::nullptr_t) const { return value_ == nullptr; }

  /// Returns whether this ``UniquePtr`` is not in an "empty" (``nullptr``)
  /// state.
  bool operator!=(std::nullptr_t) const { return value_ != nullptr; }

  /// Returns the underlying (possibly null) pointer.
  T* get() { return value_; }
  /// Returns the underlying (possibly null) pointer.
  const T* get() const { return value_; }

  /// Permits accesses to members of ``T`` via ``my_unique_ptr->Member``.
  ///
  /// The behavior of this operation is undefined if this ``UniquePtr`` is in an
  /// "empty" (``nullptr``) state.
  T* operator->() { return value_; }
  const T* operator->() const { return value_; }

  /// Returns a reference to any underlying value.
  ///
  /// The behavior of this operation is undefined if this ``UniquePtr`` is in an
  /// "empty" (``nullptr``) state.
  T& operator*() { return *value_; }
  const T& operator*() const { return *value_; }

 private:
  /// A pointer to the contained value.
  T* value_;

  /// The ``layout_` with which ``value_``'s allocation was initially created.
  ///
  /// Unfortunately this is not simply ``Layout::Of<T>()`` since ``T`` may be
  /// a base class of the original allocated type.
  const Layout* layout_;

  /// The ``allocator_`` in which ``value_`` is stored.
  /// This must be tracked in order to deallocate the memory upon destruction.
  Allocator* allocator_;

  /// Allow converting move constructor and assignment to access fields of
  /// this class.
  ///
  /// Without this, ``UniquePtr<U>`` would not be able to access fields of
  /// ``UniquePtr<T>``.
  template <typename U>
  friend class UniquePtr;

  class PrivateConstructorType {};
  static constexpr PrivateConstructorType kPrivateConstructor{};

 public:
  /// Private constructor that is public only for use with `emplace` and
  /// other in-place construction functions.
  ///
  /// Constructs a ``UniquePtr`` from an already-allocated value.
  ///
  /// NOTE: Instances of this type are most commonly constructed using
  /// ``Allocator::MakeUnique``.
  UniquePtr(PrivateConstructorType,
            T* value,
            const Layout* layout,
            Allocator* allocator)
      : value_(value), layout_(layout), allocator_(allocator) {}

  // Allow construction with ``kPrivateConstructor`` to the implementation
  // of ``MakeUnique``.
  friend class Allocator;
};

}  // namespace pw::allocator
