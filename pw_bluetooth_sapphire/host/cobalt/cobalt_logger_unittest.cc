// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt_logger.h"

#include <fbl/macros.h>

#include "lib/async-loop/default.h"
#include "lib/fidl/cpp/binding.h"
#include "lib/fidl/cpp/interface_ptr.h"
#include "lib/fidl/cpp/interface_request.h"
#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"

namespace bt {
namespace cobalt {
namespace {

class COBALT_CobaltLoggerTest : public ::gtest::TestLoopFixture {
 public:
  class LoggerImpl : public ::fuchsia::cobalt::Logger {
   public:
    LoggerImpl() : requests_made_(0), callback_status_(::fuchsia::cobalt::Status::OK){};
    ~LoggerImpl() override = default;

    void LogEvent(uint32_t metric_id, uint32_t event_code, LogEventCallback callback) override {
      callback(callback_status_);
      requests_made_++;
      ::fuchsia::cobalt::CobaltEvent event = {metric_id, {event_code}};
      last_recorded_event_ = std::make_unique<::fuchsia::cobalt::CobaltEvent>(std::move(event));
    };
    void LogEventCount(uint32_t metric_id, uint32_t event_code, ::std::string component,
                       int64_t period_duration_micros, int64_t count,
                       LogEventCountCallback callback) override {
      callback(callback_status_);
      requests_made_++;
      ::fuchsia::cobalt::CobaltEvent event = {metric_id, {event_code}};
      ::fuchsia::cobalt::CountEvent payload = {period_duration_micros, count};
      event.payload.set_event_count(std::move(payload));
      last_recorded_event_ = std::make_unique<::fuchsia::cobalt::CobaltEvent>(std::move(event));
    };
    void LogElapsedTime(uint32_t metric_id, uint32_t event_code, ::std::string component,
                        int64_t elapsed_micros, LogElapsedTimeCallback callback) override {
      callback(callback_status_);
      requests_made_++;
      ::fuchsia::cobalt::CobaltEvent event = {metric_id, {event_code}};
      event.payload.set_elapsed_micros(elapsed_micros);
      last_recorded_event_ = std::make_unique<::fuchsia::cobalt::CobaltEvent>(std::move(event));
    };
    void LogFrameRate(uint32_t metric_id, uint32_t event_code, ::std::string component, float fps,
                      LogFrameRateCallback callback) override {
      FAIL() << "Mock method unimplemented";
    };
    void LogMemoryUsage(uint32_t metric_id, uint32_t event_code, ::std::string component,
                        int64_t bytes, LogMemoryUsageCallback callback) override {
      FAIL() << "Mock method unimplemented";
    };

    void LogString(uint32_t metric_id, ::std::string s, LogStringCallback callback) override {
      FAIL() << "Mock method unimplemented";
    };
    void StartTimer(uint32_t metric_id, uint32_t event_code, ::std::string component,
                    ::std::string timer_id, uint64_t timestamp, uint32_t timeout_s,
                    StartTimerCallback callback) override {
      FAIL() << "Mock method unimplemented";
    };
    void EndTimer(::std::string timer_id, uint64_t timestamp, uint32_t timeout_s,
                  EndTimerCallback callback) override {
      FAIL() << "Mock method unimplemented";
    };
    void LogIntHistogram(uint32_t metric_id, uint32_t event_code, ::std::string component,
                         ::std::vector<::fuchsia::cobalt::HistogramBucket> histogram,
                         LogIntHistogramCallback callback) override {
      FAIL() << "Mock method unimplemented";
    };
    void LogCustomEvent(uint32_t metric_id,
                        ::std::vector<::fuchsia::cobalt::CustomEventValue> event_values,
                        LogCustomEventCallback callback) override {
      FAIL() << "Mock method unimplemented";
    };
    void LogCobaltEvent(::fuchsia::cobalt::CobaltEvent event,
                        LogCobaltEventCallback callback) override {
      callback(callback_status_);
      requests_made_++;
      last_recorded_event_ = std::make_unique<::fuchsia::cobalt::CobaltEvent>(std::move(event));
    };
    void LogCobaltEvents(::std::vector<::fuchsia::cobalt::CobaltEvent> events,
                         LogCobaltEventsCallback callback) override {
      callback(callback_status_);
      requests_made_++;
      if (!events.empty()) {
        last_recorded_event_ =
            std::make_unique<::fuchsia::cobalt::CobaltEvent>(std::move(events[events.size() - 1]));
      } else {
        last_recorded_event_ = nullptr;
      }
    };

    // Return the total number of requests made to the server
    uint32_t RequestsMade() const { return requests_made_; }

    // Return the last event that was sent from a client to the server.
    const ::fuchsia::cobalt::CobaltEvent* LastRecordedEvent() const {
      return last_recorded_event_.get();
    }

    // All requests to record a cobalt event contain a response with a single status value.
    // Use this method to set the cobalt status that will be passed to fidl response callbacks.
    void SetCallbackStatus(::fuchsia::cobalt::Status status) { callback_status_ = status; }

   private:
    uint32_t requests_made_;
    std::unique_ptr<::fuchsia::cobalt::CobaltEvent> last_recorded_event_;
    ::fuchsia::cobalt::Status callback_status_;
  };

 public:
  COBALT_CobaltLoggerTest() = default;
  ~COBALT_CobaltLoggerTest() override = default;

 protected:
  void TearDown() override {
    RunLoopUntilIdle();  // Run all pending tasks.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(COBALT_CobaltLoggerTest);
};

// Tests that the cobalt logger without a interface handle to the cobalt service will
// silently ignore logging requests
TEST_F(COBALT_CobaltLoggerTest, NotBoundToService) {
  auto logger = CobaltLogger::Create();
  logger->LogEvent(1, 2);
  logger->LogEventCount(1, 2, 3);
  logger->LogElapsedTime(1, 2, 3);
  logger->LogCobaltEvent({1, {}, EventCount{2}});
  logger->LogCobaltEvents({{1, {}, EventCount{2}}, {2, {2}, ElapsedMicros{3}}});
}

TEST_F(COBALT_CobaltLoggerTest, BoundToService) {
  auto logger = CobaltLogger::Create();

  LoggerImpl impl;
  ::fidl::Binding<fuchsia::cobalt::Logger> binding(&impl);

  ::fidl::InterfaceHandle<::fuchsia::cobalt::Logger> handle;
  binding.Bind(handle.NewRequest(), dispatcher());
  EXPECT_TRUE(handle.is_valid());

  EXPECT_EQ(async_get_default_dispatcher(), dispatcher());

  logger->Bind(std::move(handle));
  EXPECT_EQ(impl.RequestsMade(), 0u);
  EXPECT_EQ(impl.LastRecordedEvent(), nullptr);

  logger->LogEvent(1, 2);
  RunLoopUntilIdle();

  EXPECT_EQ(impl.RequestsMade(), 1u);
  EXPECT_EQ(impl.LastRecordedEvent()->metric_id, 1u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes.size(), 1u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes[0], 2u);

  logger->LogEventCount(3, 4, 5);
  RunLoopUntilIdle();

  EXPECT_EQ(impl.RequestsMade(), 2u);
  EXPECT_EQ(impl.LastRecordedEvent()->metric_id, 3u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes.size(), 1u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes[0], 4u);
  EXPECT_EQ(impl.LastRecordedEvent()->payload.event_count().count, 5u);

  logger->LogElapsedTime(6, 7, 8);
  RunLoopUntilIdle();

  EXPECT_EQ(impl.RequestsMade(), 3u);
  EXPECT_EQ(impl.LastRecordedEvent()->metric_id, 6u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes.size(), 1u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes[0], 7u);
  EXPECT_EQ(impl.LastRecordedEvent()->payload.elapsed_micros(), 8u);

  logger->LogCobaltEvent({9, {10}, EventCount{11}});
  RunLoopUntilIdle();

  EXPECT_EQ(impl.RequestsMade(), 4u);
  EXPECT_EQ(impl.LastRecordedEvent()->metric_id, 9u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes.size(), 1u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes[0], 10u);
  EXPECT_EQ(impl.LastRecordedEvent()->payload.event_count().count, 11u);

  logger->LogCobaltEvents({{12, {13, 14}, EventCount{15}}});
  RunLoopUntilIdle();

  EXPECT_EQ(impl.RequestsMade(), 5u);
  EXPECT_EQ(impl.LastRecordedEvent()->metric_id, 12u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes.size(), 2u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes[0], 13u);
  EXPECT_EQ(impl.LastRecordedEvent()->event_codes[1], 14u);
  EXPECT_EQ(impl.LastRecordedEvent()->payload.event_count().count, 15u);
}

TEST_F(COBALT_CobaltLoggerTest, CloseServer) {
  auto logger = CobaltLogger::Create();

  LoggerImpl impl;
  ::fidl::Binding<fuchsia::cobalt::Logger> binding(&impl);

  ::fidl::InterfaceHandle<::fuchsia::cobalt::Logger> handle;
  binding.Bind(handle.NewRequest(), dispatcher());
  EXPECT_TRUE(handle.is_valid());

  EXPECT_EQ(async_get_default_dispatcher(), dispatcher());

  logger->Bind(std::move(handle));
  EXPECT_EQ(impl.RequestsMade(), 0u);
  EXPECT_EQ(impl.LastRecordedEvent(), nullptr);

  logger->LogEvent(1, 2);
  // Ensure that the server is set up correctly before shuting it down.
  RunLoopUntilIdle();

  logger->LogEvent(3, 4);
  EXPECT_TRUE(binding.is_bound());
  binding.Close(ZX_ERR_PEER_CLOSED);
  EXPECT_FALSE(binding.is_bound());
  logger->LogEvent(5, 6);

  RunLoopUntilIdle();
  // Only the first event was logged.
  EXPECT_EQ(impl.RequestsMade(), 1u);
  EXPECT_NE(impl.LastRecordedEvent(), nullptr);
}

}  // namespace
}  // namespace cobalt
}  // namespace bt
