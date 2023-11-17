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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_RECOMBINER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_RECOMBINER_H_

#include <endian.h>

#include <cstdint>

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/pdu.h"
#include "pw_bluetooth_sapphire/internal/host/transport/acl_data_packet.h"

namespace bt::l2cap {

// A Recombiner can be used to obtain complete L2CAP frames from received
// fragments. Incoming ACL data packets can be accumulated in a Recombiner.
//
// Each instance of Recombiner is intended to be used over a unique logical
// link. ACL data packets with different connection handles should not be added
// to the same Recombiner (the code will assert this in debug-mode).
//
// THREAD-SAFETY:
//
// This class is not thread-safe. External locking should be provided if an
// instance will be accessed on multiple threads.
class Recombiner final {
 public:
  explicit Recombiner(hci_spec::ConnectionHandle handle);

  // Consumes an ACL data fragment. This function may return a complete L2CAP
  // PDU if |fragment| completes a sequence or constitutes a complete fragment
  // on its own. The |frames_dropped| flag is set to true if a sequence was
  // dropped due to a recombination error. The most likely causes for an error
  // are:
  //
  //   1. |fragment| contains a malformed L2CAP frame. A packet is treated as
  //   malformed if:
  //      a. Its suspected to be the first fragment in a new recombination
  //      sequence and does not contain a complete L2CAP basic header. b. After
  //      a recombination sequence is considered complete, the length of the
  //      frame does not match the length that was obtained from the L2CAP basic
  //      header.
  //
  //   2. |fragment| begins a new sequence when a prior incomplete sequence was
  //   in progress, in which case the incomplete sequence is dropped but
  //   |fragment| is retained UNLESS |fragment| itself constitues a malformed
  //   PDU (as in #1);
  //
  //   3. |fragment| is a continuing fragment that leaves the sequence in
  //   progress in a malformed state, in which case the sequence and |fragment|
  //   are dropped;
  //
  // A "true" |frames_dropped| value does not imply that the supplied input
  // |fragment| itself was in error and it is possible for |frames_dropped| to
  // be true alongside a valid |pdu| value. The caller can resume calling
  // ConsumeFragment as normal, as the Recombiner can internally recover from a
  // recombination error.
  //
  // This function panics if |fragment| is not built for the connection handle
  // that this Recombiner was assigned to.
  struct Result {
    std::optional<PDU> pdu;
    bool frames_dropped;
  };
  Result ConsumeFragment(hci::ACLDataPacketPtr fragment);

 private:
  // Handles a new ACL data fragment received when a recombination is not in
  // progress. This may deliver |fragment| as is if it constitutes a complete
  // PDU, drop it if it's malformed, or initiate a new recombination if it's
  // partial.
  Result ProcessFirstFragment(hci::ACLDataPacketPtr fragment);

  // Clears the current recombination.
  void ClearRecombination();

  // Begins a trace for a new queued fragment, tracking a single new trace ID in
  // |trace_ids_|.
  void BeginTrace();

  // Ends the traces for all queued fragments. This gets called by
  // ClearRecombination() when a pending recombination ends (either successfully
  // or in error).
  void EndTraces();

  struct Recombination {
    PDU pdu;
    size_t expected_frame_length = 0u;
    size_t accumulated_length = 0u;
  };
  std::optional<Recombination> recombination_;

  // The handle for the logical link this Recombiner operates on. This field is
  // here purely to enforce that this Recombiner is used with ACL fragments from
  // the correct link.
  const hci_spec::ConnectionHandle handle_;

  // Trace flow IDs for the fragments being recombined into a single PDU.
  // Flows track from AddFragment to Release, only when there is fragmentation.
  // (PDUs are expected to be released immediately when there is no recombining)
  std::vector<trace_flow_id_t> trace_ids_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Recombiner);
};

}  // namespace bt::l2cap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_RECOMBINER_H_
