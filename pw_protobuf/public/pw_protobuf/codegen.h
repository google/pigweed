// Copyright 2019 The Pigweed Authors
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

#include "pw_protobuf/encoder.h"

namespace pw::protobuf {

// Base class for generated encoders. Stores a reference to a low-level proto
// encoder. If representing a nested message, knows the field number of the
// message in its parent and and automatically calls Push and Pop when on the
// encoder when created/destroyed.
class ProtoMessageEncoder {
 public:
  ProtoMessageEncoder(Encoder* encoder, uint32_t parent_field = 0)
      : encoder_(encoder), parent_field_(parent_field) {
    if (parent_field_ != 0) {
      encoder_->Push(parent_field_);
    }
  }

  ~ProtoMessageEncoder() {
    if (parent_field_ != 0) {
      encoder_->Pop();
    }
  }

 protected:
  Encoder* encoder_;
  uint32_t parent_field_;
};

}  // namespace pw::protobuf
