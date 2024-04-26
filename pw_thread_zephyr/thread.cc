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
#include "pw_thread/thread.h"

#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>

#include "pw_assert/check.h"
#include "pw_preprocessor/compiler.h"
#include "pw_thread/id.h"
#include "pw_thread_zephyr/config.h"
#include "pw_thread_zephyr/context.h"
#include "pw_thread_zephyr/options.h"

using pw::thread::zephyr::Context;

namespace pw::thread {
namespace {

k_spinlock global_thread_done_lock;

}  // namespace

void Context::ThreadEntryPoint(void* void_context_ptr, void*, void*) {
  Context& context = *static_cast<Context*>(void_context_ptr);

  // Invoke the user's thread function. This may never return.
  context.fn_();
  context.fn_ = nullptr;

  k_spinlock_key_t key = k_spin_lock(&global_thread_done_lock);
  if (context.detached()) {
    context.set_task_handle(nullptr);
  } else {
    // Defer cleanup to Thread's join() or detach().
    context.set_thread_done();
  }
  k_spin_unlock(&global_thread_done_lock, key);
}

void Context::CreateThread(const zephyr::Options& options,
                           DeprecatedOrNewThreadFn&& thread_fn,
                           Context*& native_type_out) {
  PW_CHECK(options.static_context() != nullptr);

  // Use the statically allocated context.
  native_type_out = options.static_context();
  // Can't use a context more than once.
  PW_DCHECK_PTR_EQ(native_type_out->task_handle(), nullptr);
  // Reset the state of the static context in case it was re-used.
  native_type_out->set_detached(false);
  native_type_out->set_thread_done(false);

  native_type_out->set_thread_routine(std::move(thread_fn));
  native_type_out->const k_tid_t task_handle =
      k_thread_create(&native_type_out->thread_info(),
                      options.static_context()->stack(),
                      options.static_context()->available_stack_size(),
                      Context::ThreadEntryPoint,
                      options.static_context(),
                      nullptr,
                      nullptr,
                      options.priority(),
                      options.native_options(),
                      K_NO_WAIT);
  PW_CHECK_NOTNULL(task_handle);  // Ensure it succeeded.
  native_type_out->set_task_handle(task_handle);
}

Thread::Thread(const thread::Options& facade_options, Function<void()>&& entry)
    : native_type_(nullptr) {
  // Cast the generic facade options to the backend specific option of which
  // only one type can exist at compile time.
  auto options = static_cast<const zephyr::Options&>(facade_options);
  Context::CreateThread(options, std::move(entry), native_type_);
}

Thread::Thread(const thread::Options& facade_options,
               ThreadRoutine entry,
               void* arg)
    : native_type_(nullptr) {
  auto options = static_cast<const zephyr::Options&>(facade_options);
  Context::CreateThread(
      options, DeprecatedFnPtrAndArg{entry, arg}, native_type_);
}

void Thread::detach() {
  PW_CHECK(joinable());

  k_spinlock_key_t key = k_spin_lock(&global_thread_done_lock);
  native_type_->set_detached();
  const bool thread_done = native_type_->thread_done();

  if (thread_done) {
    // The task finished (hit end of Context::ThreadEntryPoint) before we
    // invoked detach, clean up the task handle to allow the Context reuse.
    native_type_->set_task_handle(nullptr);
  } else {
    // We're detaching before the task finished, defer cleanup to the task at
    // the end of Context::ThreadEntryPoint.
  }

  k_spin_unlock(&global_thread_done_lock, key);

  // Update to no longer represent a thread of execution.
  native_type_ = nullptr;
}

void Thread::join() {
  PW_CHECK(joinable());
  PW_CHECK(this_thread::get_id() != get_id());

  PW_CHECK_INT_EQ(0, k_thread_join(native_type_->task_handle_, K_FOREVER));

  native_type_->set_task_handle(nullptr);

  // Update to no longer represent a thread of execution.
  native_type_ = nullptr;
}

}  // namespace pw::thread
