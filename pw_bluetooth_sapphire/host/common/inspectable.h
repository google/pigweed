// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_INSPECTABLE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_INSPECTABLE_H_

#include <lib/fit/function.h>
#include <lib/sys/inspect/cpp/component.h>

#include <string>

#include <fbl/macros.h>

namespace bt {

// InspectableGuard is returned by |Inspectable::Mutable()|.
// It will update the corresponding Inspect property when it is destroyed.
// Therefore, the lifetime of InspectableGuard should usually be that of a temporary, or be scoped
// for a single update.
//
// InspectableGuard's primary use cases are calling non-const methods on objects and assigning
// member variable values in structs.
//
// Example:
//   StringInspectable<hci::LMPFeatureSet> lmp_features;
//   lmp_features.Mutable()->SetPage(page, features);
template <typename ValueT>
class InspectableGuard {
 public:
  InspectableGuard(ValueT& value, fit::closure update_cb)
      : value_(value), update_cb_(std::move(update_cb)) {}
  ~InspectableGuard() { update_cb_(); }

  ValueT& operator*() { return value_; }

  ValueT* operator->() { return &value_; }

 private:
  ValueT& value_;
  fit::closure update_cb_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(InspectableGuard);
};

// Inspectable is a utility class for keeping inspected values in sync with their corresponding
// Inspect property.
//
// PropertyT must be an Inspect property type such that PropertyT::Set(PropertyInnerT) is valid.
// PropertyInnerT corresponds to the type contained in PropertyT (e.g. string is contained by
// inspect::StringProperty).
//
// Example:
//   inspect::Inspector inspector;
//   auto& root = inspector.GetRoot();
//   Inspectable inspectable(std::string("A"), root.CreateString(std::string("property_name"));
//
//   // Hierarchy: { root: property_name: "A" }
//
//   inspectable.Set("B");
//
//   // Hierarchy: { root: property_name: "B" }
template <typename ValueT, typename PropertyT = inspect::StringProperty,
          typename PropertyInnerT = std::string>
class Inspectable {
 public:
  // When the desired property type DOES NOT match ValueT, a conversion function |convert| is
  // necessary. This is often the case when using optionals or when converting a number/enum into a
  // string is desired. Example:
  //   auto convert = [](std::optional<hci::HCIVersion> version) -> std::string {
  //     return version ? HCIVersionToString(*version) : "null";
  //   };
  //   Inspectable<std::optional<HCIVersion>> hci_version(
  //     HCIVersion::k5_0,
  //     inspect_node.CreateString("hci", ""),
  //     convert);
  using ConvertFunction = fit::function<PropertyInnerT(const ValueT&)>;
  Inspectable(ValueT initial_value, PropertyT property,
              ConvertFunction convert = &Inspectable::DefaultConvert)
      : value_(std::move(initial_value)),
        property_(std::move(property)),
        convert_(std::move(convert)) {
    static_assert(!std::is_pointer_v<ValueT>, "Pointer passed to Inspectable");

    // Update property immediately to ensure consistency between property and initial value.
    UpdateProperty();
  }

  Inspectable(Inspectable&&) = default;
  Inspectable& operator=(Inspectable&&) = default;
  virtual ~Inspectable() = default;

  const ValueT& value() const { return value_; }

  const ValueT& operator*() const { return value_; }

  const ValueT* operator->() const { return &value_; }

  // Update value and property. This is the ONLY place the value should be updated directly.
  const ValueT& Set(const ValueT& value) {
    value_ = value;
    UpdateProperty();
    return value_;
  }

  // Returns a InspectableGuard wrapper around the contained value that allows for non-const methods
  // to be called. The returned value should only be used as a temporary.
  InspectableGuard<ValueT> Mutable() {
    return InspectableGuard(value_, fit::bind_member(this, &Inspectable::UpdateProperty));
  }

  static PropertyInnerT DefaultConvert(const ValueT& value) { return value; }

 private:
  void UpdateProperty() { property_.Set(convert_(value_)); }

  ValueT value_;
  PropertyT property_;
  ConvertFunction convert_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Inspectable);
};

// Convenience Inspectable types:

template <typename ValueT>
using IntInspectable = Inspectable<ValueT, inspect::IntProperty, uint64_t>;

template <typename ValueT>
using UintInspectable = Inspectable<ValueT, inspect::UintProperty, uint64_t>;

template <typename ValueT>
using BoolInspectable = Inspectable<ValueT, inspect::BoolProperty, bool>;

template <typename ValueT>
using StringInspectable = Inspectable<ValueT, inspect::StringProperty, std::string>;

// A common practice in the Bluetooth stack is to define ToString() for classes.
// ToStringInspectable is for use with such a class |ValueT| where |ValueT::ToString()| is defined.
// |PropertyT::Set(std::string)| must accept a string.
// Example:
// class Foo {
//   public:
//     std::string ToString() { ... }
// };
//
// ToStringInspectable<Foo> foo(Foo(), inspect_node.CreateString("foo", ""));
template <class ValueT, class PropertyT = inspect::StringProperty>
class ToStringInspectable final : public Inspectable<ValueT, PropertyT, std::string> {
 public:
  ToStringInspectable(ValueT value, PropertyT property)
      : Inspectable<ValueT, PropertyT, std::string>(std::move(value), std::move(property),
                                                    &ToStringInspectable::Convert) {}
  ToStringInspectable(ToStringInspectable&&) = default;
  ToStringInspectable& operator=(ToStringInspectable&&) = default;

  static std::string Convert(const ValueT& value) { return value.ToString(); }

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ToStringInspectable);
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_INSPECTABLE_H_
