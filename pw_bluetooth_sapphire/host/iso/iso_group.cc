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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_group.h"

#include <pw_assert/check.h>

#include "pw_bluetooth/hci_commands.emb.h"

namespace bt::iso {
namespace {

class IsoGroupImpl : public IsoGroup {
 public:
  IsoGroupImpl(hci_spec::CigIdentifier id,
               hci::Transport::WeakPtr hci,
               CigStreamCreator::WeakPtr cig_stream_creator,
               OnClosedCallback on_closed_callback)
      : IsoGroup(id,
                 std::move(hci),
                 std::move(cig_stream_creator),
                 std::move(on_closed_callback)),
        weak_self_(this) {}

 private:
  // IsoGroup overrides.
  WeakPtr GetWeakPtr() override { return weak_self_.GetWeakPtr(); }

  // Keep last, must be destroyed before any other member.
  WeakSelf<IsoGroupImpl> weak_self_;
};

}  // namespace

std::unique_ptr<IsoGroup> IsoGroup::CreateCig(
    hci_spec::CigIdentifier id,
    hci::Transport::WeakPtr hci,
    CigStreamCreator::WeakPtr cig_stream_creator,
    IsoGroup::OnClosedCallback on_closed_callback) {
  return std::make_unique<IsoGroupImpl>(id,
                                        std::move(hci),
                                        std::move(cig_stream_creator),
                                        std::move(on_closed_callback));
}

}  // namespace bt::iso
