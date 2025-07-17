// Copyright 2025 The Pigweed Authors
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

#include "pw_multibuf/size_report/receiver.h"

#include "pw_multibuf/size_report/common.h"

namespace pw::multibuf::size_report {

void BasicReceiver::CheckDemoLinkHeader(
    const examples::DemoLinkHeader& demo_link_header) {
  PW_ASSERT(demo_link_header.src_addr == kDemoLinkSender);
  PW_ASSERT(demo_link_header.dst_addr == kDemoLinkReceiver);
}

void BasicReceiver::CheckDemoLinkFooter(
    const examples::DemoLinkFooter& demo_link_footer, uint32_t checksum) {
  PW_ASSERT(demo_link_footer.crc32 == checksum);
}

void BasicReceiver::CheckDemoNetworkHeader(
    const examples::DemoNetworkHeader& demo_network_header,
    size_t payload_len) {
  PW_ASSERT(demo_network_header.src_addr == kDemoNetworkSender);
  PW_ASSERT(demo_network_header.dst_addr == kDemoNetworkReceiver);
  PW_ASSERT(demo_network_header.length == payload_len);
}

void BasicReceiver::CheckDemoTransportFirstHeader(
    const examples::DemoTransportFirstHeader& demo_transport_header) {
  segment_id_ = demo_transport_header.segment_id;
  offset_ = 0;
  remaining_ = demo_transport_header.total_length;
  CheckDemoTransportHeader(demo_transport_header);
}

void BasicReceiver::CheckDemoTransportHeader(
    const examples::DemoTransportHeader& demo_transport_header) {
  PW_ASSERT(demo_transport_header.segment_id == segment_id_);
  PW_ASSERT(demo_transport_header.offset == offset_);
  PW_ASSERT(demo_transport_header.length <= remaining_);

  offset_ += demo_transport_header.length;
  remaining_ -= demo_transport_header.length;
}

}  // namespace pw::multibuf::size_report
