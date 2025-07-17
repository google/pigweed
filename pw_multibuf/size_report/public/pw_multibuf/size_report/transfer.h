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
#pragma once

#include <algorithm>

#include "pw_async2/dispatcher.h"
#include "pw_multibuf/size_report/common.h"
#include "pw_multibuf/size_report/receiver.h"
#include "pw_multibuf/size_report/sender.h"

namespace pw::multibuf::size_report {

template <typename MultiBufType>
void TransferMessage(Sender<MultiBufType>& sender,
                     Receiver<MultiBufType>& receiver) {
  async2::Dispatcher dispatcher;
  dispatcher.Post(sender);
  dispatcher.Post(receiver);
  sender.Send(kLoremIpsum.data());
  dispatcher.RunToCompletion();
  auto received = receiver.TakeReceived();
  PW_ASSERT(received.has_value());
  auto& mb = internal::GetMultiBuf(*received);
  ConstByteSpan expected = as_bytes(span(kLoremIpsum));
  PW_ASSERT(std::equal(mb.begin(), mb.end(), expected.begin(), expected.end()));
}

}  // namespace pw::multibuf::size_report
