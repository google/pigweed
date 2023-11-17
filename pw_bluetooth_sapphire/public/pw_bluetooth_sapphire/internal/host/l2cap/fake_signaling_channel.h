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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_SIGNALING_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_SIGNALING_CHANNEL_H_

#include <pw_async/heap_dispatcher.h>

#include <unordered_map>
#include <vector>

#include "pw_bluetooth_sapphire/internal/host/l2cap/signaling_channel.h"

// Helper for FakeSignalingChannel::AddOutbound to add file and line numbers of
// the test call site that expected the command, and to reduce one level of
// braces in the responses. |fake_sig| should be a FakeSignalingChannel lvalue.
#define EXPECT_OUTBOUND_REQ(fake_sig, req_code, req_payload, ...) \
  (fake_sig).AddOutbound(                                         \
      __FILE__, __LINE__, req_code, req_payload, {__VA_ARGS__})

namespace bt::l2cap::internal::testing {

// Can be injected into internal L2CAP tests to drive fake interactions over the
// signaling channels with remote peers (in both directions). Expectations for
// inbound and outbound expected transactions are not synchronized.
class FakeSignalingChannel : public SignalingChannelInterface {
 public:
  using TransactionId = size_t;

  // Simulated response's command code and payload.
  using Response = std::pair<Status, BufferView>;

  // |dispatcher| is the test message loop's dispatcher
  explicit FakeSignalingChannel(pw::async::Dispatcher& pw_dispatcher);
  ~FakeSignalingChannel() override;

  // SignalingChannelInterface overrides
  bool SendRequest(CommandCode req_code,
                   const ByteBuffer& payload,
                   ResponseHandler cb) override;
  void ServeRequest(CommandCode req_code, RequestDelegate cb) override;

  // Add an expected outbound request, which FakeSignalingChannel will respond
  // to with the contents of |responses|. The request's contents will be
  // expected to match |req_code| and |req_payload|. The request's response
  // handler will be expected to handle all responses provided here.
  // Returns a handle that can be used to provide additional responses with
  // |ReceiveResponses|. |file| and |line| will be used to trace test failures.
  TransactionId AddOutbound(const char* file,
                            int line,
                            CommandCode req_code,
                            BufferView req_payload,
                            std::vector<Response> responses);

  // Receive additional responses to an already received request.
  void ReceiveResponses(TransactionId id,
                        const std::vector<Response>& responses);

  // Simulate reception of an inbound request with |req_code| and |req_payload|,
  // then expect a corresponding outbound response with payload |rsp_payload|.
  void ReceiveExpect(CommandCode req_code,
                     const ByteBuffer& req_payload,
                     const ByteBuffer& rsp_payload);

  // Simulate reception of an inbound request with |req_code| and |req_payload|,
  // then expect a matching rejection with the Not Understood reason.
  void ReceiveExpectRejectNotUnderstood(CommandCode req_code,
                                        const ByteBuffer& req_payload);

  // Simulate reception of an inbound request with |req_code| and |req_payload|,
  // then expect a matching rejection with the Invalid Channel ID reason and the
  // rejected IDs |local_cid| and |remote_cid|.
  void ReceiveExpectRejectInvalidChannelId(CommandCode req_code,
                                           const ByteBuffer& req_payload,
                                           ChannelId local_cid,
                                           ChannelId remote_cid);

 private:
  // Expected outbound request and response(s) that this fake sends back
  struct Transaction {
    const char* const file;
    const int line;
    const CommandCode request_code;
    const BufferView req_payload;
    const std::vector<std::pair<Status, BufferView>> responses;

    // Assigned when the request is actually sent
    SignalingChannel::ResponseHandler response_callback = nullptr;

    // Does not include responses handled in |ReceiveResponses|.
    size_t responses_handled = 0UL;
  };

  // Simulate reception of |responses|, calling |transaction.response_callback|
  // on each response until it returns false. Returns the number of invocations.
  size_t TriggerResponses(const Transaction& transaction,
                          const std::vector<Response>& responses);

  // Test a previously-registered request handler by simulating an inbound
  // request of |req_code| and |req_payload|. The test will assert-fail if no
  // handler had been registered with |ServeRequest|. |fake_responder| will be
  // generated internally based on the kind of reply that the handler is
  // expected to send and is passed to the handler-under-test. The test will
  // fail if no reply at all is sent.
  void ReceiveExpectInternal(CommandCode req_code,
                             const ByteBuffer& req_payload,
                             Responder* fake_responder);

  // Expected outbound transactions
  std::vector<Transaction> transactions_;

  // Index of current outbound transaction expected through SendRequest
  size_t expected_transaction_index_ = 0UL;

  // Registered inbound request delegates
  std::unordered_map<CommandCode, RequestDelegate> request_handlers_;

  // Test message loop dispatcher
  pw::async::HeapDispatcher heap_dispatcher_;
};

}  // namespace bt::l2cap::internal::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_SIGNALING_CHANNEL_H_
