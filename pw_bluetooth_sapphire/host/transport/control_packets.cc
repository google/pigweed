// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "control_packets.h"

#include "slab_allocators.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"

namespace bt::hci {
namespace {

// Limit CommandPacket template instantiations to 2 (small and large):
using SmallCommandPacket =
    allocators::internal::FixedSizePacket<hci_spec::CommandHeader,
                                          allocators::kSmallControlPacketSize>;
using LargeCommandPacket =
    allocators::internal::FixedSizePacket<hci_spec::CommandHeader,
                                          allocators::kLargeControlPacketSize>;

using EventFixedSizedPacket =
    allocators::internal::FixedSizePacket<hci_spec::EventHeader,
                                          allocators::kLargeControlPacketSize>;

// TODO(fxbug.dev/106841): Use Pigweed's slab allocator
std::unique_ptr<CommandPacket> NewCommandPacket(size_t payload_size) {
  BT_DEBUG_ASSERT(payload_size <= allocators::kLargeControlPayloadSize);

  if (payload_size <= allocators::kSmallControlPayloadSize) {
    return std::make_unique<SmallCommandPacket>(payload_size);
  }
  return std::make_unique<LargeCommandPacket>(payload_size);
}

// Returns true and populates the |out_code| field with the status parameter.
// Returns false if |event|'s payload is too small to hold a T. T must have a
// |status| member of type pw::bluetooth::emboss::StatusCode for this to compile.
template <typename T>
bool StatusCodeFromEvent(const EventPacket& event, pw::bluetooth::emboss::StatusCode* out_code) {
  BT_DEBUG_ASSERT(out_code);

  if (event.view().payload_size() < sizeof(T))
    return false;

  *out_code = event.params<T>().status;
  return true;
}

// For packet definitions that have been migrated to Emboss, parameterize this method over the
// Emboss view.
template <typename T>
bool StatusCodeFromEmbossEvent(const EventPacket& event,
                               pw::bluetooth::emboss::StatusCode* out_code) {
  BT_DEBUG_ASSERT(out_code);

  auto emboss_packet = EmbossEventPacket::New<T>(event.view().size());
  bt::MutableBufferView dest = emboss_packet.mutable_data();
  event.view().data().Copy(&dest);

  if (event.view().payload_size() <
      T::IntrinsicSizeInBytes().Read() -
          pw::bluetooth::emboss::EventHeader::IntrinsicSizeInBytes()) {
    return false;
  }

  *out_code = emboss_packet.view_t().status().Read();
  return true;
}

// As pw::bluetooth::emboss::StatusCodeFromEvent, but for LEMetaEvent subevents.
// Returns true and populates the |out_code| field with the subevent status parameter.
// Returns false if |event|'s payload is too small to hold a LEMetaEvent containing a T. T must have
// a |status| member of type pw::bluetooth::emboss::StatusCode for this to compile.
template <typename T>
bool StatusCodeFromSubevent(const EventPacket& event, pw::bluetooth::emboss::StatusCode* out_code) {
  BT_ASSERT(out_code);

  if (event.view().payload_size() < sizeof(hci_spec::LEMetaEventParams) + sizeof(T))
    return false;

  *out_code = event.subevent_params<T>()->status;
  return true;
}

// Specialization for the CommandComplete event.
template <>
bool StatusCodeFromEvent<hci_spec::CommandCompleteEventParams>(
    const EventPacket& event, pw::bluetooth::emboss::StatusCode* out_code) {
  BT_DEBUG_ASSERT(out_code);

  const auto* params = event.return_params<hci_spec::SimpleReturnParams>();
  if (!params)
    return false;

  *out_code = params->status;
  return true;
}

}  // namespace

namespace hci_android = bt::hci_spec::vendor::android;

// static
std::unique_ptr<CommandPacket> CommandPacket::New(hci_spec::OpCode opcode, size_t payload_size) {
  auto packet = NewCommandPacket(payload_size);
  if (!packet)
    return nullptr;

  packet->WriteHeader(opcode);
  return packet;
}

void CommandPacket::WriteHeader(hci_spec::OpCode opcode) {
  mutable_view()->mutable_header()->opcode = htole16(opcode);
  BT_ASSERT(view().payload_size() < std::numeric_limits<uint8_t>::max());
  mutable_view()->mutable_header()->parameter_total_size =
      static_cast<uint8_t>(view().payload_size());
}

// static
std::unique_ptr<EventPacket> EventPacket::New(size_t payload_size) {
  // TODO(fxbug.dev/106841): Use Pigweed's slab allocator
  return std::make_unique<EventFixedSizedPacket>(payload_size);
}

bool EventPacket::ToStatusCode(pw::bluetooth::emboss::StatusCode* out_code) const {
#define CASE_EVENT_STATUS(event_name)      \
  case hci_spec::k##event_name##EventCode: \
    return StatusCodeFromEvent<hci_spec::event_name##EventParams>(*this, out_code)

#define CASE_EMBOSS_EVENT_STATUS(event_name) \
  case hci_spec::k##event_name##EventCode:   \
    return StatusCodeFromEmbossEvent<pw::bluetooth::emboss::event_name##EventView>(*this, out_code)

#define CASE_SUBEVENT_STATUS(subevent_name)      \
  case hci_spec::k##subevent_name##SubeventCode: \
    return StatusCodeFromSubevent<hci_spec::subevent_name##SubeventParams>(*this, out_code)

  switch (event_code()) {
    CASE_EVENT_STATUS(AuthenticationComplete);
    CASE_EVENT_STATUS(ChangeConnectionLinkKeyComplete);
    CASE_EVENT_STATUS(CommandComplete);
    CASE_EVENT_STATUS(CommandStatus);
    CASE_EVENT_STATUS(ConnectionComplete);
    CASE_EVENT_STATUS(DisconnectionComplete);
    CASE_EVENT_STATUS(EncryptionChange);
    CASE_EVENT_STATUS(EncryptionKeyRefreshComplete);
    CASE_EVENT_STATUS(RemoteNameRequestComplete);
    CASE_EVENT_STATUS(ReadRemoteVersionInfoComplete);
    CASE_EVENT_STATUS(ReadRemoteSupportedFeaturesComplete);
    CASE_EVENT_STATUS(ReadRemoteExtendedFeaturesComplete);
    CASE_EVENT_STATUS(RoleChange);
    CASE_EVENT_STATUS(SimplePairingComplete);
    CASE_EVENT_STATUS(SynchronousConnectionComplete);
    CASE_EMBOSS_EVENT_STATUS(InquiryComplete);
    case hci_spec::kLEMetaEventCode: {
      auto subevent_code = params<hci_spec::LEMetaEventParams>().subevent_code;
      switch (subevent_code) {
        CASE_SUBEVENT_STATUS(LEAdvertisingSetTerminated);
        CASE_SUBEVENT_STATUS(LEConnectionComplete);
        CASE_SUBEVENT_STATUS(LEReadRemoteFeaturesComplete);
        default:
          BT_PANIC("LE subevent (%#.2x) not implemented!", subevent_code);
          break;
      }
    }

      // TODO(armansito): Complete this list.

    default:
      BT_PANIC("event (%#.2x) not implemented!", event_code());
      break;
  }
  return false;

#undef CASE_EVENT_STATUS
}

hci::Result<> EventPacket::ToResult() const {
  pw::bluetooth::emboss::StatusCode code;
  if (!ToStatusCode(&code)) {
    return bt::ToResult(HostError::kPacketMalformed);
  }
  return bt::ToResult(code);
}

void EventPacket::InitializeFromBuffer() {
  mutable_view()->Resize(view().header().parameter_total_size);
}

}  // namespace bt::hci
