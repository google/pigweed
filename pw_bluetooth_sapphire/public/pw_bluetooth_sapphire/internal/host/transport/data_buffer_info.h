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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_DATA_BUFFER_INFO_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_DATA_BUFFER_INFO_H_

#include <stddef.h>

namespace bt::hci {

// Represents the controller data buffer settings for one of the BR/EDR, LE, or
// SCO transports.
class DataBufferInfo {
 public:
  // Initialize fields to non-zero values.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  DataBufferInfo(size_t max_data_length, size_t max_num_packets)
      : max_data_length_(max_data_length), max_num_packets_(max_num_packets) {}

  // The default constructor sets all fields to zero. This can be used to
  // represent a data buffer that does not exist (e.g. the controller has a
  // single shared buffer and no dedicated LE buffer).
  DataBufferInfo() = default;

  // The maximum length (in octets) of the data portion of each HCI
  // packet that the controller is able to accept.
  size_t max_data_length() const { return max_data_length_; }

  // Returns the total number of HCI packets that can be stored in the
  // data buffer represented by this object.
  size_t max_num_packets() const { return max_num_packets_; }

  // Returns true if both fields are set to non-zero.
  bool IsAvailable() const { return max_data_length_ && max_num_packets_; }

  // Comparison operators.
  bool operator==(const DataBufferInfo& other) const {
    return max_data_length_ == other.max_data_length_ &&
           max_num_packets_ == other.max_num_packets_;
  }
  bool operator!=(const DataBufferInfo& other) const {
    return !(*this == other);
  }

 private:
  size_t max_data_length_ = 0u;
  size_t max_num_packets_ = 0u;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_DATA_BUFFER_INFO_H_
