// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PEER_CACHE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PEER_CACHE_H_

#include <fbl/function.h>
#include <fbl/macros.h>
#include <lib/async/cpp/task.h>

#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/identity_resolving_list.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {

class ByteBuffer;

namespace hci {
struct LowEnergyScanResult;
}  // namespace hci

namespace gap {

// A PeerCache provides access to remote Bluetooth devices that are
// known to the system.
class PeerCache final {
 public:
  using PeerCallback = fit::function<void(const Peer& peer)>;
  using PeerIdCallback = fit::function<void(PeerId identifier)>;

  PeerCache() = default;

  // Creates a new peer entry using the given parameters, and returns a
  // (non-owning) pointer to that peer. The caller must not retain the pointer
  // beyond the current dispatcher task, as the underlying Peer is owned
  // by |this| PeerCache, and may be invalidated spontaneously.
  //
  // Returns nullptr if an entry matching |address| already exists in the cache,
  // including as a public identity of a peer with a different technology.
  Peer* NewPeer(const DeviceAddress& address, bool connectable);

  // Iterates over all current peers in the map, running |f| on each entry
  // synchronously. This is intended for IPC methods that request a list of
  // peers.
  //
  // Clients should use the FindBy*() methods below to interact with
  // Peer objects.
  void ForEach(PeerCallback f);

  // Creates a new non-temporary peer entry using the given |identifier| and
  // identity |address|. This is intended to initialize this PeerCache
  // with previously bonded peers while bootstrapping a bt-host peer. The
  // "peer bonded" callback will not be invoked.
  //
  // This method is not intended for updating the bonding data of a peer that
  // already exists the cache and returns false if a mapping for |identifier| or
  // |address| is already present. Use Store*Bond() methods to update pairing
  // information of an existing peer.
  //
  // If a peer already exists that has the same public identity address with a
  // different technology, this method will return false. The existing peer
  // should be instead updated with new bond information to create a dual-mode
  // peer.
  //
  bool AddBondedPeer(PeerId identifier, const DeviceAddress& address,
                     const sm::PairingData& bond_data,
                     const std::optional<sm::LTK>& link_key);

  // Update the peer with the given identifier with new LE bonding
  // information. The peer will be considered "bonded" and the bonded callback
  // will be notified. If the peer is already bonded then bonding data will be
  // updated.
  //
  // If |bond_data| contains an |identity_address|, the peer cache will be
  // updated with a new mapping from that address to this peer identifier. If
  // the identity address already maps to an existing peer, this method will
  // return false. TODO(armansito): Merge the peers instead of failing? What
  // happens if we obtain a LE identity address from a dual-mode peer that
  // matches the BD_ADDR previously obtained from it over BR/EDR?
  bool StoreLowEnergyBond(PeerId identifier, const sm::PairingData& bond_data);

  // Update a peer identified by BD_ADDR |address| with a new BR/EDR link key.
  // The peer will be considered "bonded" and the bonded callback notified. If
  // the peer is already bonded then the link key will be updated. Returns
  // false if the address does not match that of a known peer.
  bool StoreBrEdrBond(const DeviceAddress& address, const sm::LTK& link_key);

  // Resets a peer |peer_id| to an unbonded state by removing secrets for its
  // transports. The peer will become temporary and may expire. This does not
  // otherwise disconnect the peer or remove its ID or address from the cache.
  // Returns true if bonds were deleted from a known peer.
  //
  // TODO(BT-824): Delete the peer immediately after disconnecting. Will return
  // true if a known peer was deleted or marked for deletion.
  bool ForgetPeer(PeerId peer_id);

  // Returns the remote peer with identifier |peer_id|. Returns nullptr if
  // |peer_id| is not recognized.
  Peer* FindById(PeerId peer_id) const;

  // Finds and returns a Peer with address |address| if it exists,
  // returns nullptr otherwise. Tries to resolve |address| if it is resolvable.
  // If |address| is of type kBREDR or kLEPublic, then this searches for peers
  // that have either type of address.
  Peer* FindByAddress(const DeviceAddress& address) const;

  // When set, |callback| will be invoked whenever a peer is added or updated.
  void set_peer_updated_callback(PeerCallback callback) {
    ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
    peer_updated_callback_ = std::move(callback);
  }

  // When set, |callback| will be invoked whenever a peer is removed.
  void set_peer_removed_callback(PeerIdCallback callback) {
    ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
    peer_removed_callback_ = std::move(callback);
  }

  // When this callback is set, |callback| will be invoked whenever the bonding
  // data of a peer is updated and should be persisted. The caller must ensure
  // that |callback| outlives |this|.
  void set_peer_bonded_callback(PeerCallback callback) {
    ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
    peer_bonded_callback_ = std::move(callback);
  }

  // Returns the number of peers that are currently in the peer cache.
  size_t count() const { return peers_.size(); }

 private:
  class PeerRecord final {
   public:
    PeerRecord(std::unique_ptr<Peer> peer, fbl::Closure remove_peer_callback)
        : peer_(std::move(peer)),
          removal_task_(std::move(remove_peer_callback)) {}

    // The copy and move ctors cannot be implicitly defined, since
    // async::TaskClosure does not support those operations. Nor is any
    // meaningful explicit definition possible.
    PeerRecord(const PeerRecord&) = delete;
    PeerRecord(PeerRecord&&) = delete;

    Peer* peer() const { return peer_.get(); }

    // Returns a pointer to removal_task_, which can be used to (re-)schedule or
    // cancel |remove_peer_callback|.
    async::TaskClosure* removal_task() { return &removal_task_; }

   private:
    std::unique_ptr<Peer> peer_;
    async::TaskClosure removal_task_;
  };

  // Create and track a record of a remote peer with a given |identifier|,
  // |address|, and connectability (|connectable|). Returns a pointer to the
  // inserted peer or nullptr if |identifier| or |address| already exists in
  // the cache.
  Peer* InsertPeerRecord(PeerId identifier, const DeviceAddress& address,
                         bool connectable);

  // Notifies interested parties that |peer| has bonded
  // |peer| must already exist in the cache.
  void NotifyPeerBonded(const Peer& peer);

  // Notifies interested parties that |peer| has seen a significant change.
  // |peer| must already exist in the cache.
  void NotifyPeerUpdated(const Peer& peer);

  // Updates the expiration time for |peer|, if a temporary. Cancels expiry,
  // if a non-temporary. Pre-conditions:
  // - |peer| must already exist in the cache
  // - can only be called from the thread that created |peer|
  void UpdateExpiry(const Peer& peer);

  // Updates the cache when an existing peer is found to be dual-mode. Also
  // notifies listeners of the "peer updated" callback.
  // |peer| must already exist in the cache.
  void MakeDualMode(const Peer& peer);

  // Removes |peer| from this cache, and notifies listeners of the removal.
  void RemovePeer(Peer* peer);

  // Search for an unique peer ID by its peer address |address|, by both
  // technologies if it is a public address. |address| should be already
  // resolved, if it is resolvable. If found, returns a valid peer ID;
  // otherwise returns std::nullopt.
  std::optional<PeerId> FindIdByAddress(const DeviceAddress& address) const;

  // Mapping from unique peer IDs to PeerRecords.
  // Owns the corresponding Peers.
  std::unordered_map<PeerId, PeerRecord> peers_;

  // Mapping from peer addresses to unique peer identifiers for all known
  // peers. This is used to look-up and update existing cached data for a
  // particular scan result so as to avoid creating duplicate entries for the
  // same peer.
  //
  // Dual-mode peers shall have identity addresses of both technologies
  // mapped to the same ID, if the addresses have the same value.
  std::unordered_map<DeviceAddress, PeerId> address_map_;

  // The LE identity resolving list used to resolve RPAs.
  IdentityResolvingList le_resolving_list_;

  PeerCallback peer_updated_callback_;
  PeerIdCallback peer_removed_callback_;
  PeerCallback peer_bonded_callback_;

  fxl::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PeerCache);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PEER_CACHE_H_
