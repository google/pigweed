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
#include <utility>

#include "pw_allocator/capability.h"
#include "pw_assert/assert.h"
#include "pw_preprocessor/compiler.h"
#include "pw_result/result.h"
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
    PW_ASSERT(!PW_ADD_OVERFLOW(size, size_, &size));
    return Layout(size, alignment_);
  }

  size_t size() const { return size_; }
  size_t alignment() const { return alignment_; }

 private:
  size_t size_ = 0;
  size_t alignment_ = 1;
};

inline bool operator==(const Layout& lhs, const Layout& rhs) {
  return lhs.size() == rhs.size() && lhs.alignment() == rhs.alignment();
}

inline bool operator!=(const Layout& lhs, const Layout& rhs) {
  return !(lhs == rhs);
}

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
  virtual ~Allocator() = default;

  const Capabilities& capabilities() const { return capabilities_; }

  /// Returns whether a given capabilityis enabled for this allocator.
  bool HasCapability(Capability capability) const {
    return capabilities_.has(capability);
  }

  /// Allocates a block of memory with the specified size and alignment.
  ///
  /// Returns `nullptr` if the allocation cannot be made, or the `layout` has a
  /// size of 0.
  ///
  /// @param[in]  layout      Describes the memory to be allocated.
  void* Allocate(Layout layout) {
    return layout.size() != 0 ? DoAllocate(layout) : nullptr;
  }

  /// Constructs and object of type `T` from the given `args`
  ///
  /// The return value is nullable, as allocating memory for the object may
  /// fail. Callers must check for this error before using the resulting
  /// pointer.
  ///
  /// @param[in]  args...     Arguments passed to the object constructor.
  template <typename T, int&... ExplicitGuard, typename... Args>
  T* New(Args&&... args) {
    void* ptr = Allocate(Layout::Of<T>());
    return ptr != nullptr ? new (ptr) T(std::forward<Args>(args)...) : nullptr;
  }

  /// Constructs and object of type `T` from the given `args`, and wraps it in a
  /// `UniquePtr`
  ///
  /// The returned value may contain null if allocating memory for the object
  /// fails. Callers must check for null before using the `UniquePtr`.
  ///
  /// @param[in]  args...     Arguments passed to the object constructor.
  template <typename T, int&... ExplicitGuard, typename... Args>
  [[nodiscard]] UniquePtr<T> MakeUnique(Args&&... args) {
    static constexpr Layout kStaticLayout = Layout::Of<T>();
    return UniquePtr<T>(UniquePtr<T>::kPrivateConstructor,
                        New<T>(std::forward<Args>(args)...),
                        &kStaticLayout,
                        this);
  }

  /// Releases a previously-allocated block of memory.
  ///
  /// The given pointer must have been previously obtained from a call to either
  /// `Allocate` or `Reallocate`; otherwise the behavior is undefined.
  ///
  /// @param[in]  ptr           Pointer to previously-allocated memory.
  /// @param[in]  layout        Describes the memory to be deallocated.
  void Deallocate(void* ptr, Layout layout) {
    if (ptr != nullptr) {
      DoDeallocate(ptr, layout);
    }
  }

  /// Destroys the object at ``ptr`` and deallocates the associated memory.
  ///
  /// The given pointer must have been previously obtained from a call to
  /// ``New`` using the same allocator; otherwise the behavior is undefined.
  ///
  /// This functions is only callable with objects whose type is ``final``.
  /// This limitation is unfortunately required due to the fact that it is not
  /// possible to inspect the size of the underlying memory allocation for
  /// virtual objects.
  ///
  /// @param[in] ptr      Pointer to previously-allocated object.
  template <typename T>
  void Delete(T* ptr) {
    static_assert(
        std::is_final_v<T>,
        "``pw::allocator::Allocator::Delete`` can only be used with ``final`` "
        "types, as the allocated size of virtual objects is unknowable.");
    std::destroy_at(ptr);
    Deallocate(ptr, Layout::Of<T>());
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
    return ptr != nullptr && new_size != 0 &&
           (layout.size() == new_size || DoResize(ptr, layout, new_size));
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
  /// @param[in]  layout      Describes the previously-allocated memory.
  /// @param[in]  new_size    Requested new size for the memory allocation.
  void* Reallocate(void* ptr, Layout layout, size_t new_size) {
    return new_size != 0 ? DoReallocate(ptr, layout, new_size) : nullptr;
  }

  /// Returns the layout used to allocate a given pointer.
  ///
  /// Some allocators may be designed to integrate with a `malloc`-style
  /// interface, wherein `Layout` details can be recovered from an allocated
  /// pointer. This can facilitate calling `Deallocate`, `Resize`, and
  /// `Reallocate` insituations where only an allocated `void*` is available.
  ///
  /// The returned layout for a given allocated pointer is not required to
  /// be identical to the one provided to the call to `Allocate` that produced
  /// that pointer. Instead, the layout is merely guaranteed to be valid for
  /// passing to `Deallocate`, `Resize`, and `Reallocate` with the same pointer.
  ///
  /// For example, an allocator that uses `Block`s to track allocations may
  /// return a layout describing the correct alignment, but a larger size
  /// corresponding to the block used.
  ///
  /// @retval UNIMPLEMENTED   This allocator cannot recover layouts.
  /// @retval NOT_FOUND       The allocator does not recognize the pointer
  ///                         as one of its allocations.
  /// @retval OK              This result contains the requested layout.
  Result<Layout> GetLayout(const void* ptr) const { return DoGetLayout(ptr); }

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
  /// @retval UNIMPLEMENTED   This object cannot recognize allocated pointers.
  /// @retval OUT_OF_RANGE    Pointer cannot be re/deallocated by this object.
  /// @retval OK              This object can re/deallocate the pointer.
  Status Query(const void* ptr, Layout layout) const {
    return DoQuery(ptr, layout);
  }

  /// Returns whether the given allocator is the same as this one.
  ///
  /// This method is used instead of ``operator==`` in keeping with
  /// ``std::pmr::memory_resource::is_equal``. There currently is no
  /// corresponding virtual ``DoIsEqual``, as allocators that would require
  /// ``dynamic_cast`` to properly determine equality are not supported.
  /// This method will allow the interface to remain unchanged should a future
  /// need for such allocators arise.
  ///
  /// @param[in]  other       Allocator to compare with this object.
  bool IsEqual(const Allocator& other) const { return this == &other; }

 protected:
  /// TODO(b/326509341): Remove when downstream consumer migrated.
  constexpr Allocator() = default;

  explicit constexpr Allocator(const Capabilities& capabilities)
      : capabilities_(capabilities) {}

 private:
  /// Virtual `Allocate` function implemented by derived classes.
  ///
  /// @param[in]  layout        Describes the memory to be allocated. Guaranteed
  ///                           to have a non-zero size.
  virtual void* DoAllocate(Layout layout) = 0;

  /// Virtual `Deallocate` function implemented by derived classes.
  ///
  /// @param[in]  ptr           Pointer to memory, guaranteed to not be null.
  /// @param[in]  layout        Describes the memory to be deallocated.
  virtual void DoDeallocate(void* ptr, Layout layout) = 0;

  /// Virtual `Resize` function implemented by derived classes.
  ///
  /// The default implementation simply returns `false`, indicating that
  /// resizing is not supported.
  ///
  /// @param[in]  ptr           Pointer to memory, guaranteed to not be null.
  /// @param[in]  old_layout    Describes the previously-allocated memory.
  /// @param[in]  new_size      Requested size, guaranteed to be non-zero and
  ///                           differ from ``old_layout.size()``.
  virtual bool DoResize(void* /*ptr*/, Layout /*layout*/, size_t /*new_size*/) {
    return false;
  }

  /// Virtual `Reallocate` function that can be overridden by derived classes.
  ///
  /// The default implementation will first try to `Resize` the data. If that is
  /// unsuccessful, it will allocate an entirely new block, copy existing data,
  /// and deallocate the given block.
  ///
  /// @param[in]  ptr           Pointer to memory..
  /// @param[in]  old_layout    Describes the previously-allocated memory.
  /// @param[in]  new_size      Requested size, guaranteed to be non-zero.
  virtual void* DoReallocate(void* ptr, Layout layout, size_t new_size);

  /// Virtual `Query` function that can be overridden by derived classes.
  ///
  /// The default implementation of this method simply returns `UNIMPLEMENTED`,
  /// indicating the allocator cannot recover layouts from allocated pointers.
  virtual Result<Layout> DoGetLayout(const void*) const {
    return Status::Unimplemented();
  }

  /// Virtual `Query` function that can be overridden by derived classes.
  ///
  /// The default implementation of this method simply returns `UNIMPLEMENTED`.
  /// Allocators which dispatch to other allocators need to override this method
  /// in order to be able to direct reallocations and deallocations to
  /// appropriate allocator.
  virtual Status DoQuery(const void*, Layout) const {
    return Status::Unimplemented();
  }

  /// Hints about optional methods implemented or optional behaviors requested
  /// by an allocator of a derived type.
  Capabilities capabilities_;
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
  constexpr UniquePtr(std::nullptr_t) : UniquePtr() {}

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

  // Move-only. These are needed since the templated move-contructor and
  // move-assignment operator do not exactly match the signature of the default
  // move-contructor and move-assignment operator, and thus do not implicitly
  // delete the copy-contructor and copy-assignment operator.
  UniquePtr(const UniquePtr&) = delete;
  UniquePtr& operator=(const UniquePtr&) = delete;

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
    return *this;
  }

  /// Sets this ``UniquePtr`` to null, destructing and deallocating any
  /// currently-held value.
  ///
  /// After this function returns, this ``UniquePtr`` will be in an "empty"
  /// (``nullptr``) state until a new value is assigned.
  UniquePtr& operator=(std::nullptr_t) { Reset(); }

  /// Destructs and deallocates any currently-held value.
  ~UniquePtr() { Reset(); }

  const Layout* layout() const { return layout_; }
  Allocator* allocator() const { return allocator_; }

  /// Releases a value from the ``UniquePtr`` without destructing or
  /// deallocating it.
  ///
  /// After this call, the object will have an "empty" (``nullptr``) value.
  T* Release() {
    T* value = value_;
    value_ = nullptr;
    layout_ = nullptr;
    allocator_ = nullptr;
    return value;
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
