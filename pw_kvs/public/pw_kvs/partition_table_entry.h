// Copyright 2020 The Pigweed Authors
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

namespace pw {

namespace partition_table {

enum DataType { kRawData, kKvs };

}  // namespace partition_table

enum class PartitionPermission : bool {
  kReadOnly = true,
  kReadAndWrite = false,
};

struct PartitionTableEntry {
  constexpr PartitionTableEntry(
      const uint32_t start_sector_index,
      const uint32_t end_sector_index,
      const uint8_t partition_identifier,
      partition_table::DataType datatype,
      PartitionPermission permission = PartitionPermission::kReadAndWrite)
      : partition_start_sector_index(start_sector_index),
        partition_end_sector_index(end_sector_index),
        partition_id(partition_identifier),
        data_type(datatype),
        partition_permission(permission) {}

  const uint32_t partition_start_sector_index;
  const uint32_t partition_end_sector_index;
  const uint8_t partition_id;
  const partition_table::DataType data_type;
  const PartitionPermission partition_permission;
};

}  // namespace pw
