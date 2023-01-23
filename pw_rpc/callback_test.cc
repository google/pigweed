// Copyright 2022 The Pigweed Authors
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

#include "gtest/gtest.h"
#include "pw_rpc/raw/client_testing.h"
#include "pw_rpc_test_protos/test.raw_rpc.pb.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_thread/sleep.h"
#include "pw_thread/test_threads.h"
#include "pw_thread/thread.h"
#include "pw_thread/yield.h"

namespace pw::rpc {
namespace {

using namespace std::chrono_literals;

using test::pw_rpc::raw::TestService;

void YieldToOtherThread() {
  // Sleep for a while and then yield just to be sure the other thread runs.
  this_thread::sleep_for(100ms);
  this_thread::yield();
}

class CallbacksTest : public ::testing::Test {
 protected:
  CallbacksTest()
      : callback_thread_(
            thread::test::TestOptionsThread0(),
            [](void* arg) {
              static_cast<CallbacksTest*>(arg)->SendResponseAfterSemaphore();
            },
            this) {}

  ~CallbacksTest() override {
    EXPECT_FALSE(callback_thread_.joinable());  // Tests must join the thread!
    EXPECT_TRUE(callback_executed_);
  }

  void RespondToCall(const RawClientReaderWriter& call) {
    respond_to_call_ = &call;
  }

  RawClientTestContext<> context_;
  sync::BinarySemaphore callback_thread_sem_;
  sync::BinarySemaphore main_thread_sem_;

  thread::Thread callback_thread_;

  // Must be set to true by the RPC callback in each test.
  volatile bool callback_executed_ = false;

  // Variables optionally used by tests. These are in this object so lambads
  // only need to capture [this] to access them.
  volatile bool call_is_in_scope_ = false;

  RawClientReaderWriter call_1_;
  RawClientReaderWriter call_2_;

 private:
  void SendResponseAfterSemaphore() {
    // Wait until the main thread says to send the response.
    callback_thread_sem_.acquire();

    context_.server().SendServerStream<TestService::TestBidirectionalStreamRpc>(
        {}, respond_to_call_->id());
  }

  const RawClientReaderWriter* respond_to_call_ = &call_1_;
};

TEST_F(CallbacksTest, DISABLED_DestructorWaitsUntilCallbacksComplete) {
  {
    RawClientReaderWriter local_call = TestService::TestBidirectionalStreamRpc(
        context_.client(), context_.channel().id());
    RespondToCall(local_call);

    call_is_in_scope_ = true;

    local_call.set_on_next([this](ConstByteSpan) {
      main_thread_sem_.release();

      // Wait for a while so the main thread tries to destroy the call.
      YieldToOtherThread();

      // Now, make sure the call is still in scope. The main thread should
      // block in the call's destructor until this callback completes.
      EXPECT_TRUE(call_is_in_scope_);

      callback_executed_ = true;
    });

    // Start the callback thread so it can invoke the callback.
    callback_thread_sem_.release();

    // Wait until the callback thread starts.
    main_thread_sem_.acquire();
  }

  // The callback thread will sleep for a bit. Meanwhile, let the call go out
  // of scope, and mark it as such.
  call_is_in_scope_ = false;

  // Wait for the callback thread to finish.
  callback_thread_.join();

  EXPECT_TRUE(callback_executed_);
}

TEST_F(CallbacksTest, DISABLED_MoveActiveCall_WaitsForCallbackToComplete) {
  call_1_ = TestService::TestBidirectionalStreamRpc(
      context_.client(), context_.channel().id(), [this](ConstByteSpan) {
        main_thread_sem_.release();  // Confirm that this thread started

        YieldToOtherThread();

        callback_executed_ = true;
      });

  // Start the callback thread so it can invoke the callback.
  callback_thread_sem_.release();

  // Confirm that the callback thread started.
  main_thread_sem_.acquire();

  // Move the call object. This thread should wait until the on_completed
  // callback is done.
  EXPECT_TRUE(call_1_.active());
  call_2_ = std::move(call_1_);

  // The callback should already have finished. This thread should have waited
  // for it to finish during the move.
  EXPECT_TRUE(callback_executed_);
  EXPECT_FALSE(call_1_.active());
  EXPECT_TRUE(call_2_.active());

  callback_thread_.join();
}

TEST_F(CallbacksTest, MoveOtherCallIntoOwnCallInCallback) {
  call_1_ = TestService::TestBidirectionalStreamRpc(
      context_.client(), context_.channel().id(), [this](ConstByteSpan) {
        main_thread_sem_.release();  // Confirm that this thread started

        call_1_ = std::move(call_2_);

        callback_executed_ = true;
      });

  call_2_ = TestService::TestBidirectionalStreamRpc(context_.client(),
                                                    context_.channel().id());

  EXPECT_TRUE(call_1_.active());
  EXPECT_TRUE(call_2_.active());

  // Start the callback thread and wait for it to finish.
  callback_thread_sem_.release();
  callback_thread_.join();

  EXPECT_TRUE(call_1_.active());
  EXPECT_FALSE(call_2_.active());
}

TEST_F(CallbacksTest, MoveOwnCallInCallback) {
  call_1_ = TestService::TestBidirectionalStreamRpc(
      context_.client(), context_.channel().id(), [this](ConstByteSpan) {
        main_thread_sem_.release();  // Confirm that this thread started

        // Cancel this call first, or the move will deadlock, since the moving
        // thread will wait for the callback thread (both this thread) to
        // terminate if the call is active.
        EXPECT_EQ(OkStatus(), call_1_.Cancel());
        call_2_ = std::move(call_1_);

        callback_executed_ = true;
      });

  call_2_ = TestService::TestBidirectionalStreamRpc(context_.client(),
                                                    context_.channel().id());

  EXPECT_TRUE(call_1_.active());
  EXPECT_TRUE(call_2_.active());

  // Start the callback thread and wait for it to finish.
  callback_thread_sem_.release();
  callback_thread_.join();

  EXPECT_FALSE(call_1_.active());
  EXPECT_FALSE(call_2_.active());
}

}  // namespace
}  // namespace pw::rpc
