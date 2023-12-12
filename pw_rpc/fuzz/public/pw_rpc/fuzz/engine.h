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
#pragma once

#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <variant>

#include "pw_containers/vector.h"
#include "pw_random/xor_shift.h"
#include "pw_rpc/benchmark.h"
#include "pw_rpc/benchmark.raw_rpc.pb.h"
#include "pw_rpc/fuzz/alarm_timer.h"
#include "pw_sync/condition_variable.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_sync/timed_mutex.h"

namespace pw::rpc::fuzz {

/// Describes an action a fuzzing thread can perform on a call.
struct Action {
  enum Op : uint8_t {
    /// No-op.
    kSkip,

    /// Waits for the call indicated by `target` to complete.
    kWait,

    /// Makes a new unary request using the call indicated by `target`. The data
    /// written is derived from `value`.
    kWriteUnary,

    /// Writes to a stream request using the call indicated by `target`, or
    /// makes
    /// a new one if not currently a stream call.  The data written is derived
    /// from `value`.
    kWriteStream,

    /// Closes the stream if the call indicated by `target` is a stream call.
    kCloseClientStream,

    /// Cancels the call indicated by `target`.
    kCancel,

    /// Abandons the call indicated by `target`.
    kAbandon,

    /// Swaps the call indicated by `target` with a call indicated by `value`.
    kSwap,

    /// Sets the call indicated by `target` to an initial, unset state.
    kDestroy,
  };

  constexpr Action() = default;
  Action(uint32_t encoded);
  Action(Op op, size_t target, uint16_t value);
  Action(Op op, size_t target, char val, size_t len);
  ~Action() = default;

  void set_thread_id(size_t thread_id_) {
    thread_id = thread_id_;
    callback_id = std::numeric_limits<size_t>::max();
  }

  void set_callback_id(size_t callback_id_) {
    thread_id = 0;
    callback_id = callback_id_;
  }

  // For a write action's value, returns the character value to be written.
  static char DecodeWriteValue(uint16_t value);

  // For a write action's value, returns the number of characters to be written.
  static size_t DecodeWriteLength(uint16_t value);

  /// Returns a value that represents the fields of an action. Constructing an
  /// `Action` with this value will produce the same fields.
  uint32_t Encode() const;

  /// Records details of the action being performed if verbose logging is
  /// enabled.
  void Log(bool verbose, size_t num_actions, const char* fmt, ...) const;

  /// Records an encountered when trying to log an action.
  void LogFailure(bool verbose, size_t num_actions, Status status) const;

  Op op = kSkip;
  size_t target = 0;
  uint16_t value = 0;

  size_t thread_id = 0;
  size_t callback_id = std::numeric_limits<size_t>::max();
};

/// Wraps an RPC call that may be either a `RawUnaryReceiver` or
/// `RawClientReaderWriter`. Allows applying `Action`s to each possible
/// type of call.
class FuzzyCall {
 public:
  using Variant =
      std::variant<std::monostate, RawUnaryReceiver, RawClientReaderWriter>;

  explicit FuzzyCall(size_t index) : index_(index), id_(index) {}
  ~FuzzyCall() = default;

  size_t id() {
    std::lock_guard lock(mutex_);
    return id_;
  }

  bool pending() {
    std::lock_guard lock(mutex_);
    return pending_;
  }

  /// Applies the given visitor to the call variant. If the action taken by the
  /// visitor is expected to complete the call, it will notify any threads
  /// waiting for the call to complete. This version of the method does not
  /// return the result of the visiting the variant.
  template <typename Visitor,
            typename std::enable_if_t<
                std::is_same_v<typename Visitor::result_type, void>,
                int> = 0>
  typename Visitor::result_type Visit(Visitor visitor, bool completes = true) {
    {
      std::lock_guard lock(mutex_);
      std::visit(std::move(visitor), call_);
    }
    if (completes && pending_.exchange(false)) {
      cv_.notify_all();
    }
  }

  /// Applies the given visitor to the call variant. If the action taken by the
  /// visitor is expected to complete the call, it will notify any threads
  /// waiting for the call to complete. This version of the method returns the
  /// result of the visiting the variant.
  template <typename Visitor,
            typename std::enable_if_t<
                !std::is_same_v<typename Visitor::result_type, void>,
                int> = 0>
  typename Visitor::result_type Visit(Visitor visitor, bool completes = true) {
    typename Visitor::result_type result;
    {
      std::lock_guard lock(mutex_);
      result = std::visit(std::move(visitor), call_);
    }
    if (completes && pending_.exchange(false)) {
      cv_.notify_all();
    }
    return result;
  }

  // Records the number of bytes written as part of a request. If `append` is
  // true, treats the write as a continuation of a streaming request.
  void RecordWrite(size_t num, bool append = false);

  /// Waits to be notified that a callback has been invoked.
  void Await() PW_LOCKS_EXCLUDED(mutex_);

  /// Completes the call, notifying any waiters.
  void Notify() PW_LOCKS_EXCLUDED(mutex_);

  /// Exchanges the call represented by this object with another.
  void Swap(FuzzyCall& other);

  /// Resets the call wrapped by this object with a new one. Destorys the
  /// previous call.
  void Reset(Variant call = Variant()) PW_LOCKS_EXCLUDED(mutex_);

  // Reports the state of this object.
  void Log() PW_LOCKS_EXCLUDED(mutex_);

 private:
  /// This represents the index in the engine's list of calls. It is used to
  /// ensure a consistent order of locking multiple calls.
  const size_t index_;

  sync::TimedMutex mutex_;
  sync::ConditionVariable cv_;

  /// An identifier that can be used find this object, e.g. by a callback, even
  /// when it has been swapped with another call.
  size_t id_ PW_GUARDED_BY(mutex_);

  /// Holds the actual pw::rpc::Call object, when present.
  Variant call_ PW_GUARDED_BY(mutex_);

  /// Set when a request is sent, and cleared when a callback is invoked.
  std::atomic_bool pending_ = false;

  /// Bytes sent in the last unary request or stream write.
  size_t last_write_ PW_GUARDED_BY(mutex_) = 0;

  /// Total bytes sent using this call object.
  size_t total_written_ PW_GUARDED_BY(mutex_) = 0;
};

/// The main RPC fuzzing engine.
///
/// This class takes or generates a sequence of actions, and dsitributes them to
/// a number of threads that can perform them using an RPC client. Passing the
/// same seed to the engine at construction will allow it to generate the same
/// sequence of actions.
class Fuzzer {
 public:
  /// Number of fuzzing threads. The first thread counted is the RPC dispatch
  /// thread.
  static constexpr size_t kNumThreads = 4;

  /// Maximum number of actions that a single thread will try to perform before
  /// exiting.
  static constexpr size_t kMaxActionsPerThread = 255;

  /// The number of call objects available to be used for fuzzing.
  static constexpr size_t kMaxConcurrentCalls = 8;

  /// The mxiumum number of individual fuzzing actions that the fuzzing threads
  /// can perform. The `+ 1` is to allow the inclusion of a special `0` action
  /// to separate each thread's actions when concatenated into a single list.
  static constexpr size_t kMaxActions =
      kNumThreads * (kMaxActionsPerThread + 1);

  explicit Fuzzer(Client& client, uint32_t channel_id);

  /// The fuzzer engine should remain pinned in memory since it is referenced by
  /// the `CallbackContext`s.
  Fuzzer(const Fuzzer&) = delete;
  Fuzzer(Fuzzer&&) = delete;
  Fuzzer& operator=(const Fuzzer&) = delete;
  Fuzzer& operator=(Fuzzer&&) = delete;

  void set_verbose(bool verbose) { verbose_ = verbose; }

  /// Sets the timeout and starts the timer.
  void set_timeout(chrono::SystemClock::duration timeout) {
    timer_.Start(timeout);
  }

  /// Generates encoded actions from the RNG and `Run`s them.
  void Run(uint64_t seed, size_t num_actions);

  /// Splits the provided `actions` between the fuzzing threads and runs them to
  /// completion.
  void Run(const Vector<uint32_t>& actions);

 private:
  /// Information passed to the RPC callbacks, including the index of the
  /// associated call and a pointer to the fuzzer object.
  struct CallbackContext {
    size_t id;
    Fuzzer* fuzzer;
  };

  /// Restarts the alarm timer, delaying it from detecting a timeout. This is
  /// called whenever actions complete and indicates progress is still being
  /// made.
  void ResetTimerLocked() PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  /// Decodes the `encoded` action and performs it. The `thread_id` is used for
  /// verbose diagnostics. When invoked from `PerformCallback` the `callback_id`
  /// will be set to the index of the associated call. This allows avoiding
  /// specific, prohibited actions, e.g. destroying a call from its own
  /// callback.
  void Perform(const Action& action) PW_LOCKS_EXCLUDED(mutex_);

  /// Returns the call with the matching `id`.
  FuzzyCall& FindCall(size_t id) PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    return FindCallLocked(id);
  }

  FuzzyCall& FindCallLocked(size_t id) PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return fuzzy_calls_[indices_[id]];
  }

  /// Returns a pointer to callback context for the given call index.
  CallbackContext* GetContext(size_t callback_id) PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    return &contexts_[callback_id];
  }

  /// Callback for stream write made by the call with the given `callback_id`.
  void OnNext(size_t callback_id) PW_LOCKS_EXCLUDED(mutex_);

  /// Callback for completed request for the call with the given `callback_id`.
  void OnCompleted(size_t callback_id) PW_LOCKS_EXCLUDED(mutex_);

  /// Callback for an error for the call with the given `callback_id`.
  void OnError(size_t callback_id, Status status) PW_LOCKS_EXCLUDED(mutex_);

  bool verbose_ = false;
  pw_rpc::raw::Benchmark::Client client_;
  BenchmarkService service_;

  /// Alarm thread that detects when no workers have made recent progress.
  AlarmTimer timer_;

  sync::Mutex mutex_;

  /// Worker threads. The first thread is the RPC response dispatcher.
  Vector<std::thread, kNumThreads> threads_;

  /// RPC call objects.
  Vector<FuzzyCall, kMaxConcurrentCalls> fuzzy_calls_;

  /// Maps each call's IDs to its index. Since calls may be move before their
  /// callbacks are invoked, this list can be used to find the original call.
  Vector<size_t, kMaxConcurrentCalls> indices_ PW_GUARDED_BY(mutex_);

  /// Context objects used to reference the engine and call.
  Vector<CallbackContext, kMaxConcurrentCalls> contexts_ PW_GUARDED_BY(mutex_);

  /// Set of actions performed as callbacks from other calls.
  Vector<uint32_t, kMaxActionsPerThread> callback_actions_ PW_GUARDED_BY(
      mutex_);
  Vector<uint32_t>::iterator callback_iterator_ PW_GUARDED_BY(mutex_);

  /// Total actions performed by all workers.
  std::atomic<size_t> num_actions_ = 0;
};

}  // namespace pw::rpc::fuzz
