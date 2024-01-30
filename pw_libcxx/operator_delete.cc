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

#include <new>

// fxbug.dev/42082880: We should look into compiler changes to avoid emitting
// references to these symbols.
// These operator delete implementations are provided to satisfy references from
// the vtable for the deleting destructor. In practice, these will never be
// reached because users should not be using new/delete.

void operator delete(void*) noexcept { __builtin_trap(); }
void operator delete[](void*) noexcept { __builtin_trap(); }

void operator delete(void*, std::align_val_t) noexcept { __builtin_trap(); }
void operator delete[](void*, std::align_val_t) noexcept { __builtin_trap(); }

void operator delete(void*, std::size_t) noexcept { __builtin_trap(); }
void operator delete[](void*, std::size_t) noexcept { __builtin_trap(); }

void operator delete(void*, std::size_t, std::align_val_t) noexcept {
  __builtin_trap();
}
void operator delete[](void*, std::size_t, std::align_val_t) noexcept {
  __builtin_trap();
}
