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

#include "pw_thread/thread_iteration.h"

#include <zephyr/kernel.h>

namespace pw::thread {

namespace zephyr {
void zephyr_adapter(const struct k_thread* thread, void* user_data) {
  const pw::thread::ThreadCallback& cb =
      *reinterpret_cast<const pw::thread::ThreadCallback*>(user_data);

  ThreadInfo thread_info;

  span<const std::byte> current_name =
      as_bytes(span(std::string_view(k_thread_name_get((k_tid_t)thread))));
  thread_info.set_thread_name(current_name);

#ifdef CONFIG_THREAD_STACK_INFO
  thread_info.set_stack_low_addr(thread->stack_info.start);
  thread_info.set_stack_pointer(thread->stack_info.start +
                                thread->stack_info.size);
#endif

  cb(thread_info);
}
}  // namespace zephyr

Status ForEachThread(const pw::thread::ThreadCallback& cb) {
  k_thread_user_cb_t adapter_cb = zephyr::zephyr_adapter;

  k_thread_foreach(adapter_cb, (void*)&cb);

  return OkStatus();
}

}  // namespace pw::thread
