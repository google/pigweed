// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_WINDOWED_INSPECT_NUMERIC_PROPERTY_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_WINDOWED_INSPECT_NUMERIC_PROPERTY_H_

#include <lib/async/cpp/time.h>

#include <optional>
#include <queue>

#include <pw_async_fuchsia/util.h>

#include "lib/zx/time.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspect.h"
#include "src/connectivity/bluetooth/core/bt-host/common/smart_task.h"

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
  // |min_resolution| is the smallest duration between changes such that they are reversed
  // independently. Changes closer to this interval may be batched together for expiry, biased
  // towards earlier expiry than |expiry_duration|. May be 0 (default) to disable batching.
  explicit WindowedInspectNumericProperty(pw::async::Dispatcher& pw_dispatcher,
                                          zx::duration expiry_duration = zx::min(10),
                                          zx::duration min_resolution = zx::duration())
      : expiry_duration_(expiry_duration),
        min_resolution_(min_resolution),
        expiry_task_(pw_dispatcher,
                     [this](pw::async::Context /*ctx*/, pw::Status status) {
                       if (!status.ok()) {
                         return;
                       }

                       BT_ASSERT(!values_.empty());
                       auto oldest_value = values_.front();
                       // Undo expiring value.
                       property_.Subtract(oldest_value.second);
                       values_.pop();
                       StartExpiryTimeout();
                     }),
        pw_dispatcher_(pw_dispatcher) {}
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
    BT_ASSERT_MSG(false, "AttachInspect not implemented for NumericPropertyT");
  }

  // Add the given value to the value of this numeric metric.
  void Add(ValueT value) {
    property_.Add(value);
    zx::time now = pw_async_fuchsia::TimepointToZxTime(pw_dispatcher_.now());
    if (!values_.empty()) {
      // If the most recent change's age is less than |min_resolution_|, merge this change to it
      if (now < values_.back().first + min_resolution_) {
        values_.back().second += value;
        return;
      }
    }
    values_.push({now, value});
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
    expiry_task_.PostAt(pw_async_fuchsia::ZxTimeToTimepoint(expiry_time));
  }

  // This is not very space efficient, requiring a node for every value during the expiry_duration_.
  std::queue<std::pair<zx::time, ValueT>> values_;

  NumericPropertyT property_;
  zx::duration expiry_duration_;
  zx::duration min_resolution_;

  using SelfT = WindowedInspectNumericProperty<NumericPropertyT, ValueT>;
  SmartTask expiry_task_;
  pw::async::Dispatcher& pw_dispatcher_;
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
CREATE_WINDOWED_TYPE(Int, int64_t)
// WindowedInspectUintProperty
CREATE_WINDOWED_TYPE(Uint, uint64_t)

#undef CREATE_WINDOWED_TYPE

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_WINDOWED_INSPECT_NUMERIC_PROPERTY_H_
