// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_WEAK_SELF_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_WEAK_SELF_H_

#include <optional>

#include <fbl/recycler.h>
#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"

template <typename T>
class DynamicWeakManager;

// WeakRef is an intrusively-counted reference to an object that may or may not still exist.
// Check is_alive() before using the get() function to get a reference to
// the object.
//
// This is not thread-safe: get() must be used on the thread the WeakPtr was created on
// (but can be passed through other threads while not being used)
template <typename T>
class WeakRef : public fbl::RefCounted<WeakRef<T>> {
 public:
  ~WeakRef() = default;

  // Returns true if the object referred is alive.
  // If this returns true, WeakRef<T>::Get() will work.
  bool is_alive() { return !!ptr_; }

  // Get a reference to the alive object.
  T& get() {
    BT_ASSERT_MSG(ptr_, "attempted to get a destroyed ptr");
    return *ptr_;
  }

  void set(T* p) { ptr_ = p; }

 private:
  explicit WeakRef(T* ptr) : ptr_(ptr) {}

  void maybe_unset(const T* doomed) {
    if (ptr_ == doomed) {
      ptr_ = nullptr;
    }
  }
  // Pointer to the existent object if it is alive, otherwise a nullptr.
  T* ptr_;
  friend class DynamicWeakManager<T>;
};

// RecyclingWeakRef is a version of WeakRef which avoids deletion after the last count is
// destructed, instead marking itself as not in use, for reuse by a WeakManager that
// maintains a pool of RecyclingWeakRefs for static memory usage.
//
// For an example, see OnlyTwoStaticManager in the unit tests for WeakSelf.
//
// We disable the Adoption Validator of RefCounted so that this pointer can be adopted multiple
// times. Otherwise RefCountedBase gets mad as us for not resetting the count to the sentinel (which
// is not possible).
template <typename T>
class RecyclingWeakRef
    : public fbl::Recyclable<RecyclingWeakRef<T>>,
      public fbl::RefCounted<RecyclingWeakRef<T>, /*EnableAdoptionValidator=*/false> {
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

  T& get() {
    BT_ASSERT_MSG(in_use_, "shouldn't get an unallocated ptr");
    BT_ASSERT_MSG(ptr_, "attempted to get a destroyed ptr");
    return *ptr_;
  }

  fbl::RefPtr<RecyclingWeakRef<T>> alloc(T* p) {
    BT_ASSERT(!in_use_);
    in_use_ = true;
    ptr_ = p;
    return fbl::AdoptRef(this);
  }

  void maybe_unset(const T* doomed) {
    if (in_use_ && ptr_ == doomed) {
      ptr_ = nullptr;
    }
  }

 private:
  friend class fbl::Recyclable<RecyclingWeakRef<T>>;
  // This method is called by Recyclable on the last reference drop.
  void fbl_recycle() {
    ptr_ = nullptr;
    in_use_ = false;
  }

  // True if this pointer is in use.
  bool in_use_ = false;

  // Pointer to the existent object if it is alive, otherwise a nullptr.
  T* ptr_;
};

// Default Manager for Weak Pointers. Each object that derives from WeakSelf holds one manager
// object. This indirection is used to enable shared static memory weak pointers across multiple
// copies of the same class of objects.
//
// The default manager allocates a single weak pointer for each object that acquires at least one
// weak reference, and holds the weak reference alive until the object referenced is destroyed.
template <typename T>
class DynamicWeakManager {
 public:
  explicit DynamicWeakManager(T* self_ptr) : self_ptr_(self_ptr), weak_ptr_ref_(nullptr) {}

  using RefType = WeakRef<T>;

  ~DynamicWeakManager() { InvalidateAll(); }

  std::optional<fbl::RefPtr<RefType>> GetWeakRef() {
    if (weak_ptr_ref_ == nullptr) {
      weak_ptr_ref_ = fbl::AdoptRef(new RefType(self_ptr_));
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
  fbl::RefPtr<WeakRef<T>> weak_ptr_ref_;
};

// WeakSelf is a class used to create pointers to an object that must be checked before
// using - because their target may have been destroyed.  These are termed "weak pointers"
// and can be vended in one of two ways:
// (1) inheriting from the WeakSelf class and initializing it on construction,
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
// (2) making a WeakSelf<T> a member of your class and using it to vend pointers.
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
//    // Other members should be defined before weak_factory_ so it is destroyed first.
//    WeakSelf<A> weak_factory_;
//  };
//
// The first method is preferable if you expect to vend weak pointers outside the class, as the
// WeakSelf::GetWeakPtr function is public. However, note that with the first method, members of the
// class will be destroyed before the class is destroyed - it may be undesirable if during
// destruction, the weak pointer should be considered dead.
template <typename T, typename WeakPtrManager = DynamicWeakManager<T>>
class WeakSelf {
 public:
  explicit WeakSelf(T* self_ptr) : manager_(self_ptr) {}
  ~WeakSelf() = default;

  class WeakPtr {
   public:
    // Default-constructed WeakPtrs point nowhere and aren't alive.
    WeakPtr() : ptr_(nullptr) {}
    explicit WeakPtr(std::nullptr_t) : WeakPtr() {}

    bool is_alive() const { return ptr_ && ptr_->is_alive(); }
    T& get() const {
      BT_ASSERT_MSG(ptr_, "tried to get never-assigned weak pointer");
      return ptr_->get();
    }
    T* operator->() const { return &get(); }

    void reset() { ptr_ = nullptr; }

   private:
    explicit WeakPtr(fbl::RefPtr<typename WeakPtrManager::RefType> ptr) : ptr_(ptr) {}

    fbl::RefPtr<typename WeakPtrManager::RefType> ptr_;

    friend WeakSelf<T, WeakPtrManager>;
  };

  // Invalidates all the WeakPtrs that have been vended before now (they will return false for
  // is_alive) and prevents any new pointers from being vended.  This is effectively the same as
  // calling the destructor, but can be done early.
  void InvalidatePtrs() { manager_.InvalidateAll(); }

  WeakPtr GetWeakPtr() {
    auto weak_ref = manager_.GetWeakRef();
    BT_ASSERT_MSG(weak_ref.has_value(), "weak pointer not available");
    return WeakPtr(*weak_ref);
  }

 private:
  WeakPtrManager manager_;
};

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_WEAK_SELF_H_
