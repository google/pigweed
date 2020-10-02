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

#include <pw_fuzzer/asan_interface.h>
#include <pw_fuzzer/fuzzed_data_provider.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "pw_protobuf/encoder.h"

namespace {

// Encodable values. The fuzzer will iteratively choose different field types to
// generate and encode.
enum FieldType : uint8_t {
  kEncodeAndClear = 0,
  kUint32,
  kPackedUint32,
  kUint64,
  kPackedUint64,
  kInt32,
  kPackedInt32,
  kInt64,
  kPackedInt64,
  kSint32,
  kPackedSint32,
  kSint64,
  kPackedSint64,
  kBool,
  kFixed32,
  kPackedFixed32,
  kFixed64,
  kPackedFixed64,
  kSfixed32,
  kPackedSfixed32,
  kSfixed64,
  kPackedSfixed64,
  kFloat,
  kPackedFloat,
  kDouble,
  kPackedDouble,
  kBytes,
  kString,
  kPush,
  kPop,
  kMaxValue = kPop,
};

// TODO(pwbug/181): Move this to pw_fuzzer/fuzzed_data_provider.h

// Uses the given |provider| to pick and return a number between 0 and the
// maximum numbers of T that can be generated from the remaining input data.
template <typename T>
size_t ConsumeSize(FuzzedDataProvider* provider) {
  size_t max = provider->remaining_bytes() / sizeof(T);
  return provider->ConsumeIntegralInRange<size_t>(0, max);
}

// Uses the given |provider| to generate several instances of T, store them in
// |data|, and then return a std::span to them. It is the caller's responsbility
// to ensure |data| remains in scope as long as the returned std::span.
template <typename T>
std::span<const T> ConsumeSpan(FuzzedDataProvider* provider,
                               std::vector<T>* data) {
  size_t num = ConsumeSize<T>(provider);
  size_t off = data->size();
  data->reserve(off + num);
  for (size_t i = 0; i < num; ++i) {
    if constexpr (std::is_floating_point<T>::value) {
      data->push_back(provider->ConsumeFloatingPoint<T>());
    } else {
      data->push_back(provider->ConsumeIntegral<T>());
    }
  }
  return std::span(&((*data)[off]), num);
}

// Uses the given |provider| to generate a string, store it in |data|, and
// return a C-style representation. It is the caller's responsbility to
// ensure |data| remains in scope as long as the returned char*.
const char* ConsumeString(FuzzedDataProvider* provider,
                          std::vector<std::string>* data) {
  size_t off = data->size();
  // OSS-Fuzz's clang doesn't have the zero-parameter version of
  // ConsumeRandomLengthString yet.
  size_t max_length = std::numeric_limits<size_t>::max();
  data->push_back(provider->ConsumeRandomLengthString(max_length));
  return (*data)[off].c_str();
}

// Uses the given |provider| to generate non-arithmetic bytes, store them in
// |data|, and return a std::span to them. It is the caller's responsbility to
// ensure |data| remains in scope as long as the returned std::span.
std::span<const std::byte> ConsumeBytes(FuzzedDataProvider* provider,
                                        std::vector<std::byte>* data) {
  size_t num = ConsumeSize<std::byte>(provider);
  auto added = provider->ConsumeBytes<std::byte>(num);
  size_t off = data->size();
  num = added.size();
  data->insert(data->end(), added.begin(), added.end());
  return std::span(&((*data)[off]), num);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static std::byte buffer[65536];

  FuzzedDataProvider provider(data, size);

  // Pick a subset of the buffer that the fuzzer is allowed to use, and poison
  // the rest.
  size_t unpoisoned_length =
      provider.ConsumeIntegralInRange<size_t>(0, sizeof(buffer));
  std::span<std::byte> unpoisoned(buffer, unpoisoned_length);
  void* poisoned = &buffer[unpoisoned_length];
  size_t poisoned_length = sizeof(buffer) - unpoisoned_length;
  ASAN_POISON_MEMORY_REGION(poisoned, poisoned_length);

  pw::protobuf::NestedEncoder encoder(unpoisoned);

  // Storage for generated spans
  std::vector<uint32_t> u32s;
  std::vector<uint64_t> u64s;
  std::vector<int32_t> s32s;
  std::vector<int64_t> s64s;
  std::vector<float> floats;
  std::vector<double> doubles;
  std::vector<std::string> strings;
  std::vector<std::byte> bytes;

  // Consume the fuzzing input, using it to generate a sequence of fields to
  // encode. Both the uint32_t field IDs and the fields values are generated.
  // Don't try to detect errors, ensures pushes and pops are balanced, or
  // otherwise hold the interface correctly. Instead, fuzz the widest possbile
  // set of inputs to the encoder to ensure it doesn't misbehave.
  while (provider.remaining_bytes() != 0) {
    switch (provider.ConsumeEnum<FieldType>()) {
      case kEncodeAndClear:
        // Special "field". Encode all the fields so far and reset the encoder.
        encoder.Encode();
        encoder.Clear();
        break;
      case kUint32:
        encoder.WriteUint32(provider.ConsumeIntegral<uint32_t>(),
                            provider.ConsumeIntegral<uint32_t>());
        break;
      case kPackedUint32:
        encoder.WritePackedUint32(provider.ConsumeIntegral<uint32_t>(),
                                  ConsumeSpan<uint32_t>(&provider, &u32s));
        break;
      case kUint64:
        encoder.WriteUint64(provider.ConsumeIntegral<uint32_t>(),
                            provider.ConsumeIntegral<uint64_t>());
        break;
      case kPackedUint64:
        encoder.WritePackedUint64(provider.ConsumeIntegral<uint32_t>(),
                                  ConsumeSpan<uint64_t>(&provider, &u64s));
        break;
      case kInt32:
        encoder.WriteInt32(provider.ConsumeIntegral<uint32_t>(),
                           provider.ConsumeIntegral<int32_t>());
        break;
      case kPackedInt32:
        encoder.WritePackedInt32(provider.ConsumeIntegral<uint32_t>(),
                                 ConsumeSpan<int32_t>(&provider, &s32s));
        break;
      case kInt64:
        encoder.WriteInt64(provider.ConsumeIntegral<uint32_t>(),
                           provider.ConsumeIntegral<int64_t>());
        break;
      case kPackedInt64:
        encoder.WritePackedInt64(provider.ConsumeIntegral<uint32_t>(),
                                 ConsumeSpan<int64_t>(&provider, &s64s));
        break;
      case kSint32:
        encoder.WriteSint32(provider.ConsumeIntegral<uint32_t>(),
                            provider.ConsumeIntegral<int32_t>());
        break;
      case kPackedSint32:
        encoder.WritePackedSint32(provider.ConsumeIntegral<uint32_t>(),
                                  ConsumeSpan<int32_t>(&provider, &s32s));
        break;
      case kSint64:
        encoder.WriteSint64(provider.ConsumeIntegral<uint32_t>(),
                            provider.ConsumeIntegral<int64_t>());
        break;
      case kPackedSint64:
        encoder.WritePackedSint64(provider.ConsumeIntegral<uint32_t>(),
                                  ConsumeSpan<int64_t>(&provider, &s64s));
        break;
      case kBool:
        encoder.WriteBool(provider.ConsumeIntegral<uint32_t>(),
                          provider.ConsumeBool());
        break;
      case kFixed32:
        encoder.WriteFixed32(provider.ConsumeIntegral<uint32_t>(),
                             provider.ConsumeIntegral<uint32_t>());
        break;
      case kPackedFixed32:
        encoder.WritePackedFixed32(provider.ConsumeIntegral<uint32_t>(),
                                   ConsumeSpan<uint32_t>(&provider, &u32s));
        break;
      case kFixed64:
        encoder.WriteFixed64(provider.ConsumeIntegral<uint32_t>(),
                             provider.ConsumeIntegral<uint64_t>());
        break;
      case kPackedFixed64:
        encoder.WritePackedFixed64(provider.ConsumeIntegral<uint32_t>(),
                                   ConsumeSpan<uint64_t>(&provider, &u64s));
        break;
      case kSfixed32:
        encoder.WriteSfixed32(provider.ConsumeIntegral<uint32_t>(),
                              provider.ConsumeIntegral<int32_t>());
        break;
      case kPackedSfixed32:
        encoder.WritePackedSfixed32(provider.ConsumeIntegral<uint32_t>(),
                                    ConsumeSpan<int32_t>(&provider, &s32s));
        break;
      case kSfixed64:
        encoder.WriteSfixed64(provider.ConsumeIntegral<uint32_t>(),
                              provider.ConsumeIntegral<int64_t>());
        break;
      case kPackedSfixed64:
        encoder.WritePackedSfixed64(provider.ConsumeIntegral<uint32_t>(),
                                    ConsumeSpan<int64_t>(&provider, &s64s));
        break;
      case kFloat:
        encoder.WriteFloat(provider.ConsumeIntegral<uint32_t>(),
                           provider.ConsumeFloatingPoint<float>());
        break;
      case kPackedFloat:
        encoder.WritePackedFloat(provider.ConsumeIntegral<uint32_t>(),
                                 ConsumeSpan<float>(&provider, &floats));
        break;
      case kDouble:
        encoder.WriteDouble(provider.ConsumeIntegral<uint32_t>(),
                            provider.ConsumeFloatingPoint<double>());
        break;
      case kPackedDouble:
        encoder.WritePackedDouble(provider.ConsumeIntegral<uint32_t>(),
                                  ConsumeSpan<double>(&provider, &doubles));
        break;
      case kBytes:
        encoder.WriteBytes(provider.ConsumeIntegral<uint32_t>(),
                           ConsumeBytes(&provider, &bytes));
        break;
      case kString:
        encoder.WriteString(provider.ConsumeIntegral<uint32_t>(),
                            ConsumeString(&provider, &strings));
        break;
      case kPush:
        // Special "field". The marks the start of a nested message.
        encoder.Push(provider.ConsumeIntegral<uint32_t>());
        break;
      case kPop:
        // Special "field". this marks the end of a nested message. No attempt
        // is made to match pushes to pops, in order to test that the encoder
        // behaves correctly when they are mismatched.
        encoder.Pop();
        break;
    }
  }
  // Ensure we call `Encode` at least once.
  encoder.Encode();

  // Don't forget to unpoison for the next iteration!
  ASAN_UNPOISON_MEMORY_REGION(poisoned, poisoned_length);
  return 0;
}
