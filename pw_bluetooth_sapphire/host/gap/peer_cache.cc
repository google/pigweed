// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peer_cache.h"

#include <fbl/function.h>
#include <zircon/assert.h>

#include "lib/async/default.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"

namespace bt {
namespace gap {

using common::DeviceAddress;

namespace {

// Return an address with the same value as given, but with type kBREDR for
// kLEPublic addresses and vice versa.
DeviceAddress GetAliasAddress(const DeviceAddress& address) {
  if (address.type() == DeviceAddress::Type::kBREDR) {
    return {DeviceAddress::Type::kLEPublic, address.value()};
  } else if (address.type() == DeviceAddress::Type::kLEPublic) {
    return {DeviceAddress::Type::kBREDR, address.value()};
  }
  return address;
}

}  // namespace

Peer* PeerCache::NewPeer(const DeviceAddress& address, bool connectable) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  auto* const peer =
      InsertPeerRecord(common::RandomPeerId(), address, connectable);
  if (peer) {
    UpdateExpiry(*peer);
    NotifyPeerUpdated(*peer);
  }
  return peer;
}

void PeerCache::ForEach(PeerCallback f) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(f);
  for (const auto& iter : peers_) {
    f(*iter.second.peer());
  }
}

bool PeerCache::AddBondedPeer(PeerId identifier, const DeviceAddress& address,
                              const sm::PairingData& bond_data,
                              const std::optional<sm::LTK>& link_key) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(!bond_data.identity_address ||
                  address.value() == bond_data.identity_address->value());
  ZX_DEBUG_ASSERT(address.type() != DeviceAddress::Type::kLEAnonymous);

  const bool bond_le = bond_data.ltk || bond_data.csrk;
  const bool bond_bredr = link_key.has_value();

  // |bond_data| must contain either a LTK or CSRK for LE Security Mode 1 or 2.
  if (address.IsLowEnergy() && !bond_le) {
    bt_log(ERROR, "gap-le", "mandatory keys missing: no LTK or CSRK (id: %s)",
           bt_str(identifier));
    return false;
  }

  if (address.IsBrEdr() && !bond_bredr) {
    bt_log(ERROR, "gap-bredr", "mandatory link key missing (id: %s)",
           bt_str(identifier));
    return false;
  }

  auto* peer = InsertPeerRecord(identifier, address, true);
  if (!peer) {
    return false;
  }

  // A bonded peer must have its identity known.
  peer->set_identity_known(true);

  if (bond_le) {
    peer->MutLe().SetBondData(bond_data);
    ZX_DEBUG_ASSERT(peer->le()->bonded());

    // Add the peer to the resolving list if it has an IRK.
    if (bond_data.irk) {
      le_resolving_list_.Add(peer->address(), bond_data.irk->value());
    }
  }

  if (bond_bredr) {
    peer->MutBrEdr().SetBondData(*link_key);
    ZX_DEBUG_ASSERT(peer->bredr()->bonded());
  }

  if (peer->technology() == TechnologyType::kDualMode) {
    address_map_[GetAliasAddress(address)] = identifier;
  }

  ZX_DEBUG_ASSERT(!peer->temporary());
  ZX_DEBUG_ASSERT(peer->bonded());
  bt_log(SPEW, "gap", "restored bonded peer: %s, id: %s", bt_str(address),
         bt_str(identifier));

  // Don't call UpdateExpiry(). Since a bonded peer starts out as
  // non-temporary it is not necessary to ever set up the expiration callback.
  NotifyPeerUpdated(*peer);
  return true;
}

bool PeerCache::StoreLowEnergyBond(PeerId identifier,
                                   const sm::PairingData& bond_data) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  auto* peer = FindById(identifier);
  if (!peer) {
    bt_log(TRACE, "gap-le", "failed to store bond for unknown peer: %s",
           bt_str(identifier));
    return false;
  }

  // Either a LTK or CSRK is mandatory for bonding (the former is needed for LE
  // Security Mode 1 and the latter is needed for Mode 2).
  if (!bond_data.ltk && !bond_data.csrk) {
    bt_log(TRACE, "gap-le", "mandatory keys missing: no IRK or CSRK (id: %s)",
           bt_str(identifier));
    return false;
  }

  if (bond_data.identity_address) {
    auto existing_id = FindIdByAddress(*bond_data.identity_address);
    if (!existing_id) {
      // Map the new address to |peer|. We leave old addresses that map to
      // this peer in the cache in case there are any pending controller
      // procedures that expect them.
      // TODO(armansito): Maybe expire the old address after a while?
      address_map_[*bond_data.identity_address] = identifier;
    } else if (*existing_id != identifier) {
      bt_log(TRACE, "gap-le",
             "identity address %s for peer %s belongs to another peer %s!",
             bt_str(*bond_data.identity_address), bt_str(identifier),
             bt_str(*existing_id));
      return false;
    }
    // We have either created a new mapping or the identity address already
    // maps to this peer.
  }

  // TODO(BT-619): Check that we're not downgrading the security level before
  // overwriting the bond.
  peer->MutLe().SetBondData(bond_data);
  ZX_DEBUG_ASSERT(!peer->temporary());
  ZX_DEBUG_ASSERT(peer->le()->bonded());

  // Add the peer to the resolving list if it has an IRK.
  if (peer->identity_known() && bond_data.irk) {
    le_resolving_list_.Add(peer->address(), bond_data.irk->value());
  }

  // Report the bond for persisting only if the identity of the peer is known.
  if (peer->identity_known()) {
    NotifyPeerBonded(*peer);
  }
  return true;
}

bool PeerCache::StoreBrEdrBond(const common::DeviceAddress& address,
                               const sm::LTK& link_key) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(address.type() == common::DeviceAddress::Type::kBREDR);
  auto* peer = FindByAddress(address);
  if (!peer) {
    bt_log(TRACE, "gap-bredr", "failed to store bond for unknown peer: %s",
           address.ToString().c_str());
    return false;
  }

  // TODO(BT-619): Check that we're not downgrading the security level before
  // overwriting the bond.
  peer->MutBrEdr().SetBondData(link_key);
  ZX_DEBUG_ASSERT(!peer->temporary());
  ZX_DEBUG_ASSERT(peer->bredr()->bonded());

  NotifyPeerBonded(*peer);
  return true;
}

bool PeerCache::ForgetPeer(PeerId peer_id) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  auto* const peer = FindById(peer_id);
  if (!peer) {
    bt_log(TRACE, "gap", "failed to unbond unknown peer %s", bt_str(peer_id));
    return false;
  }

  bool bond_removed = false;
  if (peer->bredr() && peer->bredr()->bonded()) {
    peer->MutBrEdr().ClearBondData();
    ZX_ASSERT(!peer->bredr()->bonded());
    bond_removed = true;
  }

  if (peer->le() && peer->le()->bonded()) {
    if (auto& address = peer->le()->bond_data()->identity_address) {
      le_resolving_list_.Remove(*address);
    }
    peer->MutLe().ClearBondData();
    ZX_ASSERT(!peer->le()->bonded());
    bond_removed = true;
  }

  // Make peer temporary but don't set up expiry task unless the peer is
  // already disconnected.
  // TODO(BT-824): Mark the peer as "delete after disconnection."
  peer->temporary_ = true;
  if (!peer->connected()) {
    // TODO(BT-824): Remove the peer.
    UpdateExpiry(*peer);
  }

  if (bond_removed) {
    NotifyPeerUpdated(*peer);
  }

  return bond_removed;
}

Peer* PeerCache::FindById(PeerId peer_id) const {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  auto iter = peers_.find(peer_id);
  return iter != peers_.end() ? iter->second.peer() : nullptr;
}

Peer* PeerCache::FindByAddress(const DeviceAddress& in_address) const {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  std::optional<DeviceAddress> address;
  if (in_address.IsResolvablePrivate()) {
    address = le_resolving_list_.Resolve(in_address);
  }

  // Fall back to the input if an identity wasn't resolved.
  if (!address) {
    address = in_address;
  }

  ZX_DEBUG_ASSERT(address);
  auto identifier = FindIdByAddress(*address);
  if (!identifier) {
    return nullptr;
  }

  auto* p = FindById(*identifier);
  ZX_DEBUG_ASSERT(p);
  return p;
}

// Private methods below.

Peer* PeerCache::InsertPeerRecord(PeerId identifier,
                                  const common::DeviceAddress& address,
                                  bool connectable) {
  if (FindIdByAddress(address)) {
    bt_log(WARN, "gap", "tried to insert peer with existing address: %s",
           address.ToString().c_str());
    return nullptr;
  }

  std::unique_ptr<Peer> peer(
      new Peer(fit::bind_member(this, &PeerCache::NotifyPeerUpdated),
               fit::bind_member(this, &PeerCache::UpdateExpiry),
               fit::bind_member(this, &PeerCache::MakeDualMode), identifier,
               address, connectable));
  // Note: we must construct the PeerRecord in-place, because it doesn't
  // support copy or move.
  auto [iter, inserted] =
      peers_.try_emplace(peer->identifier(), std::move(peer),
                         [this, p = peer.get()] { RemovePeer(p); });
  if (!inserted) {
    bt_log(WARN, "gap", "tried to insert peer with existing ID: %s",
           bt_str(identifier));
    return nullptr;
  }

  address_map_[address] = identifier;
  return iter->second.peer();
}

void PeerCache::NotifyPeerBonded(const Peer& peer) {
  ZX_DEBUG_ASSERT(peers_.find(peer.identifier()) != peers_.end());
  ZX_DEBUG_ASSERT(peers_.at(peer.identifier()).peer() == &peer);
  ZX_DEBUG_ASSERT_MSG(peer.identity_known(),
                      "peers not allowed to bond with unknown identity!");

  bt_log(INFO, "gap", "peer bonded %s", peer.ToString().c_str());
  if (peer_bonded_callback_) {
    peer_bonded_callback_(peer);
  }
}

void PeerCache::NotifyPeerUpdated(const Peer& peer) {
  ZX_DEBUG_ASSERT(peers_.find(peer.identifier()) != peers_.end());
  ZX_DEBUG_ASSERT(peers_.at(peer.identifier()).peer() == &peer);
  if (peer_updated_callback_)
    peer_updated_callback_(peer);
}

void PeerCache::UpdateExpiry(const Peer& peer) {
  auto peer_record_iter = peers_.find(peer.identifier());
  ZX_DEBUG_ASSERT(peer_record_iter != peers_.end());

  auto& peer_record = peer_record_iter->second;
  ZX_DEBUG_ASSERT(peer_record.peer() == &peer);

  const auto cancel_res = peer_record.removal_task()->Cancel();
  ZX_DEBUG_ASSERT(cancel_res == ZX_OK || cancel_res == ZX_ERR_NOT_FOUND);

  // Previous expiry task has been canceled. Re-schedule only if the peer is
  // temporary.
  if (peer.temporary()) {
    const auto schedule_res = peer_record.removal_task()->PostDelayed(
        async_get_default_dispatcher(), kCacheTimeout);
    ZX_DEBUG_ASSERT(schedule_res == ZX_OK || schedule_res == ZX_ERR_BAD_STATE);
  }
}

void PeerCache::MakeDualMode(const Peer& peer) {
  ZX_DEBUG_ASSERT(address_map_.at(peer.address()) == peer.identifier());
  const auto address_alias = GetAliasAddress(peer.address());
  auto [iter, inserted] =
      address_map_.try_emplace(address_alias, peer.identifier());
  ZX_DEBUG_ASSERT_MSG(inserted || iter->second == peer.identifier(),
                      "%s can't become dual-mode because %s maps to %s",
                      bt_str(peer.identifier()), bt_str(address_alias),
                      bt_str(iter->second));

  // The peer became dual mode in lieu of adding a new peer but is as
  // significant, so notify listeners of the change.
  NotifyPeerUpdated(peer);
}

void PeerCache::RemovePeer(Peer* peer) {
  ZX_DEBUG_ASSERT(peer);

  auto peer_record_it = peers_.find(peer->identifier());
  ZX_DEBUG_ASSERT(peer_record_it != peers_.end());
  ZX_DEBUG_ASSERT(peer_record_it->second.peer() == peer);

  PeerId id = peer->identifier();
  bt_log(SPEW, "gap", "removing peer %s", bt_str(id));
  for (auto iter = address_map_.begin(); iter != address_map_.end();) {
    if (iter->second == id) {
      iter = address_map_.erase(iter);
    } else {
      iter++;
    }
  }
  peers_.erase(peer_record_it);  // Destroys |peer|.
  if (peer_removed_callback_) {
    peer_removed_callback_(id);
  }
}

std::optional<PeerId> PeerCache::FindIdByAddress(
    const common::DeviceAddress& address) const {
  auto iter = address_map_.find(address);
  if (iter == address_map_.end()) {
    // Search again using the other technology's address. This is necessary when
    // a dual-mode peer is known by only one technology and is then discovered
    // or connected on its other technology.
    iter = address_map_.find(GetAliasAddress(address));
  }

  if (iter == address_map_.end()) {
    return {};
  }
  return {iter->second};
}

}  // namespace gap
}  // namespace bt
