// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TASK_DOMAIN_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TASK_DOMAIN_H_

#include <string>

#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include <lib/async-loop/cpp/loop.h>
#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <zircon/assert.h>

#include "lib/fxl/macros.h"

namespace btlib {
namespace common {

// A task domain is a mixin for objects that maintain state that needs to be
// accessed exclusively on a specific dispatcher.
//
//   * A TaskDomain can be initialized with a task runner representing the
//     serialization domain. If not, TaskDomain will spawn a thread with the
//     runner.
//
//   * TaskDomain provides a PostMessage() method which can be used to schedule
//     a task on the domain. The TaskDomain is guaranteed to remain alive during
//     the task execution. This guarantee requires that T derive from
//     fbl::RefCounted<T>.
//
//   * Tasks that are posted or run after clean up will be ignored. The
//     ScheduleCleanUp() method must be called before all references to T are
//     dropped.
//
//     T must provide a CleanUp() method, which will be scheduled on the
//     domain's task runner by ScheduleCleanUp(). This can be used to clean up
//     state that is restricted to the task runner.
//
// EXAMPLE:
//
//   class MyObject : public fbl::RefCounted<MyObject>,
//                    public TaskDomain<MyObject> {
//    public:
//     // Initialize by spawning a thread with a dispatcher owned by this
//     // domain.
//     MyObject() : common::TaskDomain<MyObject>(this, "my-thread") {}
//
//     // Initialize to run on |dispatcher|.
//     explicit MyObject(async_dispatcher_t* dispatcher)
//        : common::TaskDomain<MyObject>(this, dispatcher) {}
//
//     void CleanUp()
//
//    private:
//     int foo_;
//   };
//
// If MyObject indirectly inherits from fbl::RefCounted, the base type needs to
// be supplied using an additional template parameter:
//
//   class MyBase : public fbl::RefCounted<MyBase> {};
//
//   class MyObject : public MyBase, public TaskDomain<MyObject, MyBase> {
//    public:
//     ...
//   };
//
// If CleanUp() should be private:
//
//   class MyObject : public fbl::RefCounted<MyObject>,
//                    public TaskDomain<MyObject> {
//    public:
//     ...
//    private:
//     BT_FRIEND_TASK_DOMAIN(MyObject);
//
//     void CleanUp() { ... }
//   };
//

namespace internal {
DECLARE_HAS_MEMBER_FN_WITH_SIGNATURE(has_clean_up, CleanUp, void (T::*)(void));
}  // namespace internal

template <typename T, typename RefCountedType = T>
class TaskDomain {
 protected:
  // Initializes this domain by spawning a new thread with a dispatcher.
  // |name| is assigned to the thread.
  TaskDomain(T* obj, std::string name) {
    Init(obj);

    loop_ = std::make_unique<async::Loop>(&kAsyncLoopConfigNoAttachToThread);
    loop_->StartThread(name.c_str());
    dispatcher_ = loop_->dispatcher();
  }

  // Initializes this domain by assigning the given |dispatcher| to it.
  TaskDomain(T* obj, async_dispatcher_t* dispatcher) : dispatcher_(dispatcher) {
    Init(obj);

    ZX_DEBUG_ASSERT(dispatcher_);
  }

  virtual ~TaskDomain() {
    ZX_DEBUG_ASSERT_MSG(!alive_,
                        "ScheduleCleanUp() must be called before destruction");
  }

  // Runs the object's CleanUp() handler on the domain's dispatcher. Quits the
  // event loop if the domain owns its thread.
  void ScheduleCleanUp() {
    PostMessage([obj = obj_, this] {
      alive_ = false;
      obj->CleanUp();

      // Quit the thread if it's owned by this object.
      if (loop_) {
        loop_->Quit();
      }
    });

    // Block until the clean up task has run.
    if (loop_) {
      loop_->JoinThreads();
    }
  }

  async_dispatcher_t* dispatcher() const { return dispatcher_; }

  void PostMessage(fit::closure func) {
    // |objref| is captured here to make sure |obj_| stays alive until |func|
    // has run.
    async::PostTask(dispatcher_, [this, func = std::move(func),
                                  objref = fbl::WrapRefPtr(obj_)] {
      if (alive_) {
        func();
      }
    });
  }

  // Asserts that this domain's dispatcher is assigned as the current thread's
  // default. This is purely intended for debug assertions and should not be
  // used for any other purpose.
  inline void AssertOnDispatcherThread() const {
    ZX_DEBUG_ASSERT(async_get_default_dispatcher() == dispatcher());
  }

  // Returns true if this domain is still alive. This function is only safe to
  // call on the domain thread.
  bool alive() const {
    AssertOnDispatcherThread();
    return alive_;
  }

 private:
  void Init(T* obj) {
    ZX_DEBUG_ASSERT(obj);
    obj_ = obj;
    alive_ = true;

    static_assert(std::is_base_of<fbl::RefCounted<RefCountedType>, T>::value,
                  "T must support fbl::RefPtr");
    static_assert(std::is_base_of<TaskDomain<T, RefCountedType>, T>::value,
                  "TaskDomain can only be used as a mixin");
    static_assert(internal::has_clean_up<T>::value,
                  "T must provide a CleanUp() method");
  }

  T* obj_;
  std::atomic_bool alive_;

  // |loop_| is null if this TaskDomain gets initialized with an existing
  // dispatcher. Otherwise it will be valid and |dispatcher_| will point to
  // |loop_|'s dispatcher. |dispatcher_| is never null.
  async_dispatcher_t* dispatcher_;
  std::unique_ptr<async::Loop> loop_;

  FXL_DISALLOW_COPY_AND_ASSIGN(TaskDomain);
};

#define BT_FRIEND_TASK_DOMAIN(Type) BT_FRIEND_TASK_DOMAIN_FULL(Type, Type)
#define BT_FRIEND_TASK_DOMAIN_FULL(Type, RefCountedType) \
    friend class ::btlib::common::TaskDomain<Type, RefCountedType>; \
    friend struct ::btlib::common::internal::has_clean_up<Type>

}  // namespace common
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TASK_DOMAIN_H_
