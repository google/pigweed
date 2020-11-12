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

#include "pw_assert/assert.h"
#include "pw_bloat/bloat_this_binary.h"
#include "pw_log/log.h"
#include "pw_rpc/server.h"
#include "pw_sys_io/sys_io.h"

int volatile* unoptimizable;

class Output : public pw::rpc::ChannelOutput {
 public:
  Output() : ChannelOutput("output") {}

  std::span<std::byte> AcquireBuffer() override { return buffer_; }

  pw::Status SendAndReleaseBuffer(std::span<const std::byte> buffer) override {
    PW_DCHECK_PTR_EQ(buffer.data(), buffer_);
    return pw::sys_io::WriteBytes(buffer).status();
  }

 private:
  std::byte buffer_[128];
};

namespace my_product {

Output output;
pw::rpc::Channel channels[] = {pw::rpc::Channel::Create<1>(&output)};
pw::rpc::Server server(channels);

}  // namespace my_product

int main() {
  pw::bloat::BloatThisBinary();

  // Ensure we are paying the cost for log and assert.
  PW_CHECK_INT_GE(*unoptimizable, 0, "Ensure this CHECK logic stays");
  PW_LOG_INFO("We care about optimizing: %d", *unoptimizable);

  std::byte packet_buffer[128];
  pw::sys_io::ReadBytes(packet_buffer);
  pw::sys_io::WriteBytes(packet_buffer);

  my_product::server.ProcessPacket(packet_buffer, my_product::output);

  return static_cast<int>(packet_buffer[92]);
}
