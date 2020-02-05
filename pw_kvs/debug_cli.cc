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

#include <iostream>
#include <sstream>
#include <string>

#include "pw_kvs/crc16_checksum.h"
#include "pw_kvs/in_memory_fake_flash.h"
#include "pw_kvs/key_value_store.h"

namespace pw::kvs {
namespace {

using std::byte;

ChecksumCrc16 checksum;
constexpr EntryHeaderFormat format{.magic = 0xBAD'C0D3, .checksum = &checksum};

void Run() {
  // 4 x 4k sectors, 16 byte alignment
  InMemoryFakeFlash<4 * 1024, 4> test_flash(16);

  FlashPartition test_partition(&test_flash, 0, test_flash.sector_count());
  test_partition.Erase(0, test_partition.sector_count());

  KeyValueStore kvs(&test_partition, format);
  kvs.Init();

  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream data(line);
    std::string cmd, key, value;
    data >> cmd >> key >> value;

    if (cmd == "init") {
      printf("Init() -> %s\n", kvs.Init().str());
    } else if (cmd == "delete") {
      printf("Delete(\"%s\") -> %s\n", key.c_str(), kvs.Delete(key).str());
    } else if (cmd == "put") {
      printf("Put(\"%s\", \"%s\") -> %s\n",
             key.c_str(),
             value.c_str(),
             kvs.Put(key, as_bytes(span(value))).str());
    } else if (cmd == "get") {
      byte buffer[128] = {};
      Status status = kvs.Get(key, buffer).status();
      printf("Get(\"%s\") -> %s\n", key.c_str(), status.str());
      if (status.ok()) {
        printf("  Key: \"%s\"\n", key.c_str());
        printf("Value: \"%s\"\n", reinterpret_cast<const char*>(buffer));
      }
    } else {
      int i = 0;
      printf("KVS CONTENTS ----------------------------------------------\n");
      for (auto& entry : kvs) {
        char value[64] = {};
        if (Status status = entry.Get(as_writable_bytes(span(value)));
            status.ok()) {
          printf("%2d: %s='%s'\n", ++i, entry.key().data(), value);
        } else {
          printf(
              "FAILED to Get key %s: %s\n", entry.key().data(), status.str());
        }
      }
      printf("---------------------------------------------- END CONTENTS\n");
    }
  }
}

}  // namespace
}  // namespace pw::kvs

int main() {
  pw::kvs::Run();
  return 0;
}
