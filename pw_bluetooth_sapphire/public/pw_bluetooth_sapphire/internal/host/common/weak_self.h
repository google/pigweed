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
#include <pw_intrusive_ptr/intrusive_ptr.h>

#include <optional>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"

template <typename T>
class DynamicWeakManager;

// WeakRef is an intrusively-counted reference to an object that may or may not
// still exist. Check is_alive() before using the get() function to get a
// reference to the object.
//
// This is not thread-safe: get() must be used on the thread the WeakPtr was
// created on (but can be passed through other threads while not being used)
class WeakRef : public pw::RefCounted<WeakRef> {
 public:
  ~WeakRef() = default;

  // Returns true if the object referred is alive.
  // If this returns true, WeakRef<T>::Get() will work.
  bool is_alive() { return !!ptr_; }

  // Get a reference to the alive object.
  void* get() {
    BT_ASSERT_MSG(ptr_, "attempted to get a destroyed ptr");
    return ptr_;
  }

  void set(void* p) { ptr_ = p; }

  void maybe_unset(const void* doomed) {
    if (ptr_ == doomed) {
      ptr_ = nullptr;
    }
  }

 private:
  template <class T>
  friend class DynamicWeakManager;

  explicit WeakRef(void* ptr) : ptr_(ptr) {}

  // Pointer to the existent object if it is alive, otherwise a nullptr. We use
  // a void* to avoid templating and to avoid having separate variables for the
  // flag and the pointer. Avoiding templating enables us to support upcasting,
  // as WeakRef type remains the same for the upcasted WeakPtr.
  void* ptr_;
};

// RecyclingWeakRef is a version of WeakRef which avoids deletion after the last
// count is destructed, instead marking itself as not in use, for reuse by a
// WeakManager that maintains a pool of RecyclingWeakRefs for static memory
// usage.
//
// For an example, see OnlyTwoStaticManager in the unit tests for WeakSelf.
class RecyclingWeakRef : public pw::Recyclable<RecyclingWeakRef>,
                         public pw::RefCounted<RecyclingWeakRef> {
 public:
  RecyclingWeakRef() : ptr_(nullptr) {}
  ~RecyclingWeakRef() = default;

  // Returns true if the object referred is alive.
  // If this returns true, Get() will work.
  // If this returns true, in_use will also return true.
  bool is_alive() { return !!ptr_; }

  // Returns true if this ref is in use.
  // This can return true while is_alive returns false.
  bool is_in_use() { return in_use_; }

  void* get() {
    BT_ASSERT_MSG(in_use_, "shouldn't get an unallocated ptr");
    BT_ASSERT_MSG(ptr_, "attempted to get a destroyed ptr");
    return ptr_;
  }

  pw::IntrusivePtr<RecyclingWeakRef> alloc(void* p) {
    BT_ASSERT(!in_use_);
    in_use_ = true;
    ptr_ = p;
    return pw::IntrusivePtr<RecyclingWeakRef>(this);
  }

  void maybe_unset(const void* doomed) {
    if (in_use_ && ptr_ == doomed) {
      ptr_ = nullptr;
    }
  }

 private:
  friend class pw::Recyclable<RecyclingWeakRef>;
  // This method is called by Recyclable on the last reference drop.
  void pw_recycle() {
    ptr_ = nullptr;
    in_use_ = false;
  }

  // True if this pointer is in use.
  bool in_use_ = false;

  // Pointer to the existent object if it is alive, otherwise a nullptr.
  void* ptr_;
};

// Default Manager for Weak Pointers. Each object that derives from WeakSelf
// holds one manager object. This indirection is used to enable shared static
// memory weak pointers across multiple copies of the same class of objects.
//
// The default manager allocates a single weak pointer for each object that
// acquires at least one weak reference, and holds the weak reference alive
// until the object referenced is destroyed.
template <typename T>
class DynamicWeakManager {
 public:
  explicit DynamicWeakManager(T* self_ptr)
      : self_ptr_(self_ptr), weak_ptr_ref_(nullptr) {}

  using RefType = WeakRef;

  ~DynamicWeakManager() { InvalidateAll(); }

  std::optional<pw::IntrusivePtr<RefType>> GetWeakRef() {
    if (weak_ptr_ref_ == nullptr) {
      weak_ptr_ref_ = pw::IntrusivePtr(new RefType(self_ptr_));
    }
    return weak_ptr_ref_;
  }

  void InvalidateAll() {
    if (weak_ptr_ref_) {
      weak_ptr_ref_->maybe_unset(self_ptr_);
    }
  }

 private:
  T* self_ptr_;
  pw::IntrusivePtr<WeakRef> weak_ptr_ref_;
};

template <typename T, typename WeakPtrManager>
class WeakSelf;

template <typename T, typename WeakPtrManager = DynamicWeakManager<T>>
class WeakPtr {
 public:
  // Default-constructed WeakPtrs point nowhere and aren't alive.
  WeakPtr() : ptr_(nullptr) {}
  explicit WeakPtr(std::nullptr_t) : WeakPtr() {}

  // Implicit upcast via copy construction.
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  WeakPtr(const WeakPtr<U>& r) : WeakPtr(r.ptr_) {}

  // Implicit upcast via move construction.
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  WeakPtr(WeakPtr<U>&& r) : WeakPtr(std::move(r.ptr_)) {}

  bool is_alive() const { return ptr_ && ptr_->is_alive(); }
  T& get() const {
    BT_ASSERT_MSG(ptr_, "tried to get never-assigned weak pointer");
    return *static_cast<T*>(ptr_->get());
  }

  T* operator->() const { return &get(); }

  void reset() { ptr_ = nullptr; }

 private:
  // Allow accessing WeakPtr<U>'s member variables when upcasting.
  template <typename U, typename RefType>
  friend class WeakPtr;

  // Only WeakSelf<T> should have access to the constructor.
  friend class WeakSelf<T, WeakPtrManager>;

  explicit WeakPtr(pw::IntrusivePtr<typename WeakPtrManager::RefType> ptr)
      : ptr_(std::move(ptr)) {}

  pw::IntrusivePtr<typename WeakPtrManager::RefType> ptr_;
};

// WeakSelf is a class used to create pointers to an object that must be checked
// before using - because their target may have been destroyed.  These are
// termed "weak pointers" and can be vended in one of two ways: (1) inheriting
// from the WeakSelf class and initializing it on construction,
//
// class A: WeakSelf<A> {
//    A() : WeakSelf(this) {}
//    auto MakeSelfReferentialCallback() {
//       auto cb = [weak = GetWeakPtr()] {
//          if (weak.is_alive()) {
//             weak->AnotherFunction();
//          }
//       }
//       return std::move(cb);
//     }
//  };
//
// (2) making a WeakSelf<T> a member of your class and using it to vend
// pointers.
//
// class A {
//  public:
//    A() : weak_factory_(this) {}
//    auto MakeSelfReferentialCallback() {
//       auto cb = [weak = weak_factory_.GetWeakPtr()] {
//          if (weak.is_alive()) {
//             weak->AnotherFunction();
//          }
//       }
//       return std::move(cb);
//     }
//   private:
//    // Other members should be defined before weak_factory_ so it is destroyed
//    first. WeakSelf<A> weak_factory_;
//  };
//
// The first method is preferable if you expect to vend weak pointers outside
// the class, as the WeakSelf::GetWeakPtr function is public. However, note that
// with the first method, members of the class will be destroyed before the
// class is destroyed - it may be undesirable if during destruction, the weak
// pointer should be considered dead.  This can be mitigated by using
// InvalidatePtrs() to invalidate the weak pointers in the destructor.
template <typename T, typename WeakPtrManager = DynamicWeakManager<T>>
class WeakSelf {
 public:
  explicit WeakSelf(T* self_ptr) : manager_(self_ptr) {}
  ~WeakSelf() = default;

  using WeakPtr = WeakPtr<T, WeakPtrManager>;

  // Invalidates all the WeakPtrs that have been vended before now (they will
  // return false for is_alive) and prevents any new pointers from being vended.
  // This is effectively the same as calling the destructor, but can be done
  // early.
  void InvalidatePtrs() { manager_.InvalidateAll(); }

  WeakPtr GetWeakPtr() {
    auto weak_ref = manager_.GetWeakRef();
    BT_ASSERT_MSG(weak_ref.has_value(), "weak pointer not available");
    return WeakPtr(*weak_ref);
  }

 private:
  WeakPtrManager manager_;
};
