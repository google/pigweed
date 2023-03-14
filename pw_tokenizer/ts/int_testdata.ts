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

const IntDB = [
  ["%d", "-128", "%u", "4294967168", '\xff\x01'],
  ["%d", "-10", "%u", "4294967286", '\x13'],
  ["%d", "-9", "%u", "4294967287", '\x11'],
  ["%d", "-8", "%u", "4294967288", '\x0f'],
  ["%d", "-7", "%u", "4294967289", '\x0d'],
  ["%d", "-6", "%u", "4294967290", '\x0b'],
  ["%d", "-5", "%u", "4294967291", '\x09'],
  ["%d", "-4", "%u", "4294967292", '\x07'],
  ["%d", "-3", "%u", "4294967293", '\x05'],
  ["%d", "-2", "%u", "4294967294", '\x03'],
  ["%d", "-1", "%u", "4294967295", '\x01'],
  ["%d", "0", "%u", "0", '\x00'],
  ["%d", "1", "%u", "1", '\x02'],
  ["%d", "2", "%u", "2", '\x04'],
  ["%d", "3", "%u", "3", '\x06'],
  ["%d", "4", "%u", "4", '\x08'],
  ["%d", "5", "%u", "5", '\x0a'],
  ["%d", "6", "%u", "6", '\x0c'],
  ["%d", "7", "%u", "7", '\x0e'],
  ["%d", "8", "%u", "8", '\x10'],
  ["%d", "9", "%u", "9", '\x12'],
  ["%d", "10", "%u", "10", '\x14'],
  ["%d", "127", "%u", "127", '\xfe\x01'],
  ["%d", "-32768", "%u", "4294934528", '\xff\xff\x03'],
  ["%d", "652344632", "%u", "652344632", '\xf0\xf4\x8f\xee\x04'],
  ["%d", "18567", "%u", "18567", '\x8e\xa2\x02'],
  ["%d", "-14", "%u", "4294967282", '\x1b'],
  ["%d", "-2147483648", "%u", "2147483648", '\xff\xff\xff\xff\x0f'],
  ["%ld", "-14", "%lu", "4294967282", '\x1b'],
  ["%d", "2075650855", "%u", "2075650855", '\xce\xac\xbf\xbb\x0f'],
  ["%lld", "5922204476835468009", "%llu", "5922204476835468009", '\xd2\xcb\x8c\x90\x86\xe6\xf2\xaf\xa4\x01'],
  ["%lld", "-9223372036854775808", "%llu", "9223372036854775808", '\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01'],
  ["%lld", "3273441488341945355", "%llu", "3273441488341945355", '\x96\xb0\xae\x9a\x96\xec\xcc\xed\x5a'],
  ["%lld", "-9223372036854775807", "%llu", "9223372036854775809", '\xfd\xff\xff\xff\xff\xff\xff\xff\xff\x01'],
]

export default IntDB;
