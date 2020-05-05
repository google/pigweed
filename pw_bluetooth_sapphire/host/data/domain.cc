// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "domain.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/task_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/data/socket_factory.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"

namespace bt {
namespace data {

class Impl final : public Domain, public TaskDomain<Impl, Domain> {
 public:
  Impl(fxl::RefPtr<hci::Transport> hci, inspect::Node node, async_dispatcher_t* dispatcher)
      : Domain(), TaskDomain<Impl, Domain>(this, dispatcher), node_(std::move(node)), hci_(hci) {
    ZX_DEBUG_ASSERT(hci_);
  }

  void Initialize() override {
    PostMessage([this] {
      // This can only run once during initialization.
      ZX_DEBUG_ASSERT(!l2cap_);

      InitializeL2CAP();
      l2cap_socket_factory_ = std::make_unique<internal::SocketFactory<l2cap::Channel>>();

      bt_log(TRACE, "data-domain", "initialized");
    });
  }

  void ShutDown() override { TaskDomain<Impl, Domain>::ScheduleCleanUp(); }

  // Called by the domain dispatcher as a result of ScheduleCleanUp().
  void CleanUp() {
    AssertOnDispatcherThread();
    bt_log(TRACE, "data-domain", "shutting down");
    l2cap_socket_factory_ = nullptr;

    // If l2cap has been initialized, we should have an acl_data_channel, as the data domain is not
    // initialized until the acl channel is initialized.
    if (l2cap_) {
      ZX_ASSERT(hci_->acl_data_channel());
      hci_->acl_data_channel()->SetDataRxHandler(nullptr, nullptr);
    }
    l2cap_ = nullptr;
  }

  void AddACLConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                        l2cap::LinkErrorCallback link_error_callback,
                        l2cap::SecurityUpgradeCallback security_callback) override {
    PostMessage([this, handle, role, lec = std::move(link_error_callback),
                 suc = std::move(security_callback)]() mutable {
      if (l2cap_) {
        l2cap_->RegisterACL(handle, role, std::move(lec), std::move(suc));
      }
    });
  }

  void AddLEConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                       l2cap::LinkErrorCallback link_error_callback,
                       l2cap::LEConnectionParameterUpdateCallback conn_param_callback,
                       l2cap::LEFixedChannelsCallback channel_callback,
                       l2cap::SecurityUpgradeCallback security_callback,
                       async_dispatcher_t* dispatcher) override {
    PostMessage([this, handle, role, cpc = std::move(conn_param_callback),
                 lec = std::move(link_error_callback), suc = std::move(security_callback),
                 cc = std::move(channel_callback), dispatcher]() mutable {
      if (l2cap_) {
        l2cap_->RegisterLE(handle, role, std::move(cpc), std::move(lec), std::move(suc),
                           dispatcher);

        auto att = l2cap_->OpenFixedChannel(handle, l2cap::kATTChannelId);
        auto smp = l2cap_->OpenFixedChannel(handle, l2cap::kLESMPChannelId);
        ZX_DEBUG_ASSERT(att);
        ZX_DEBUG_ASSERT(smp);
        async::PostTask(dispatcher,
                        [att = std::move(att), smp = std::move(smp), cb = std::move(cc)]() mutable {
                          cb(std::move(att), std::move(smp));
                        });
      }
    });
  }

  void RemoveConnection(hci::ConnectionHandle handle) override {
    PostMessage([this, handle] {
      if (l2cap_) {
        l2cap_->Unregister(handle);
      }
    });
  }

  void AssignLinkSecurityProperties(hci::ConnectionHandle handle,
                                    sm::SecurityProperties security) override {
    PostMessage([this, handle, security] {
      if (l2cap_) {
        l2cap_->AssignLinkSecurityProperties(handle, security);
      }
    });
  }

  void RequestConnectionParameterUpdate(hci::ConnectionHandle handle,
                                        hci::LEPreferredConnectionParameters params,
                                        l2cap::ConnectionParameterUpdateRequestCallback request_cb,
                                        async_dispatcher_t* dispatcher) override {
    PostMessage([this, handle, params, cb = std::move(request_cb), dispatcher]() mutable {
      if (l2cap_) {
        l2cap_->RequestConnectionParameterUpdate(handle, params, std::move(cb), dispatcher);
      }
    });
  }

  void OpenL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                        l2cap::ChannelParameters params, l2cap::ChannelCallback cb,
                        async_dispatcher_t* dispatcher) override {
    ZX_DEBUG_ASSERT(dispatcher);
    PostMessage([this, handle, psm, params, cb = std::move(cb), dispatcher]() mutable {
      if (l2cap_) {
        l2cap_->OpenChannel(handle, psm, params, std::move(cb), dispatcher);
      }
    });
  }

  void OpenL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                        l2cap::ChannelParameters params, SocketCallback socket_callback,
                        async_dispatcher_t* cb_dispatcher) override {
    ZX_DEBUG_ASSERT(cb_dispatcher);
    OpenL2capChannel(
        handle, psm, params,
        [this, handle, cb = std::move(socket_callback), cb_dispatcher](auto channel) mutable {
          // MakeSocketForChannel makes invalid sockets for null channels (i.e.
          // that have failed to open).
          zx::socket s = l2cap_socket_factory_->MakeSocketForChannel(channel);
          auto chan_info = channel ? std::optional(channel->info()) : std::nullopt;
          l2cap::ChannelSocket chan_sock(std::move(s), chan_info);
          async::PostTask(cb_dispatcher, [chan_sock = std::move(chan_sock), cb = std::move(cb),
                                          handle]() mutable { cb(std::move(chan_sock), handle); });
        },
        dispatcher());
  }

  void RegisterService(l2cap::PSM psm, l2cap::ChannelParameters params,
                       l2cap::ChannelCallback callback, async_dispatcher_t* dispatcher) override {
    PostMessage([this, psm, params, callback = std::move(callback), dispatcher]() mutable {
      if (l2cap_) {
        const bool result = l2cap_->RegisterService(psm, params, std::move(callback), dispatcher);
        ZX_DEBUG_ASSERT(result);
      } else {
        // RegisterService could be called early in host initialization, so log
        // cases where L2CAP isn't ready for a service handler.
        bt_log(WARN, "l2cap", "failed to register handler for PSM %#.4x while uninitialized", psm);
      }
    });
  }

  void RegisterService(l2cap::PSM psm, l2cap::ChannelParameters params,
                       SocketCallback socket_callback, async_dispatcher_t* cb_dispatcher) override {
    RegisterService(
        psm, params,
        [this, cb = std::move(socket_callback), cb_dispatcher](auto channel) mutable {
          zx::socket s = l2cap_socket_factory_->MakeSocketForChannel(channel);
          auto chan_info = channel ? std::optional(channel->info()) : std::nullopt;
          l2cap::ChannelSocket chan_sock(std::move(s), chan_info);
          // Called every time the service is connected, cb must be shared.
          async::PostTask(cb_dispatcher, [sock = std::move(chan_sock), cb = cb.share(),
                                          handle = channel->link_handle()]() mutable {
            cb(std::move(sock), handle);
          });
        },
        dispatcher());
  }

  void UnregisterService(l2cap::PSM psm) override {
    PostMessage([this, psm] {
      if (l2cap_) {
        l2cap_->UnregisterService(psm);
      }
    });
  }

 private:
  void InitializeL2CAP() {
    AssertOnDispatcherThread();
    ZX_ASSERT(hci_->acl_data_channel());
    const auto& acl_buffer_info = hci_->acl_data_channel()->GetBufferInfo();
    const auto& le_buffer_info = hci_->acl_data_channel()->GetLEBufferInfo();

    // LE Buffer Info is always available from ACLDataChannel.
    ZX_ASSERT(acl_buffer_info.IsAvailable());
    auto send_packets =
        fit::bind_member(hci_->acl_data_channel(), &hci::ACLDataChannel::SendPackets);
    auto drop_queued_acl =
        fit::bind_member(hci_->acl_data_channel(), &hci::ACLDataChannel::DropQueuedPackets);

    l2cap_ = std::make_unique<l2cap::ChannelManager>(
        acl_buffer_info.max_data_length(), le_buffer_info.max_data_length(),
        std::move(send_packets), std::move(drop_queued_acl), dispatcher());
    hci_->acl_data_channel()->SetDataRxHandler(l2cap_->MakeInboundDataHandler(), dispatcher());
  }

  // All members below must be accessed on the data domain thread.

  // Inspect hierarchy node representing the data domain.
  inspect::Node node_;

  // Handle to the underlying HCI transport.
  fxl::RefPtr<hci::Transport> hci_;

  std::unique_ptr<l2cap::ChannelManager> l2cap_;

  // Creates sockets that bridge internal L2CAP channels to profile processes.
  std::unique_ptr<internal::SocketFactory<l2cap::Channel>> l2cap_socket_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Impl);
};

// static
fbl::RefPtr<Domain> Domain::Create(fxl::RefPtr<hci::Transport> hci, inspect::Node node,
                                   async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(hci);
  ZX_DEBUG_ASSERT(dispatcher);
  return AdoptRef(new Impl(hci, std::move(node), dispatcher));
}

}  // namespace data
}  // namespace bt
