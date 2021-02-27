// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_WINDOWED_INSPECT_NUMERIC_PROPERTY_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_WINDOWED_INSPECT_NUMERIC_PROPERTY_H_

#include <lib/async/cpp/task.h>
#include <lib/async/cpp/time.h>
#include <lib/async/default.h>
#include <lib/sys/inspect/cpp/component.h>

#include <queue>

namespace bt {

// This class is a utility for wrapping a numeric Inspect property, such as IntProperty or
// UintProperty, such that value updates are reversed after |expiry_duration|. This is useful for
// creating properties like "disconnects in the past 10 minutes". Note that this is not very space
// efficient and should not be used for properties that get updated extremely frequently.
//
// |NumericPropertyT| is an inspect property like inspect::IntProperty, and |ValueT| is the internal
// value like int64_t. Use the convenience types below to simplify type declaration.
template <typename NumericPropertyT, typename ValueT>
class WindowedInspectNumericProperty {
 public:
  // |expiry_duration| is the time duration after which changes should be reversed.
  explicit WindowedInspectNumericProperty(zx::duration expiry_duration = zx::min(10))
      : expiry_duration_(expiry_duration) {}
  virtual ~WindowedInspectNumericProperty() = default;

  // Allow moving, disallow copying.
  WindowedInspectNumericProperty(const WindowedInspectNumericProperty& other) = delete;
  WindowedInspectNumericProperty(WindowedInspectNumericProperty&& other) noexcept = default;
  WindowedInspectNumericProperty& operator=(const WindowedInspectNumericProperty& other) = delete;
  WindowedInspectNumericProperty& operator=(WindowedInspectNumericProperty&& other) noexcept =
      default;

  // Set the underlying inspect property, and reset the expiry timer. This is used by the
  // convenience types that implement AttachInspect() below.
  void SetProperty(NumericPropertyT property) {
    property_ = std::move(property);
    expiry_task_.Cancel();
    // Clear queue without running pop() in a loop.
    values_ = decltype(values_)();
  }

  // Create an inspect property named "name" as a child of "node".
  //
  // AttachInspect is only supported for the convenience types declared below.
  virtual void AttachInspect(::inspect::Node& node, std::string name) {
    ZX_ASSERT_MSG(false, "AttachInspect not implemented for NumericPropertyT");
  }

  // Add the given value to the value of this numeric metric.
  void Add(ValueT value) {
    zx::time now = async::Now(async_get_default_dispatcher());
    values_.push({now, value});
    property_.Add(value);
    StartExpiryTimeout();
  }

  // Return true if property is valid.
  explicit operator bool() { return property_; }

 private:
  void StartExpiryTimeout() {
    if (values_.empty() || expiry_task_.is_pending()) {
      return;
    }

    auto oldest_value = values_.front();
    zx::time oldest_value_time = oldest_value.first;
    zx::time expiry_time = expiry_duration_ + oldest_value_time;
    expiry_task_.PostForTime(async_get_default_dispatcher(), expiry_time);
  }

  void OnExpiryTimeout() {
    ZX_ASSERT(!values_.empty());
    auto oldest_value = values_.front();
    // Undo expiring value.
    property_.Subtract(oldest_value.second);
    values_.pop();
    StartExpiryTimeout();
  }

  // This is not very space efficient, requiring a node for every value during the expiry_duration_.
  std::queue<std::pair<zx::time, ValueT>> values_;

  NumericPropertyT property_;
  zx::duration expiry_duration_;

  using SelfT = WindowedInspectNumericProperty<NumericPropertyT, ValueT>;
  async::TaskClosureMethod<SelfT, &SelfT::OnExpiryTimeout> expiry_task_{this};
};

// Convenience WindowedInspectNumericProperty types:

#define CREATE_WINDOWED_TYPE(property_t, inner_t)                                         \
  class WindowedInspect##property_t##Property final                                       \
      : public WindowedInspectNumericProperty<::inspect::property_t##Property, inner_t> { \
   public:                                                                                \
    using WindowedInspectNumericProperty<::inspect::property_t##Property,                 \
                                         inner_t>::WindowedInspectNumericProperty;        \
    void AttachInspect(::inspect::Node& node, std::string name) override {                \
      this->SetProperty(node.Create##property_t(name, inner_t()));                        \
    }                                                                                     \
  };

// WindowedInspectIntProperty
CREATE_WINDOWED_TYPE(Int, int64_t);
// WindowedInspectUintProperty
CREATE_WINDOWED_TYPE(Uint, uint64_t);

#undef CREATE_WINDOWED_TYPE

};  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_WINDOWED_INSPECT_NUMERIC_PROPERTY_H_
