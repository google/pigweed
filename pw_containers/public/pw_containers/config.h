// Copyright 2024 The Pigweed Authors
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

// See b/362348318 for background and details.
//
// The singly-linked `pw::IntrusiveList` will eventually migrate to
// `pw::IntrusiveForwardList` and a doubly-linked `pw::IntrusiveList` will take
// its place.
//
// As part of this migration, usage of `pw::IntrusiveList` may be marked as
// deprecated. This configuration option suppresses the deprecation warning.
//
// At some point in the near future, this will default to 0. Downstream projects
// may still suppress the warning by overriding this configuration, but must be
// aware that the legacy alias will eventually be removed.
#ifndef PW_CONTAINERS_INTRUSIVE_LIST_SUPPRESS_DEPRECATION_WARNING
#define PW_CONTAINERS_INTRUSIVE_LIST_SUPPRESS_DEPRECATION_WARNING 1
#endif  // PW_CONTAINERS_INTRUSIVE_LIST_SUPPRESS_DEPRECATION_WARNING

// See b/362348318 for background and details.
//
// The singly-linked `pw::IntrusiveList` will eventually migrate to
// `pw::IntrusiveForwardList` and a doubly-linked `pw::IntrusiveList` will take
// its place.
//
// As part of this migration, the type alias for `pw::IntrusiveList` may be
// omitted. This configuration option provides the legacy type alias.
//
// At some point in the near future, this will default to 0. Downstream projects
// may still use the legacy alias by overriding this configuration, but must be
// aware that the legacy alias will eventually be removed.
#ifndef PW_CONTAINERS_USE_LEGACY_INTRUSIVE_LIST
#define PW_CONTAINERS_USE_LEGACY_INTRUSIVE_LIST 1
#endif  // PW_CONTAINERS_USE_LEGACY_INTRUSIVE_LIST
