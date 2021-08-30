// Copyright 2021 The Pigweed Authors
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

#include <cstring>

#include "pw_assert/check.h"
#include "pw_bloat/bloat_this_binary.h"
#include "pw_blob_store/blob_store.h"
#include "pw_kvs/flash_test_partition.h"
#include "pw_kvs/key_value_store.h"
#include "pw_log/log.h"

using pw::blob_store::BlobStore;

namespace {

char working_buffer[256];
volatile bool is_set;

constexpr size_t kMaxSectorCount = 64;
constexpr size_t kKvsMaxEntries = 32;

// For KVS magic value always use a random 32 bit integer rather than a human
// readable 4 bytes. See pw_kvs/format.h for more information.
static constexpr pw::kvs::EntryFormat kvs_format = {.magic = 0x22d3f8a0,
                                                    .checksum = nullptr};

volatile size_t kvs_entry_count;

pw::kvs::KeyValueStoreBuffer<kKvsMaxEntries, kMaxSectorCount> test_kvs(
    &pw::kvs::FlashTestPartition(), kvs_format);

int volatile* unoptimizable;

}  // namespace

int main() {
  pw::bloat::BloatThisBinary();

  // Start of base **********************
  // Ensure we are paying the cost for log and assert.
  PW_CHECK_INT_GE(*unoptimizable, 0, "Ensure this CHECK logic stays");
  PW_LOG_INFO("We care about optimizing: %d", *unoptimizable);

  void* result =
      std::memset((void*)working_buffer, sizeof(working_buffer), 0x55);
  is_set = (result != nullptr);

  test_kvs.Init().IgnoreError();  // TODO(pwbug/387): Handle Status properly

  unsigned kvs_value = 42;
  test_kvs.Put("example_key", kvs_value)
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly

  kvs_entry_count = test_kvs.size();

  unsigned read_value = 0;
  test_kvs.Get("example_key", &read_value)
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  test_kvs.Delete("example_key")
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly

  auto val = pw::kvs::FlashTestPartition().PartitionAddressToMcuAddress(0);
  PW_LOG_INFO("Use the variable. %u", unsigned(*val));

  std::array<std::byte, 32> blob_source_buffer;
  pw::ConstByteSpan write_data = std::span(blob_source_buffer);
  char name[16] = "BLOB";
  std::array<std::byte, 32> read_buffer;
  pw::ByteSpan read_span = read_buffer;
  PW_LOG_INFO("Do something so variables are used. %u, %c, %u",
              unsigned(write_data.size()),
              name[0],
              unsigned(read_span.size()));
  // End of base **********************

  // Start of deferred blob **********************
  constexpr size_t kBufferSize = 1;

  pw::blob_store::BlobStoreBuffer<kBufferSize> blob(
      name, pw::kvs::FlashTestPartition(), nullptr, test_kvs, kBufferSize);
  blob.Init().IgnoreError();  // TODO(pwbug/387): Handle Status properly

  // Use writer.
  constexpr size_t kMetadataBufferSize =
      BlobStore::BlobWriter::RequiredMetadataBufferSize(0);
  std::array<std::byte, kMetadataBufferSize> metadata_buffer;
  pw::blob_store::BlobStore::DeferredWriter writer(blob, metadata_buffer);
  writer.Open().IgnoreError();  // TODO(pwbug/387): Handle Status properly
  writer.Write(write_data)
      .IgnoreError();            // TODO(pwbug/387): Handle Status properly
  writer.Flush().IgnoreError();  // TODO(pwbug/387): Handle Status properly
  writer.Close().IgnoreError();  // TODO(pwbug/387): Handle Status properly

  // Use reader.
  pw::blob_store::BlobStore::BlobReader reader(blob);
  reader.Open().IgnoreError();  // TODO(pwbug/387): Handle Status properly
  pw::Result<pw::ConstByteSpan> get_result = reader.GetMemoryMappedBlob();
  PW_LOG_INFO("%d", get_result.ok());
  auto reader_result = reader.Read(read_span);
  reader.Close().IgnoreError();  // TODO(pwbug/387): Handle Status properly
  PW_LOG_INFO("%d", reader_result.ok());

  // End of deferred blob **********************

  return 0;
}
