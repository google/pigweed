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
#include <cpp-string/string_printf.h>

#include <cstdint>
#include <string>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"

namespace bt::hci_spec {

// Remote devices and local controllers have a feature set defined by the
// Link Manager Protocol.
// LMP features are organized into "pages", each containing a bit-mask of
// supported controller features. See Core Spec v5.0, Vol 2, Part C, Section 3.3
// "Feature Mask Definition".
// Three of these pages (the standard page plus two "extended feature" pages)
// are defined by the spec.

// See LMPFeature in hci_constants.h for the list of feature bits.
class LMPFeatureSet {
 public:
  // Creates a feature set with no pages set.
  LMPFeatureSet() : valid_pages_{false}, last_page_number_(0) {}

  // The maximum extended page that we support
  constexpr static uint8_t kMaxLastPageNumber = 2;
  constexpr static uint8_t kMaxPages = kMaxLastPageNumber + 1;

  // Returns true if |bit| is set in the LMP Features.
  // |page| is the page that this bit resides on.
  // Page 0 is the standard features.
  inline bool HasBit(size_t page, LMPFeature bit) const {
    return HasPage(page) && (features_[page] & static_cast<uint64_t>(bit));
  }

  // Sets |page| features to |features|
  inline void SetPage(size_t page, uint64_t features) {
    BT_ASSERT(page < kMaxPages);
    features_[page] = features;
    valid_pages_[page] = true;
  }

  // Returns true if the feature page |page| has been set.
  inline bool HasPage(size_t page) const {
    return (page < kMaxPages) && valid_pages_[page];
  }

  inline std::string ToString() const {
    std::string str;
    for (size_t i = 0; i <= last_page_number_; i++)
      if (HasPage(i))
        str += bt_lib_cpp_string::StringPrintf(
            "[P%zu: 0x%016lx]", i, features_[i]);
    return str;
  }

  inline void set_last_page_number(uint8_t page) {
    if (page > kMaxLastPageNumber) {
      bt_log(TRACE,
             "hci",
             "attempt to set lmp last page number to %u, capping at %u",
             page,
             kMaxLastPageNumber);
      last_page_number_ = kMaxLastPageNumber;
    } else {
      last_page_number_ = page;
    }
  }

  inline uint8_t last_page_number() const { return last_page_number_; }

 private:
  uint64_t features_[kMaxPages];
  bool valid_pages_[kMaxPages];
  uint8_t last_page_number_;
};

}  // namespace bt::hci_spec
