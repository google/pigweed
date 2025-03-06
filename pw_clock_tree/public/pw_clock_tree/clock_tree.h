// Copyright 2024 The Pigweed Authors
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
#include <memory>
#include <mutex>

#include "pw_assert/assert.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/mutex.h"

namespace pw::clock_tree {

/// Abstract base class for a clock tree element of a clock tree.
///
/// Class implementations of `Element` must implement `Acquire` and `Release`
/// functions. For clock tree elements that only get enabled / configured,
/// it is sufficient to only override the `DoEnable` function, otherwise it
/// is required to override the `DoDisable` function to disable the respective
/// clock tree element.
///
/// Note: Clock tree element classes shouldn't be directly derived from the
/// `Element` class, but from the `ElementBlocking`,
/// `ElementNonBlockingCannotFail` or `ElementNonBlockingMightFail` class.
class Element {
 public:
  constexpr Element(bool may_block = false) : may_block_(may_block) {}
  virtual ~Element() = default;

  /// Get reference count for this clock tree element.
  uint32_t ref_count() const { return ref_count_; }

  /// Check whether acquiring or releasing the element may block.
  bool may_block() const { return may_block_; }

  // Not copyable or movable
  Element(const Element&) = delete;
  Element(const Element&&) = delete;
  Element& operator=(const Element&) = delete;
  Element& operator=(const Element&&) = delete;

 protected:
  /// Acquire a reference to the clock tree element.
  ///
  /// Acquiring a reference to a clock tree element ensures that the
  /// clock tree element is configured and enabled.
  ///
  /// If the clock tree element depends on another clock tree element,
  /// a reference to the dependent clock tree element will get
  /// acquired when the first reference to this clock tree element
  /// gets acquired. This ensures that all dependent clock tree
  /// elements have been enabled before this clock tree element gets
  /// configured and enabled.
  virtual Status Acquire() = 0;

  /// Release a reference to the clock tree element.
  ///
  /// Releasing the last reference to the clock tree element will disable
  /// the clock tree element.
  ///
  /// When the last reference to the clock tree element gets released,
  /// the clock tree element gets disabled if the `DoDisable` function
  /// is overridden.
  ///
  /// If the clock tree element depends on another clock tree element,
  /// a reference to the dependent clock tree element will get
  /// released once the last reference to this clock tree element has
  /// been released and the clock tree element has been disabled.
  /// This ensures that the clock tree element gets disabled before
  /// all dependent clock tree elements have been disabled.
  virtual Status Release() = 0;

  /// Increment reference count and return incremented value.
  uint32_t IncRef() { return ++ref_count_; }

  /// Decrement reference count and return decremented value.
  uint32_t DecRef() { return --ref_count_; }

  /// Function called when the clock tree element needs to get enabled.
  virtual Status DoEnable() = 0;

  /// Function called when the clock tree element can get disabled.
  ///
  /// Can be overridden by child class in case the clock tree element
  /// can be disabled to save power.
  virtual Status DoDisable() { return OkStatus(); }

 private:
  /// Reference count for this tree element.
  uint32_t ref_count_ = 0;

  /// Whether acquiring or releasing the element may block.
  const bool may_block_;

  friend class ClockTree;
  template <typename ElementType>
  friend class DependentElement;
  friend class ClockDivider;
};

/// Abstract class of a clock tree element that might need to block to perform
/// element updates.
class ElementBlocking : public Element {
 public:
  constexpr ElementBlocking() : Element(/*may_block=*/true) {}
};

/// Abstract class of a clock tree element that will not block to perform
/// element updates and will not fail when performing clock updates.
class ElementNonBlockingCannotFail : public Element {};

/// Abstract class of a clock tree element that will not block to perform
/// element updates and might fail when performing clock updates.
class ElementNonBlockingMightFail : public Element {};

/// Abstract class template of a clock tree element that provides a clock
/// source.
///
/// A `ClockSource` clock tree element does not depend on any other
/// clock tree element, but provides a clock to the system that is not
/// derived from another clock.
///
/// Class implementations of `ClockSource` must implement
/// `Acquire` and `Release` functions. For clock sources that only get
/// enabled / configured, it is sufficient to only override the `DoEnable`
/// function, otherwise it is required to override the `DoDisable` function to
/// disable the clock source.
///
/// Template argument `ElementType` can be of class `ElementBlocking`,
/// `ElementNonBlockingCannotFail` or
/// `ElementNonBlockingMightFail.`
template <typename ElementType>
class ClockSource : public ElementType {
 private:
  /// Acquire a reference to the clock source.
  ///
  /// When the first reference gets acquired, the clock source gets enabled.
  Status Acquire() final {
    if (this->IncRef() > 1) {
      // This clock tree element is already enabled.
      return OkStatus();
    }

    // Enable clock source.
    Status status = this->DoEnable();
    if (!status.ok()) {
      this->DecRef();
    }
    return status;
  }

  /// Release a reference to the clock source.
  ///
  /// When the last reference gets released, the clock source gets disabled.
  Status Release() final {
    if (this->DecRef() > 0) {
      // The clock tree element remains enabled.
      return OkStatus();
    }

    // Disable the clock source.
    Status status = this->DoDisable();
    if (!status.ok()) {
      this->IncRef();
    }
    return status;
  }
};

/// Class that represents a no-op clock source clock tree element that can
/// be used to satisfy the dependent source clock tree element dependency
/// for clock source classes that expect a source clock tree element.
class ClockSourceNoOp : public ClockSource<ElementNonBlockingCannotFail> {
 private:
  pw::Status DoEnable() final { return pw::OkStatus(); }

  pw::Status DoDisable() final { return pw::OkStatus(); }
};

/// Abstract class template of a clock tree element that depends on another
/// clock tree element.
///
/// A `DependentElement` clock tree element depends on another
/// clock tree element.
///
/// Class implementations of `DependentElement` must override the
/// `DoEnable` function, the `DoDisable` function can be overridden to disable
/// the dependent clock tree element to save power.
///
/// Template argument `ElementType` can be of class `ElementBlocking`,
/// `ElementNonBlockingCannotFail` or
/// `ElementNonBlockingMightFail.`
template <typename ElementType>
class DependentElement : public ElementType {
 public:
  /// Create a dependent clock tree element that depends on `source`.
  constexpr DependentElement(ElementType& source) : source_(&source) {}

 protected:
  /// Update source dependency.
  ///
  /// It is the responsibility of the derived class to ensure that the
  /// source dependency can only be changed when permitted, i.e. only
  /// if reference count is zero.
  /// If the update is permitted while the reference count is greater than
  /// zero, the caller of this function must make sure that the `DoEnable`
  /// method has access to the updated configuration matching the new `source`
  /// dependency. Only if the `UpdateSource` call succeeds, the new source has
  /// been configured as the `source_` element for this element, otherwise the
  /// old source element is still configured as `source_` element for this
  /// element. If the `DoEnable` call of the new source fails, the current
  /// element will be disabled, since the previous source got already released,
  /// and the old source remains configured as the dependent element.
  Status UpdateSource(ElementType& new_source, bool permit_change_if_in_use) {
    // If the element is not enabled, we can update the `source_` directly.
    if (this->ref_count() == 0) {
      source_ = &new_source;
      return OkStatus();
    }

    // The element is active, check whether we are allowed to change the source.
    if (!permit_change_if_in_use) {
      return Status::FailedPrecondition();
    }

    ElementType* old_source = source_;

    // Acquire the dependent sources for the new_source element.
    PW_TRY(new_source.Acquire());

    // Disable this current element configuration.
    if (Status status = this->DoDisable(); !status.ok()) {
      new_source.Release().IgnoreError();
      return status;
    }

    // Enable the new source element configuration.
    Status status = this->DoEnable();

    // Release the reference to the old dependent source regardless whether
    // we have enabled the new source, since we have successfully disabled it.
    old_source->Release().IgnoreError();

    // Check whether the `DoEnable` method succeeded for the new source.
    if (!status.ok()) {
      new_source.Release().IgnoreError();
      this->DecRef();
      return status;
    }

    // Everything has succeeded, change the source_ element.
    source_ = &new_source;
    return OkStatus();
  }

 private:
  /// Acquire a reference to the dependent clock tree element.
  ///
  /// When the first reference gets acquired, a reference to the source
  /// element gets acquired, before the dependent clock tree element gets
  /// enabled.
  Status Acquire() final {
    if (this->IncRef() > 1) {
      // This clock tree element is already enabled.
      return OkStatus();
    }

    // Acquire a reference to the dependent clock tree element before
    // enabling this clock tree element.
    if (Status status = source_->Acquire(); !status.ok()) {
      this->DecRef();
      return status;
    }

    Status status = this->DoEnable();
    if (!status.ok()) {
      source_->Release().IgnoreError();
      this->DecRef();
    }
    return status;
  }

  /// Release a reference to the dependent clock tree element.
  ///
  /// When the last reference gets released, the dependent clock tree
  /// element gets disabled (if implemented), before the reference to the
  /// `source` element gets released.
  Status Release() final {
    if (this->DecRef() > 0) {
      // The clock tree element remains enabled.
      return OkStatus();
    }

    // Disable the clock tree element.
    if (Status status = this->DoDisable(); !status.ok()) {
      this->IncRef();
      return status;
    }
    // Even if releasing the dependent source references fails,
    // we won't re-enable the clock source, and instead just return
    // the error code to the caller.
    return source_->Release();
  }

  /// Pointer to the source clock tree element this clock tree element depends
  /// on.
  ElementType* source_;
};

/// Abstract class of the clock divider specific interface.
///
/// The clock divider interface allows APIs to accept a `ClockDivider`
/// element, if they want to use the `ClockTree`'s `SetDividerValue` method.
/// They can use the `element` method to call the `ClockTree`'s `Acquire` and
/// `Release` methods.
class ClockDivider {
 public:
  constexpr ClockDivider(Element& element) : element_(element) {}

  virtual ~ClockDivider() = default;

  /// Set `divider` value.
  ///
  /// The `divider` value will get updated as part of this method if the clock
  /// divider is currently active, otherwise the new divider value will be
  /// configured when the clock divider gets enabled next.
  virtual Status Set(uint32_t divider) = 0;

  /// Return the element implementing this interface.
  Element& element() const { return element_; }

 private:
  /// Reference to element implementing this interface.
  Element& element_;
};

/// Abstract class template of a clock divider element.
///
/// A `ClockDivider` clock tree element depends on another clock tree element
/// and has a divider value that gets configured when the clock divider gets
/// enabled.
///
/// Class implementations of `ClockDivider` must override the `DoEnable`
/// function.
///
/// Template argument `ElementType` can be of class `ElementBlocking`,
/// `ElementNonBlockingCannotFail` or
/// `ElementNonBlockingMightFail.`
template <typename ElementType>
class ClockDividerElement : public DependentElement<ElementType>,
                            public ClockDivider {
 public:
  /// Create a clock divider element that depends on `source` and gets
  /// configured with `divider` value when enabled.
  constexpr ClockDividerElement(ElementType& source, uint32_t divider)
      : DependentElement<ElementType>(source),
        ClockDivider(static_cast<Element&>(*this)),
        divider_(divider) {}

  /// Set `divider` value.
  ///
  /// The `divider` value will get updated as part of this method if the clock
  /// divider is currently active, otherwise the new divider value will be
  /// configured when the clock divider gets enabled next.
  Status Set(uint32_t divider) override {
    uint32_t old_divider = divider_;
    divider_ = divider;
    if (this->ref_count() == 0) {
      return OkStatus();
    }

    Status status = this->DoEnable();
    if (!status.ok()) {
      // Restore old divider value.
      divider_ = old_divider;
    }
    return status;
  }

 protected:
  /// Get current divider value.
  uint32_t divider() const { return divider_; }

 private:
  /// Configured divider value.
  uint32_t divider_;
};

/// Alias for a blocking clock divider tree element.
using ClockDividerBlocking = ClockDividerElement<ElementBlocking>;

/// Alias for a non-blocking clock divider tree element where updates cannot
/// fail.
using ClockDividerNonBlockingCannotFail =
    ClockDividerElement<ElementNonBlockingCannotFail>;

/// Alias for a non-blocking clock divider tree element where updates might
/// fail.
using ClockDividerNonBlockingMightFail =
    ClockDividerElement<ElementNonBlockingMightFail>;

/// Clock tree class that manages the state of clock tree elements.
///
/// The `ClockTree` provides the `Acquire` and `Release` methods to
/// acquire a reference to `ElementNonBlockingCannotFail`,
/// `ElementNonBlockingMightFail`, or `ElementBlocking` elements or
/// to the generic `Element` element. These functions will acquire the
/// proper lock to ensure that clock updates are synchronized.
///
/// The `SetDividerValue` method allows to change the divider value for
/// `ClockDividerNonBlockingCannotFail`, `ClockDividerNonBlockingMightFail`
/// or `ClockDividerBlocking` elements, or to the generic `ClockDivider`
/// element.
class ClockTree {
 public:
  /// Acquire a reference to a non-blocking clock tree element.
  /// Acquiring the clock tree element will succeed.
  void Acquire(ElementNonBlockingCannotFail& element) {
    std::lock_guard lock(interrupt_spin_lock_);
    Status status = element.Acquire();
    PW_DASSERT(status.ok());
  }

  /// Acquire a reference to a non-blocking clock tree element.
  /// Acquiring the clock tree element might fail.
  Status Acquire(ElementNonBlockingMightFail& element) {
    std::lock_guard lock(interrupt_spin_lock_);
    return element.Acquire();
  }

  /// Acquire a reference to a blocking clock tree element.
  /// Acquiring the clock tree element might fail.
  Status Acquire(ElementBlocking& element) {
    std::lock_guard lock(mutex_);
    return element.Acquire();
  }

  /// Acquire a reference to a clock tree element.
  /// Acquiring the clock tree element might fail.
  ///
  /// Note: May not be called from inside an interrupt context or with
  /// interrupts disabled.
  Status Acquire(Element& element) {
    if (element.may_block()) {
      std::lock_guard lock(mutex_);
      return element.Acquire();
    }
    std::lock_guard lock(interrupt_spin_lock_);
    return element.Acquire();
  }

  /// Acquire a reference to clock tree element `element` while `element_with`
  /// clock tree is enabled.
  /// Acquiring the clock tree element might fail.
  ///
  /// This is useful when dealing with synchronized clock muxes where, in order
  /// to switch to a new clock source, both the old and new clock must be
  /// running. This ensures that the old clock source (`element_with`) is
  /// running before attempting to activate the new clock source (`element`).
  ///
  /// Note: May not be called from inside an interrupt context or with
  /// interrupts disabled.
  Status AcquireWith(Element& element, Element& element_with) {
    PW_TRY(Acquire(element_with));
    Status status = Acquire(element);
    Release(element_with).IgnoreError();
    return status;
  }

  /// Release a reference to a non-blocking clock tree element.
  /// Releasing the clock tree element will succeed.
  void Release(ElementNonBlockingCannotFail& element) {
    std::lock_guard lock(interrupt_spin_lock_);
    Status status = element.Release();
    PW_DASSERT(status.ok());
  }

  /// Release a reference to a non-blocking clock tree element.
  /// Releasing the clock tree element might fail.
  Status Release(ElementNonBlockingMightFail& element) {
    std::lock_guard lock(interrupt_spin_lock_);
    return element.Release();
  }

  /// Release a reference to a blocking clock tree element.
  /// Releasing the clock tree element might fail.
  Status Release(ElementBlocking& element) {
    std::lock_guard lock(mutex_);
    return element.Release();
  }

  /// Release a reference to a clock tree element.
  /// Releasing the clock tree element might fail.
  ///
  /// Note: May not be called from inside an interrupt context or with
  /// interrupts disabled.
  Status Release(Element& element) {
    if (element.may_block()) {
      std::lock_guard lock(mutex_);
      return element.Release();
    }
    std::lock_guard lock(interrupt_spin_lock_);
    return element.Release();
  }

  /// Set divider value for a non-blocking clock divider element.
  /// Setting the clock divider value will succeed.
  void SetDividerValue(ClockDividerNonBlockingCannotFail& clock_divider,
                       uint32_t divider_value) {
    std::lock_guard lock(interrupt_spin_lock_);
    Status status = clock_divider.Set(divider_value);
    PW_DASSERT(status.ok());
  }

  /// Set divider value for a non-blocking clock divider element.
  /// Setting the clock divider value might fail.
  Status SetDividerValue(ClockDividerNonBlockingMightFail& clock_divider,
                         uint32_t divider_value) {
    std::lock_guard lock(interrupt_spin_lock_);
    return clock_divider.Set(divider_value);
  }

  /// Set divider value for a blocking clock divider element.
  /// Setting the clock divider value might fail.
  Status SetDividerValue(ClockDividerBlocking& clock_divider,
                         uint32_t divider_value) {
    std::lock_guard lock(mutex_);
    return clock_divider.Set(divider_value);
  }

  /// Set divider value for a clock divider element.
  /// Setting the clock divider value might fail.
  ///
  /// Note: May not be called from inside an interrupt context or with
  /// interrupts disabled.
  Status SetDividerValue(ClockDivider& clock_divider, uint32_t divider_value) {
    if (clock_divider.element().may_block()) {
      std::lock_guard lock(mutex_);
      return clock_divider.Set(divider_value);
    }
    std::lock_guard lock(interrupt_spin_lock_);
    return clock_divider.Set(divider_value);
  }

 protected:
  /// `mutex_` protects `ElementBlocking` clock tree elements.
  sync::Mutex mutex_;

  /// `interrupt_spin_lock_` protects `ElementNonBlockingCannotFail`
  /// and `ElementNonBlockingMightFail` clock tree elements.
  sync::InterruptSpinLock interrupt_spin_lock_;
};

/// Helper class that allows drivers to accept optional clock tree
/// information and streamline clock tree operations.
class ElementController {
 public:
  /// Create an element controller that accepts optional clock
  /// tree and element information.
  constexpr ElementController(ClockTree* clock_tree = nullptr,
                              Element* element = nullptr)
      : clock_tree_(clock_tree), element_(element) {}

  /// Acquire a reference to the optional clock tree element.
  ///
  /// If not both optional clock_tree and element pointers are
  /// non-null, the function just returns `pw::OkStatus()`.
  Status Acquire() {
    if (clock_tree_ != nullptr && element_ != nullptr) {
      return clock_tree_->Acquire(*element_);
    }
    return OkStatus();
  }

  /// Release a reference to the optional clock tree element.
  ///
  /// If not both optional clock_tree and element pointers are
  /// non-null, the function just returns `pw::OkStatus()`.
  Status Release() {
    if (clock_tree_ != nullptr && element_ != nullptr) {
      return clock_tree_->Release(*element_);
    }
    return OkStatus();
  }

  /// Pointer to optional `ClockTree` object.
  ClockTree* clock_tree_ = nullptr;

  /// Pointer to optional `Element` object.
  Element* element_ = nullptr;
};
}  // namespace pw::clock_tree
