// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "control_packets.h"

#include <zircon/assert.h>

#include "slab_allocators.h"

namespace bt::hci {
namespace slab_allocators {

// Slab-allocator traits for command packets.
using LargeCommandTraits =
    PacketTraits<CommandHeader, kLargeControlPacketSize, kNumLargeControlPackets>;
using SmallCommandTraits =
    PacketTraits<CommandHeader, kSmallControlPacketSize, kNumSmallControlPackets>;

// Slab-allocator traits for event packets. Since event packets are only
// received (and not sent) and because the packet size cannot be determined
// before the contents are read from the underlying channel, CommandChannel
// always allocates the largest possible buffer for events. Thus, a small buffer
// allocator is not needed.
using EventTraits = PacketTraits<EventHeader, kLargeControlPacketSize, kNumLargeControlPackets>;

using LargeCommandAllocator = fbl::SlabAllocator<LargeCommandTraits>;
using SmallCommandAllocator = fbl::SlabAllocator<SmallCommandTraits>;
using EventAllocator = fbl::SlabAllocator<EventTraits>;

}  // namespace slab_allocators

namespace {

std::unique_ptr<CommandPacket> NewCommandPacket(size_t payload_size) {
  ZX_DEBUG_ASSERT(payload_size <= slab_allocators::kLargeControlPayloadSize);

  if (payload_size <= slab_allocators::kSmallControlPayloadSize) {
    auto buffer = slab_allocators::SmallCommandAllocator::New(payload_size);
    if (buffer)
      return buffer;

    // We failed to allocate a small buffer; fall back to the large allocator.
  }

  return slab_allocators::LargeCommandAllocator::New(payload_size);
}

// Returns true and populates the |out_code| field with the status parameter.
// Returns false if |event|'s payload is too small to hold a T. T must have a
// |status| member of type hci::StatusCode for this to compile.
template <typename T>
bool StatusCodeFromEvent(const EventPacket& event, hci::StatusCode* out_code) {
  ZX_DEBUG_ASSERT(out_code);

  if (event.view().payload_size() < sizeof(T))
    return false;

  *out_code = event.params<T>().status;
  return true;
}

// As StatusCodeFromEvent, but for LEMetaEvent subevents.
// Returns true and populates the |out_code| field with the subevent status parameter.
// Returns false if |event|'s payload is too small to hold a LEMetaEvent containing a T. T must have
// a |status| member of type hci::StatusCode for this to compile.
template <typename T>
bool StatusCodeFromSubevent(const EventPacket& event, hci::StatusCode* out_code) {
  ZX_ASSERT(out_code);

  if (event.view().payload_size() < sizeof(LEMetaEventParams) + sizeof(T))
    return false;

  *out_code = event.le_event_params<T>()->status;
  return true;
}

// Specialization for the CommandComplete event.
template <>
bool StatusCodeFromEvent<CommandCompleteEventParams>(const EventPacket& event,
                                                     hci::StatusCode* out_code) {
  ZX_DEBUG_ASSERT(out_code);

  const auto* params = event.return_params<SimpleReturnParams>();
  if (!params)
    return false;

  *out_code = params->status;
  return true;
}

}  // namespace

// static
std::unique_ptr<CommandPacket> CommandPacket::New(OpCode opcode, size_t payload_size) {
  auto packet = NewCommandPacket(payload_size);
  if (!packet)
    return nullptr;

  packet->WriteHeader(opcode);
  return packet;
}

void CommandPacket::WriteHeader(OpCode opcode) {
  mutable_view()->mutable_header()->opcode = htole16(opcode);
  mutable_view()->mutable_header()->parameter_total_size = view().payload_size();
}

// static
std::unique_ptr<EventPacket> EventPacket::New(size_t payload_size) {
  return slab_allocators::EventAllocator::New(payload_size);
}

bool EventPacket::ToStatusCode(StatusCode* out_code) const {
#define CASE_EVENT_STATUS(event_name) \
  case k##event_name##EventCode:      \
    return StatusCodeFromEvent<event_name##EventParams>(*this, out_code)

#define CASE_SUBEVENT_STATUS(subevent_name) \
  case k##subevent_name##SubeventCode:      \
    return StatusCodeFromSubevent<subevent_name##SubeventParams>(*this, out_code)

  switch (event_code()) {
    CASE_EVENT_STATUS(AuthenticationComplete);
    CASE_EVENT_STATUS(ChangeConnectionLinkKeyComplete);
    CASE_EVENT_STATUS(CommandComplete);
    CASE_EVENT_STATUS(CommandStatus);
    CASE_EVENT_STATUS(ConnectionComplete);
    CASE_EVENT_STATUS(DisconnectionComplete);
    CASE_EVENT_STATUS(InquiryComplete);
    CASE_EVENT_STATUS(EncryptionChange);
    CASE_EVENT_STATUS(RemoteNameRequestComplete);
    CASE_EVENT_STATUS(ReadRemoteVersionInfoComplete);
    CASE_EVENT_STATUS(ReadRemoteSupportedFeaturesComplete);
    CASE_EVENT_STATUS(ReadRemoteExtendedFeaturesComplete);
    CASE_EVENT_STATUS(SimplePairingComplete);
    CASE_EVENT_STATUS(SynchronousConnectionComplete);
    case kLEMetaEventCode: {
      auto subevent_code = params<LEMetaEventParams>().subevent_code;
      switch (subevent_code) {
        CASE_SUBEVENT_STATUS(LEReadRemoteFeaturesComplete);
        default:
          ZX_PANIC("LE subevent (%#.2x) not implemented!", subevent_code);
          break;
      }
    }

      // TODO(armansito): Complete this list.

    default:
      ZX_PANIC("event (%#.2x) not implemented!", event_code());
      break;
  }
  return false;

#undef CASE_EVENT_STATUS
}

Status EventPacket::ToStatus() const {
  StatusCode code;
  if (!ToStatusCode(&code)) {
    return Status(HostError::kPacketMalformed);
  }
  return Status(code);
}

void EventPacket::InitializeFromBuffer() {
  mutable_view()->Resize(view().header().parameter_total_size);
}

}  // namespace bt::hci

DECLARE_STATIC_SLAB_ALLOCATOR_STORAGE(bt::hci::slab_allocators::LargeCommandTraits,
                                      bt::hci::slab_allocators::kMaxNumSlabs, true);
DECLARE_STATIC_SLAB_ALLOCATOR_STORAGE(bt::hci::slab_allocators::SmallCommandTraits,
                                      bt::hci::slab_allocators::kMaxNumSlabs, true);
DECLARE_STATIC_SLAB_ALLOCATOR_STORAGE(bt::hci::slab_allocators::EventTraits,
                                      bt::hci::slab_allocators::kMaxNumSlabs, true);
