// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "channel_manager.h"

#include <zircon/assert.h>

#include "logical_link.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace bt {
namespace l2cap {

ChannelManager::ChannelManager(size_t max_acl_payload_size, size_t max_le_payload_size,
                               SendAclCallback send_acl_cb, async_dispatcher_t* l2cap_dispatcher)
    : max_acl_payload_size_(max_acl_payload_size),
      max_le_payload_size_(max_le_payload_size),
      send_acl_cb_(std::move(send_acl_cb)),
      l2cap_dispatcher_(l2cap_dispatcher),
      weak_ptr_factory_(this) {
  ZX_ASSERT(send_acl_cb_);
  ZX_ASSERT(l2cap_dispatcher_);
}

ChannelManager::~ChannelManager() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  // Explicitly shut down all links to force associated L2CAP channels to
  // release their strong references.
  for (auto& [handle, link] : ll_map_) {
    link->Close();
  }
}

hci::ACLPacketHandler ChannelManager::MakeInboundDataHandler() {
  return [self = weak_ptr_factory_.GetWeakPtr()](auto packet) {
    if (self) {
      self->OnACLDataReceived(std::move(packet));
    }
  };
}

void ChannelManager::RegisterACL(hci::ConnectionHandle handle, hci::Connection::Role role,
                                 LinkErrorCallback link_error_cb,
                                 SecurityUpgradeCallback security_cb,
                                 async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  bt_log(TRACE, "l2cap", "register ACL link (handle: %#.4x)", handle);

  auto* ll = RegisterInternal(handle, hci::Connection::LinkType::kACL, role, max_acl_payload_size_);
  ll->set_error_callback(std::move(link_error_cb), dispatcher);
  ll->set_security_upgrade_callback(std::move(security_cb), dispatcher);
}

void ChannelManager::RegisterLE(hci::ConnectionHandle handle, hci::Connection::Role role,
                                LEConnectionParameterUpdateCallback conn_param_cb,
                                LinkErrorCallback link_error_cb,
                                SecurityUpgradeCallback security_cb,
                                async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  bt_log(TRACE, "l2cap", "register LE link (handle: %#.4x)", handle);

  auto* ll = RegisterInternal(handle, hci::Connection::LinkType::kLE, role, max_le_payload_size_);
  ll->set_error_callback(std::move(link_error_cb), dispatcher);
  ll->set_security_upgrade_callback(std::move(security_cb), dispatcher);
  ll->le_signaling_channel()->set_conn_param_update_callback(std::move(conn_param_cb), dispatcher);
}

void ChannelManager::Unregister(hci::ConnectionHandle handle) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  bt_log(TRACE, "l2cap", "unregister link (handle: %#.4x)", handle);

  pending_packets_.erase(handle);
  auto iter = ll_map_.find(handle);
  ZX_DEBUG_ASSERT_MSG(iter != ll_map_.end(), "attempted to remove unknown connection handle: %#.4x",
                      handle);

  // Explicitly shut down the link to force associated L2CAP channels to release
  // their strong references.
  iter->second->Close();
  ll_map_.erase(iter);
}

void ChannelManager::AssignLinkSecurityProperties(hci::ConnectionHandle handle,
                                                  sm::SecurityProperties security) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  bt_log(TRACE, "l2cap", "received new security properties (handle: %#.4x)", handle);

  auto iter = ll_map_.find(handle);
  if (iter == ll_map_.end()) {
    bt_log(TRACE, "l2cap", "ignoring new security properties on unknown link");
    return;
  }

  iter->second->AssignSecurityProperties(security);
}

fbl::RefPtr<Channel> ChannelManager::OpenFixedChannel(hci::ConnectionHandle handle,
                                                      ChannelId channel_id) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  auto iter = ll_map_.find(handle);
  if (iter == ll_map_.end()) {
    bt_log(ERROR, "l2cap", "cannot open fixed channel on unknown connection handle: %#.4x", handle);
    return nullptr;
  }

  return iter->second->OpenFixedChannel(channel_id);
}

void ChannelManager::OpenChannel(hci::ConnectionHandle handle, PSM psm, ChannelCallback cb,
                                 async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  auto iter = ll_map_.find(handle);
  if (iter == ll_map_.end()) {
    bt_log(ERROR, "l2cap", "Cannot open channel on unknown connection handle: %#.4x", handle);
    async::PostTask(dispatcher, [cb = std::move(cb)] { cb(nullptr); });
    return;
  }

  iter->second->OpenChannel(psm, std::move(cb), dispatcher);
}

bool ChannelManager::RegisterService(PSM psm, ChannelCallback cb, async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  // v5.0 Vol 3, Part A, Sec 4.2: PSMs shall be odd and the least significant
  // bit of the most significant byte shall be zero
  if (((psm & 0x0001) != 0x0001) || ((psm & 0x0100) != 0x0000)) {
    return false;
  }

  auto iter = services_.find(psm);
  if (iter != services_.end()) {
    return false;
  }

  // Bind |dispatcher| in callback that forwards the created channel to the
  // service provider.
  ChannelCallback pass_channel = [this, dispatcher,
                                  cb = std::move(cb)](fbl::RefPtr<Channel> chan) mutable {
    ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

    // Do not transfer ownership of |cb| when passing a channel to L2CAP.
    // |chan| is safe to move because its lifetime spans each invocation.
    async::PostTask(dispatcher, [cb = cb.share(), chan = std::move(chan)] { cb(std::move(chan)); });
  };

  services_[psm] = std::move(pass_channel);
  return true;
}

void ChannelManager::UnregisterService(PSM psm) {
  FXL_DCHECK(thread_checker_.IsCreationThreadCurrent());

  services_.erase(psm);
}

// Called when an ACL data packet is received from the controller. This method
// is responsible for routing the packet to the corresponding LogicalLink.
void ChannelManager::OnACLDataReceived(hci::ACLDataPacketPtr packet) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  auto handle = packet->connection_handle();

  auto iter = ll_map_.find(handle);
  PendingPacketMap::iterator pp_iter;

  // If a LogicalLink does not exist, we set up a queue for its packets to be
  // delivered when the LogicalLink gets created.
  if (iter == ll_map_.end()) {
    pp_iter = pending_packets_.emplace(handle, LinkedList<hci::ACLDataPacket>()).first;
  } else {
    // A logical link exists. |pp_iter| will be valid only if the drain task has
    // not run yet (see ChannelManager::RegisterInternal()).
    pp_iter = pending_packets_.find(handle);
  }

  if (pp_iter != pending_packets_.end()) {
    pp_iter->second.push_back(std::move(packet));
    bt_log(SPEW, "l2cap", "queued rx packet on handle: %#.4x", handle);
    return;
  }

  iter->second->HandleRxPacket(std::move(packet));
}

internal::LogicalLink* ChannelManager::RegisterInternal(hci::ConnectionHandle handle,
                                                        hci::Connection::LinkType ll_type,
                                                        hci::Connection::Role role,
                                                        size_t max_payload_size) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  // TODO(armansito): Return nullptr instead of asserting. Callers shouldn't
  // assume this will succeed.
  auto iter = ll_map_.find(handle);
  ZX_DEBUG_ASSERT_MSG(iter == ll_map_.end(), "connection handle re-used! (handle=%#.4x)", handle);

  auto send_acl_cb = [this, ll_type](auto packets) {
    return send_acl_cb_(std::move(packets), ll_type);
  };

  auto ll = internal::LogicalLink::New(handle, ll_type, role, l2cap_dispatcher_, max_payload_size,
                                       std::move(send_acl_cb),
                                       fit::bind_member(this, &ChannelManager::QueryService));

  // Route all pending packets to the link.
  auto pp_iter = pending_packets_.find(handle);
  if (pp_iter != pending_packets_.end()) {
    auto& packets = pp_iter->second;
    while (!packets.is_empty()) {
      ll->HandleRxPacket(packets.pop_front());
    }
    pending_packets_.erase(pp_iter);
  }

  auto* ll_raw = ll.get();
  ll_map_[handle] = std::move(ll);

  return ll_raw;
}

ChannelCallback ChannelManager::QueryService(hci::ConnectionHandle handle, PSM psm) {
  auto iter = services_.find(psm);
  if (iter == services_.end()) {
    return nullptr;
  }

  // This will be called in LogicalLink. Each callback in |services_| already trampolines to the
  // appropriate dispatcher (passed to RegisterService).
  return iter->second.share();
}

}  // namespace l2cap
}  // namespace bt
