// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_l2cap.h"

#include <endian.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt::testing {

FakeL2cap::FakeL2cap(SendFrameCallback send_frame_callback,
                     UnexpectedPduCallback unexpected_pdu_callback,
                     l2cap::ChannelId largest_channel_id)
    : send_frame_callback_(std::move(send_frame_callback)),
      unexpected_pdu_callback_(std::move(unexpected_pdu_callback)),
      largest_channel_id_(largest_channel_id) {}

void FakeL2cap::RegisterHandler(l2cap::ChannelId cid, ChannelReceiveCallback callback) {
  ZX_ASSERT(cid < l2cap::kFirstDynamicChannelId);
  if (callbacks_.find(cid) != callbacks_.end()) {
    bt_log(WARN, "fake-hci", "Overwriting previous handler for Channel ID %hu", cid);
  }
  callbacks_.insert_or_assign(cid, std::move(callback));
}

void FakeL2cap::RegisterService(l2cap::PSM psm, FakeDynamicChannelCallback callback) {
  if (callbacks_.find(psm) != callbacks_.end()) {
    bt_log(WARN, "fake-hci", "Overwriting previous handler for PSM %.4hu", psm);
  }
  registered_services_.insert_or_assign(psm, std::move(callback));
}

bool FakeL2cap::RegisterDynamicChannel(hci::ConnectionHandle conn, l2cap::PSM psm,
                                       l2cap::ChannelId local_cid, l2cap::ChannelId remote_cid) {
  ZX_ASSERT(local_cid >= l2cap::kFirstDynamicChannelId);
  auto channel_map = dynamic_channels_.find(conn);
  if (channel_map != dynamic_channels_.end()) {
    if (channel_map->second.find(local_cid) == channel_map->second.end()) {
      bt_log(ERROR, "fake-hci", "Dynamic channel already exists at handle %hu and Channel ID %hu",
             conn, local_cid);
      return false;
    }
  }
  if (!ServiceRegisteredForPsm(psm)) {
    bt_log(ERROR, "fake-hci", "No service registered for psm %hu", psm);
    return false;
  }
  if (dynamic_channels_.find(conn) == dynamic_channels_.end()) {
    std::unordered_map<l2cap::ChannelId, std::unique_ptr<FakeDynamicChannel>> new_conn_map;
    dynamic_channels_.emplace(conn, std::move(new_conn_map));
  }
  dynamic_channels_.find(conn)->second.insert_or_assign(
      local_cid, std::make_unique<FakeDynamicChannel>(conn, psm, local_cid, remote_cid));
  return true;
}

bool FakeL2cap::RegisterDynamicChannelWithPsm(hci::ConnectionHandle conn,
                                              l2cap::ChannelId local_cid) {
  auto channel = FindDynamicChannelByLocalId(conn, local_cid);
  if (channel) {
    ZX_ASSERT(channel->opened());
    auto psm_iter = registered_services_.find(channel->psm());
    if (psm_iter == registered_services_.end()) {
      bt_log(ERROR, "fake-hci", "No service registered for psm %hu", channel->psm());
      return false;
    }
    psm_iter->second(channel);
    // If the callback does not assign a send_packet_callback, default to
    // FakeL2cap's version.
    if (!channel->send_packet_callback()) {
      auto send_cb = [this, channel](auto& packet) -> void {
        auto& l2cap_callback = this->send_frame_callback();
        l2cap_callback(channel->handle(), channel->local_cid(), packet);
      };
      channel->set_send_packet_callback(send_cb);
    }
    return true;
  }
  return false;
}

bool FakeL2cap::ServiceRegisteredForPsm(l2cap::PSM psm) {
  auto iter = registered_services_.find(psm);
  if (iter == registered_services_.end()) {
    return false;
  }
  return true;
}

l2cap::ChannelId FakeL2cap::FindAvailableDynamicChannelId(hci::ConnectionHandle conn) {
  auto channel_map = dynamic_channels_.find(conn);
  // If there are no dynamic channel connections on the ConnectHandle, assign
  // it the first ID
  if (channel_map == dynamic_channels_.end()) {
    return l2cap::kFirstDynamicChannelId;
  }
  for (l2cap::ChannelId id = l2cap::kFirstDynamicChannelId; id < largest_channel_id_ + 1; id++) {
    if (channel_map->second.count(id) == 0) {
      return id;
    }
  }
  return l2cap::kInvalidChannelId;
}

fxl::WeakPtr<FakeDynamicChannel> FakeL2cap::FindDynamicChannelByLocalId(
    hci::ConnectionHandle conn, l2cap::ChannelId local_cid) {
  auto channel_map = dynamic_channels_.find(conn);
  if (channel_map == dynamic_channels_.end()) {
    return nullptr;
  }
  auto channel = channel_map->second.find(local_cid);
  if (channel == channel_map->second.end()) {
    return nullptr;
  }
  return channel->second->AsWeakPtr();
}

fxl::WeakPtr<FakeDynamicChannel> FakeL2cap::FindDynamicChannelByRemoteId(
    hci::ConnectionHandle conn, l2cap::ChannelId remote_cid) {
  auto channel_map = dynamic_channels_.find(conn);
  if (channel_map == dynamic_channels_.end()) {
    return nullptr;
  }
  for (auto& [id, channel_ptr] : channel_map->second) {
    if (channel_ptr->remote_cid() == remote_cid) {
      return channel_ptr->AsWeakPtr();
    }
  }
  return nullptr;
}

void FakeL2cap::DeleteDynamicChannelByLocalId(hci::ConnectionHandle conn,
                                              l2cap::ChannelId local_cid) {
  auto channel_map = dynamic_channels_.find(conn);
  if (channel_map == dynamic_channels_.end()) {
    bt_log(ERROR, "hci-fake", "Attempt to delete unregistered channel.");
    return;
  }
  auto channel = channel_map->second.find(local_cid);
  if (channel == channel_map->second.end()) {
    bt_log(ERROR, "hci-fake", "Attempt to delete unregistered channel.");
    return;
  }
  channel->second = nullptr;
  dynamic_channels_[conn].erase(local_cid);
}

void FakeL2cap::HandlePdu(hci::ConnectionHandle conn, const ByteBuffer& pdu) {
  if (pdu.size() < sizeof(l2cap::BasicHeader)) {
    bt_log(WARN, "fake-hci", "malformed L2CAP packet!");
    return;
  }

  // Extract channel ID and strip the L2CAP header from the pdu.
  const auto& header = pdu.As<l2cap::BasicHeader>();
  l2cap::ChannelId cid = le16toh(header.channel_id);
  auto header_len = sizeof(header);
  auto payload_len = le16toh(header.length);
  auto sdu = DynamicByteBuffer(payload_len);
  pdu.Copy(&sdu, header_len, payload_len);

  // Execute corresponding handler function based on the channel type.
  if (cid < l2cap::kFirstDynamicChannelId) {
    auto handler = callbacks_.find(cid);
    if (handler == callbacks_.end()) {
      return unexpected_pdu_callback_(conn, pdu);
    } else {
      return handler->second(conn, sdu);
    }
  } else {
    auto channel = FindDynamicChannelByLocalId(conn, cid);
    if (channel) {
      auto& callback = channel->packet_handler_callback();
      return callback(sdu);
    }
  }
  return unexpected_pdu_callback_(conn, pdu);
}

}  // namespace bt::testing
