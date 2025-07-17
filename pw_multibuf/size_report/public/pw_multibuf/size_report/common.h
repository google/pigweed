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

#include <string_view>

namespace pw::multibuf::size_report {

// In some examples, the following fixed addresses are used .
inline constexpr uint16_t kDemoLinkSender = 0x0001;
inline constexpr uint16_t kDemoLinkReceiver = 0x0002;
inline constexpr uint64_t kDemoNetworkSender = 0xAAAAAAAAAAAAAAAULL;
inline constexpr uint64_t kDemoNetworkReceiver = 0xBBBBBBBBBBBBBBBULL;

// In some examples, the protocols are used to send sections 1.10.32 and
// 1.10.33 of Cicero's "De finibus bonorum et malorum".
inline constexpr std::string_view kLoremIpsum =
    "Sed ut perspiciatis, unde omnis iste natus error sit voluptatem "
    "accusantium doloremque laudantium, totam rem aperiam eaque ipsa, quae ab "
    "illo inventore veritatis et quasi architecto beatae vitae dicta sunt, "
    "explicabo. Nemo enim ipsam voluptatem, quia voluptas sit, aspernatur aut "
    "odit aut fugit, sed quia consequuntur magni dolores eos, qui ratione "
    "voluptatem sequi nesciunt, neque porro quisquam est, qui dolorem ipsum, "
    "quia dolor sit amet consectetur adipisci velit, sed quia non numquam  "
    "eius modi tempora incidunt, ut labore et dolore magnam aliquam quaerat "
    "voluptatem. Ut enim ad minima veniam, quis nostrum exercitationem ullam "
    "corporis suscipit laboriosam, nisi ut aliquid ex ea commodi consequatur? "
    "Quis autem vel eum iure reprehenderit, qui in ea voluptate velit esse, "
    "quam nihil molestiae consequatur, vel illum, qui dolorem eum fugiat, quo "
    "voluptas nulla pariatur?"
    "At vero eos et accusamus et iusto odio dignissimos ducimus, qui "
    "blanditiis praesentium voluptatum deleniti atque corrupti, quos dolores "
    "et quas molestias excepturi sint, obcaecati cupiditate non provident, "
    "similique sunt in culpa, qui officia deserunt mollitia animi, id est "
    "laborum et dolorum fuga. Et harum quidem reruum facilis est et expedita "
    "distinctio. Nam libero tempore, cum soluta nobis est eligendi optio, "
    "cumque nihil impedit, quo minus id, quod maxime placeat facere possimus, "
    "omnis voluptas assumenda est, omnis dolor repellendus. Temporibus autem "
    "quibusdam et aut officiis debitis aut rerum necessitatibus saepe eveniet, "
    "ut et voluptates repudiandae sint et molestiae non recusandae. Itaque "
    "earum rerum hic tenetur a sapiente delectus, ut aut reiciendis "
    "voluptatibus maiores alias consequatur aut perferendis doloribus "
    "asperiores repellat.";

}  // namespace pw::multibuf::size_report
