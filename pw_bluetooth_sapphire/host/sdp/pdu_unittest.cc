
// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/sdp/pdu.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/sdp.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/status.h"

namespace bt::sdp {
namespace {

// The Default MTU in basic mode (Spec v5.1, Vol 3 Part A Section 5.1)
const uint16_t kDefaultMaxSize = 672;
// The smallest MTU allowed by the spec.
const uint16_t kMinMaxSize = 48;

// Helper function to match one of two options, and print useful information on failure.
template <class Container1, class Container2, class Container3>
bool MatchesOneOf(const Container1& one, const Container2& two, const Container3& actual) {
  bool opt_one = std::equal(one.begin(), one.end(), actual.begin(), actual.end());
  bool opt_two = std::equal(two.begin(), two.end(), actual.begin(), actual.end());

  if (!(opt_one || opt_two)) {
    std::cout << "Expected one of {";
    PrintByteContainer(one.begin(), one.end());
    std::cout << "}\n or {";
    PrintByteContainer(two.begin(), two.end());
    std::cout << "}\n   Found: { ";
    PrintByteContainer(actual.begin(), actual.end());
    std::cout << "}" << std::endl;
  }
  return opt_one || opt_two;
}

TEST(PDUTest, ErrorResponse) {
  ErrorResponse response;
  EXPECT_FALSE(response.complete());

  auto kInvalidContState =
      CreateStaticByteBuffer(0x01,        // opcode: kErrorResponse
                             0xDE, 0xAD,  // transaction ID: 0xDEAD
                             0x00, 0x02,  // parameter length: 2 bytes
                             0x00, 0x05,  // ErrorCode: Invalid Continuation State
                             0xFF, 0x00   // dummy extra bytes to cause an error
      );

  Status status = response.Parse(kInvalidContState.view(sizeof(Header)));
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kPacketMalformed, status.error());

  status = response.Parse(kInvalidContState.view(sizeof(Header), 2));
  EXPECT_TRUE(status);
  EXPECT_TRUE(response.complete());
  EXPECT_EQ(ErrorCode::kInvalidContinuationState, response.error_code());

  response.set_error_code(ErrorCode::kInvalidContinuationState);
  auto ptr =
      response.GetPDU(0xF00F /* ignored */, 0xDEAD, kDefaultMaxSize /* ignored */, BufferView());

  EXPECT_TRUE(ContainersEqual(kInvalidContState.view(0, 7), *ptr));
}

TEST(PDUTest, ServiceSearchRequestParse) {
  const auto kL2capSearch = CreateStaticByteBuffer(
      // ServiceSearchPattern
      0x35, 0x03,        // Sequence uint8 3 bytes
      0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
      0x00, 0x10,        // MaximumServiceRecordCount: 16
      0x00               // Contunuation State: none
  );

  ServiceSearchRequest req(kL2capSearch);
  EXPECT_TRUE(req.valid());
  EXPECT_EQ(1u, req.service_search_pattern().size());
  EXPECT_TRUE(req.service_search_pattern().count(protocol::kL2CAP));
  EXPECT_EQ(16, req.max_service_record_count());

  const auto kL2capSearchOne = CreateStaticByteBuffer(
      // ServiceSearchPattern
      0x35, 0x06,        // Sequence uint8 6 bytes
      0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
      0x19, 0xED, 0xFE,  // UUID: 0xEDFE (unknown, doesn't need to be found)
      0x00, 0x01,        // MaximumServiceRecordCount: 1
      0x00               // Contunuation State: none
  );

  ServiceSearchRequest req_one(kL2capSearchOne);
  EXPECT_TRUE(req_one.valid());
  EXPECT_EQ(2u, req_one.service_search_pattern().size());
  EXPECT_EQ(1, req_one.max_service_record_count());

  const auto kInvalidNoItems = CreateStaticByteBuffer(
      // ServiceSearchPattern
      0x35, 0x00,  // Sequence uint8 0 bytes
      0xFF, 0xFF,  // MaximumServiceRecordCount: (none)
      0x00         // Contunuation State: none
  );

  ServiceSearchRequest req2(kInvalidNoItems);
  EXPECT_FALSE(req2.valid());

  const auto kInvalidTooManyItems = CreateStaticByteBuffer(
      // ServiceSearchPattern
      0x35, 0x27,        // Sequence uint8 27 bytes
      0x19, 0x30, 0x01,  // 13 UUIDs in the search
      0x19, 0x30, 0x02, 0x19, 0x30, 0x03, 0x19, 0x30, 0x04, 0x19, 0x30, 0x05, 0x19, 0x30, 0x06,
      0x19, 0x30, 0x07, 0x19, 0x30, 0x08, 0x19, 0x30, 0x09, 0x19, 0x30, 0x10, 0x19, 0x30, 0x11,
      0x19, 0x30, 0x12, 0x19, 0x30, 0x13, 0xFF, 0xFF,  // MaximumServiceRecordCount: (none)
      0x00                                             // Contunuation State: none
  );

  ServiceSearchRequest req3(kInvalidTooManyItems);
  EXPECT_FALSE(req3.valid());

  const auto kInvalidMaxSizeZero = CreateStaticByteBuffer(
      // ServiceSearchPattern
      0x35, 0x06,        // Sequence uint8 6 bytes
      0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
      0x19, 0xED, 0xFE,  // UUID: 0xEDFE (unknown, doesn't need to be found)
      0x00, 0x00,        // MaximumServiceRecordCount: 0
      0x00               // Contunuation State: none
  );

  ServiceSearchRequest req4(kInvalidMaxSizeZero);
  EXPECT_FALSE(req4.valid());
};

TEST(PDUTest, ServiceSearchRequestGetPDU) {
  ServiceSearchRequest req;

  req.set_search_pattern({protocol::kATT, protocol::kL2CAP});
  req.set_max_service_record_count(64);

  // Order is not specified, so there are two valid PDUs representing this.
  const auto kExpected =
      CreateStaticByteBuffer(kServiceSearchRequest, 0x12, 0x34,  // Transaction ID
                             0x00, 0x0B,                         // Parameter length (11 bytes)
                             // ServiceSearchPattern
                             0x35, 0x06,        // Sequence uint8 6 bytes
                             0x19, 0x00, 0x07,  // UUID (ATT)
                             0x19, 0x01, 0x00,  // UUID (L2CAP)
                             0x00, 0x40,        // MaximumServiceRecordCount: 64
                             0x00               // No continuation state
      );
  const auto kExpected2 =
      CreateStaticByteBuffer(kServiceSearchRequest, 0x12, 0x34,  // Transaction ID
                             0x00, 0x0B,                         // Parameter length (11 bytes)
                             // ServiceSearchPattern
                             0x35, 0x06,        // Sequence uint8 6 bytes
                             0x19, 0x01, 0x00,  // UUID (L2CAP)
                             0x19, 0x00, 0x07,  // UUID (ATT)
                             0x00, 0x40,        // MaximumServiceRecordCount: 64
                             0x00               // No continuation state
      );

  auto pdu = req.GetPDU(0x1234);
  EXPECT_TRUE(MatchesOneOf(kExpected, kExpected2, *pdu));
};

TEST(PDUTest, ServiceSearchResponseParse) {
  const auto kValidResponse = CreateStaticByteBuffer(0x00, 0x02,  // Total service record count: 2
                                                     0x00, 0x02,  // Current service record count: 2
                                                     0x00, 0x00, 0x00, 0x01,  // Service Handle 1
                                                     0x00, 0x00, 0x00, 0x02,  // Service Handle 2
                                                     0x00  // No continuation state
  );

  ServiceSearchResponse resp;
  auto status = resp.Parse(kValidResponse);
  EXPECT_TRUE(status);

  // Can't parse into an already complete record.
  status = resp.Parse(kValidResponse);
  EXPECT_FALSE(status);

  const auto kNotEnoughRecords =
      CreateStaticByteBuffer(0x00, 0x02,              // Total service record count: 2
                             0x00, 0x02,              // Current service record count: 2
                             0x00, 0x00, 0x00, 0x01,  // Service Handle 1
                             0x00                     // No continuation state
      );
  // Doesn't contain the right # of records.
  ServiceSearchResponse resp2;
  status = resp2.Parse(kNotEnoughRecords);
  EXPECT_FALSE(status);

  // A Truncated packet doesn't parse either.
  const auto kTruncated = CreateStaticByteBuffer(0x00, 0x02,  // Total service record count: 2
                                                 0x00, 0x02   // Current service record count: 2
  );
  ServiceSearchResponse resp3;
  status = resp3.Parse(kTruncated);
  EXPECT_FALSE(status);

  // Too many bytes for the number of records is also not allowed (with or without a continuation
  // state)
  const auto kTooLong = CreateStaticByteBuffer(0x00, 0x01,  // Total service record count: 1
                                               0x00, 0x01,  // Current service record count: 1
                                               0x00, 0x00, 0x00, 0x01,  // Service Handle 1
                                               0x00, 0x00, 0x00, 0x02,  // Service Handle 2
                                               0x00                     // No continuation state
  );
  ServiceSearchResponse resp4;
  status = resp4.Parse(kTooLong);
  EXPECT_FALSE(status);

  const auto kTooLongWithContinuation =
      CreateStaticByteBuffer(0x00, 0x04,              // Total service record count: 1
                             0x00, 0x01,              // Current service record count: 1
                             0x00, 0x00, 0x00, 0x01,  // Service Handle 1
                             0x00, 0x00, 0x00, 0x02,  // Service Handle 2
                             0x04,                    // Continuation state (len: 4)
                             0xF0, 0x9F, 0x92, 0x96   // Continuation state
      );
  ServiceSearchResponse resp5;
  status = resp5.Parse(kTooLongWithContinuation);
  EXPECT_FALSE(status);
};

TEST(PDUTest, ServiceSearchResponsePDU) {
  std::vector<ServiceHandle> results{1, 2};
  ServiceSearchResponse resp;

  // Empty results
  const auto kExpectedEmpty = CreateStaticByteBuffer(0x03,        // ServiceSearch Response PDU ID
                                                     0x01, 0x10,  // Transaction ID (0x0110)
                                                     0x00, 0x05,  // Parameter length: 5 bytes
                                                     0x00, 0x00,  // Total service record count: 0
                                                     0x00, 0x00,  // Current service record count: 0
                                                     0x00         // No continuation state
  );

  auto pdu = resp.GetPDU(0xFFFF, 0x0110, kDefaultMaxSize, BufferView());
  EXPECT_TRUE(ContainersEqual(kExpectedEmpty, *pdu));

  resp.set_service_record_handle_list(results);

  const auto kExpected = CreateStaticByteBuffer(0x03,        // ServiceSearch Response PDU ID
                                                0x01, 0x10,  // Transaction ID (0x0110)
                                                0x00, 0x0d,  // Parameter length: 13 bytes
                                                0x00, 0x02,  // Total service record count: 2
                                                0x00, 0x02,  // Current service record count: 2
                                                0x00, 0x00, 0x00, 0x01,  // Service record 1
                                                0x00, 0x00, 0x00, 0x02,  // Service record 2
                                                0x00                     // No continuation state
  );

  pdu = resp.GetPDU(0xFFFF, 0x0110, kDefaultMaxSize, BufferView());
  EXPECT_TRUE(ContainersEqual(kExpected, *pdu));

  const auto kExpectedLimited =
      CreateStaticByteBuffer(0x03,                    // ServiceSearchResponse PDU ID
                             0x01, 0x10,              // Transaction ID (0x0110)
                             0x00, 0x09,              // Parameter length: 9
                             0x00, 0x01,              // Total service record count: 1
                             0x00, 0x01,              // Current service record count: 1
                             0x00, 0x00, 0x00, 0x01,  // Service record 1
                             0x00                     // No continuation state
      );

  pdu = resp.GetPDU(1, 0x0110, kDefaultMaxSize, BufferView());
  EXPECT_TRUE(ContainersEqual(kExpectedLimited, *pdu));

  resp.set_service_record_handle_list({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
  const auto kExpectedLarge =
      CreateStaticByteBuffer(0x03,                    // ServiceSearchResponse PDU ID
                             0x01, 0x10,              // Transaction ID (0x0110)
                             0x00, 0x31,              // Parameter length: 49
                             0x00, 0x0B,              // Total service record count: 11
                             0x00, 0x0B,              // Current service record count: 11
                             0x00, 0x00, 0x00, 0x01,  // Service record 1
                             0x00, 0x00, 0x00, 0x02,  // Service record 2
                             0x00, 0x00, 0x00, 0x03,  // Service record 3
                             0x00, 0x00, 0x00, 0x04,  // Service record 4
                             0x00, 0x00, 0x00, 0x05,  // Service record 5
                             0x00, 0x00, 0x00, 0x06,  // Service record 6
                             0x00, 0x00, 0x00, 0x07,  // Service record 7
                             0x00, 0x00, 0x00, 0x08,  // Service record 8
                             0x00, 0x00, 0x00, 0x09,  // Service record 9
                             0x00, 0x00, 0x00, 0x0A,  // Service record 10
                             0x00, 0x00, 0x00, 0x0B,  // Service record 11
                             0x00                     // No continuation state.
      );

  pdu = resp.GetPDU(0x00FF, 0x0110, kDefaultMaxSize, BufferView());
  EXPECT_TRUE(ContainersEqual(kExpectedLarge, *pdu));
};

TEST(PDUTest, ServiceSearchResponsePDU_MaxSize) {
  std::vector<ServiceHandle> results{1, 2};
  ServiceSearchResponse resp;

  // Empty results
  const auto kExpectedEmpty = CreateStaticByteBuffer(0x03,        // ServiceSearch Response PDU ID
                                                     0x01, 0x10,  // Transaction ID (0x0110)
                                                     0x00, 0x05,  // Parameter length: 5 bytes
                                                     0x00, 0x00,  // Total service record count: 0
                                                     0x00, 0x00,  // Current service record count: 0
                                                     0x00         // No continuation state
  );

  auto pdu = resp.GetPDU(0xFFFF, 0x0110, kMinMaxSize, BufferView());
  EXPECT_TRUE(ContainersEqual(kExpectedEmpty, *pdu));

  resp.set_service_record_handle_list(results);

  const auto kExpected = CreateStaticByteBuffer(0x03,        // ServiceSearch Response PDU ID
                                                0x01, 0x10,  // Transaction ID (0x0110)
                                                0x00, 0x0d,  // Parameter length: 13 bytes
                                                0x00, 0x02,  // Total service record count: 2
                                                0x00, 0x02,  // Current service record count: 2
                                                0x00, 0x00, 0x00, 0x01,  // Service record 1
                                                0x00, 0x00, 0x00, 0x02,  // Service record 2
                                                0x00                     // No continuation state
  );

  pdu = resp.GetPDU(0xFFFF, 0x0110, kMinMaxSize, BufferView());
  EXPECT_TRUE(ContainersEqual(kExpected, *pdu));

  const auto kExpectedLimited =
      CreateStaticByteBuffer(0x03,                    // ServiceSearchResponse PDU ID
                             0x01, 0x10,              // Transaction ID (0x0110)
                             0x00, 0x09,              // Parameter length: 9
                             0x00, 0x01,              // Total service record count: 1
                             0x00, 0x01,              // Current service record count: 1
                             0x00, 0x00, 0x00, 0x01,  // Service record 1
                             0x00                     // No continuation state
      );

  pdu = resp.GetPDU(1, 0x0110, kMinMaxSize, BufferView());
  EXPECT_TRUE(ContainersEqual(kExpectedLimited, *pdu));

  resp.set_service_record_handle_list({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
  const auto kExpectedContinuation =
      CreateStaticByteBuffer(0x03,                    // ServiceSearchResponse PDU ID
                             0x01, 0x10,              // Transaction ID (0x0110)
                             0x00, 0x2B,              // Parameter length: 42
                             0x00, 0x0B,              // Total service record count: 11
                             0x00, 0x09,              // Current service record count: 9
                             0x00, 0x00, 0x00, 0x01,  // Service record 1
                             0x00, 0x00, 0x00, 0x02,  // Service record 2
                             0x00, 0x00, 0x00, 0x03,  // Service record 3
                             0x00, 0x00, 0x00, 0x04,  // Service record 4
                             0x00, 0x00, 0x00, 0x05,  // Service record 5
                             0x00, 0x00, 0x00, 0x06,  // Service record 6
                             0x00, 0x00, 0x00, 0x07,  // Service record 7
                             0x00, 0x00, 0x00, 0x08,  // Service record 8
                             0x00, 0x00, 0x00, 0x09,  // Service record 9
                             0x02, 0x00, 0x09         // Continuation state.
      );

  // The MTU size here should limit the number of service records returned to 9.
  pdu = resp.GetPDU(0x00FF, 0x0110, kMinMaxSize, BufferView());
  EXPECT_TRUE(ContainersEqual(kExpectedContinuation, *pdu));

  const auto kExpectedRest = CreateStaticByteBuffer(0x03,        // ServiceSearchResponse PDU ID
                                                    0x01, 0x10,  // Transaction ID (0x0110)
                                                    0x00, 0x0D,  // Parameter length: 13
                                                    0x00, 0x0B,  // Total service record count: 11
                                                    0x00, 0x02,  // Current service record count: 2
                                                    0x00, 0x00, 0x00, 0x0A,  // Service record 10
                                                    0x00, 0x00, 0x00, 0x0B,  // Service record 11
                                                    0x00  // No continuation state.
  );

  pdu = resp.GetPDU(0x00FF, 0x0110, kMinMaxSize, CreateStaticByteBuffer(0x00, 0x09));
  EXPECT_TRUE(ContainersEqual(kExpectedRest, *pdu));
};

TEST(PDUTest, ServiceAttributeRequestValidity) {
  ServiceAttributeRequest req;

  // No attributes requested, so it begins invalid
  EXPECT_FALSE(req.valid());

  // Adding an attribute makes it valid, and adds a range.
  req.AddAttribute(kServiceClassIdList);

  EXPECT_TRUE(req.valid());
  EXPECT_EQ(1u, req.attribute_ranges().size());
  EXPECT_EQ(kServiceClassIdList, req.attribute_ranges().front().start);

  // Adding an attribute adjacent just adds to the range (on either end)
  req.AddAttribute(kServiceClassIdList - 1);  // kServiceRecordHandle
  req.AddAttribute(kServiceClassIdList + 1);
  req.AddAttribute(kServiceClassIdList + 2);

  EXPECT_TRUE(req.valid());
  EXPECT_EQ(1u, req.attribute_ranges().size());
  EXPECT_EQ(kServiceClassIdList - 1, req.attribute_ranges().front().start);
  EXPECT_EQ(kServiceClassIdList + 2, req.attribute_ranges().front().end);

  // Adding a disjoint range makes it two ranges, and they're in the right
  // order.
  req.AddAttribute(kServiceClassIdList + 4);

  EXPECT_TRUE(req.valid());
  EXPECT_EQ(2u, req.attribute_ranges().size());
  EXPECT_EQ(kServiceClassIdList - 1, req.attribute_ranges().front().start);
  EXPECT_EQ(kServiceClassIdList + 2, req.attribute_ranges().front().end);
  EXPECT_EQ(kServiceClassIdList + 4, req.attribute_ranges().back().start);
  EXPECT_EQ(kServiceClassIdList + 4, req.attribute_ranges().back().end);

  // Adding one that makes it contiguous collapses them to a single range.
  req.AddAttribute(kServiceClassIdList + 3);

  EXPECT_EQ(1u, req.attribute_ranges().size());
  EXPECT_EQ(kServiceClassIdList - 1, req.attribute_ranges().front().start);
  EXPECT_EQ(kServiceClassIdList + 4, req.attribute_ranges().front().end);

  EXPECT_TRUE(req.valid());
}

TEST(PDUTest, ServiceAttriuteRequestAddRange) {
  ServiceAttributeRequest req;

  req.AddAttributeRange(0x0010, 0xFFF0);

  EXPECT_EQ(1u, req.attribute_ranges().size());
  EXPECT_EQ(0x0010, req.attribute_ranges().front().start);
  EXPECT_EQ(0xFFF0, req.attribute_ranges().front().end);

  req.AddAttributeRange(0x0100, 0xFF00);

  EXPECT_EQ(1u, req.attribute_ranges().size());
  EXPECT_EQ(0x0010, req.attribute_ranges().front().start);
  EXPECT_EQ(0xFFF0, req.attribute_ranges().front().end);

  req.AddAttributeRange(0x0000, 0x0002);

  EXPECT_EQ(2u, req.attribute_ranges().size());
  EXPECT_EQ(0x0000, req.attribute_ranges().front().start);
  EXPECT_EQ(0x0002, req.attribute_ranges().front().end);

  req.AddAttributeRange(0x0003, 0x000F);

  EXPECT_EQ(1u, req.attribute_ranges().size());
  EXPECT_EQ(0x0000, req.attribute_ranges().front().start);
  EXPECT_EQ(0xFFF0, req.attribute_ranges().front().end);

  req.AddAttributeRange(0xFFF2, 0xFFF3);
  req.AddAttributeRange(0xFFF5, 0xFFF6);
  req.AddAttributeRange(0xFFF8, 0xFFF9);
  req.AddAttributeRange(0xFFFB, 0xFFFC);

  EXPECT_EQ(5u, req.attribute_ranges().size());

  // merges 0x0000-0xFFF0 with 0xFFF2-0xFFF3 and new range leaving
  // 0x0000-0xFFF3, 0xFFF5-0xFFF6, 0xFFF8-0xFFF9 and 0xFFFB-0xFFFC
  req.AddAttributeRange(0xFFF1, 0xFFF1);

  EXPECT_EQ(4u, req.attribute_ranges().size());

  // merges everything except 0xFFFB-0xFFFC
  req.AddAttributeRange(0xFFF1, 0xFFF9);

  EXPECT_EQ(2u, req.attribute_ranges().size());

  req.AddAttributeRange(0xFFFA, 0xFFFF);

  EXPECT_EQ(1u, req.attribute_ranges().size());
  EXPECT_EQ(0x0000, req.attribute_ranges().front().start);
  EXPECT_EQ(0xFFFF, req.attribute_ranges().front().end);
}

TEST(PDUTest, ServiceAttributeRequestParse) {
  const auto kSDPAllAttributes = CreateStaticByteBuffer(
      0x00, 0x00, 0x00, 0x00,  // ServiceRecordHandle: 0
      0xF0, 0x0F,              // Maximum attribute byte count: (max = 61455)
      // Attribute ID List
      0x35, 0x05,              // Sequence uint8 5 bytes
      0x0A,                    // uint32_t
      0x00, 0x00, 0xFF, 0xFF,  // Attribute range: 0x000 - 0xFFFF (All of them)
      0x00                     // No continuation state
  );

  ServiceAttributeRequest req(kSDPAllAttributes);

  EXPECT_TRUE(req.valid());
  EXPECT_EQ(1u, req.attribute_ranges().size());
  EXPECT_EQ(0x0000, req.attribute_ranges().front().start);
  EXPECT_EQ(0xFFFF, req.attribute_ranges().front().end);
  EXPECT_EQ(61455, req.max_attribute_byte_count());

  const auto kContinued =
      CreateStaticByteBuffer(0x00, 0x00, 0x00, 0x00,  // ServiceRecordHandle: 0
                             0x00, 0x0F,              // Maximum attribute byte count: (max = 15)
                             // Attribute ID List
                             0x35, 0x06,             // Sequence uint8 3 bytes
                             0x09,                   // uint16_t
                             0x00, 0x01,             // Attribute ID: 1
                             0x09,                   // uint16_t
                             0x00, 0x02,             // Attribute ID: 2
                             0x03, 0x12, 0x34, 0x56  // Continuation state
      );

  ServiceAttributeRequest req_cont_state(kContinued);

  EXPECT_TRUE(req_cont_state.valid());
  EXPECT_EQ(2u, req_cont_state.attribute_ranges().size());
  EXPECT_EQ(0x0001, req_cont_state.attribute_ranges().front().start);
  EXPECT_EQ(0x0001, req_cont_state.attribute_ranges().front().end);
  EXPECT_EQ(0x0002, req_cont_state.attribute_ranges().back().start);
  EXPECT_EQ(0x0002, req_cont_state.attribute_ranges().back().end);
  EXPECT_EQ(15, req_cont_state.max_attribute_byte_count());

  // Too short request.
  ServiceAttributeRequest req_short((BufferView()));

  EXPECT_FALSE(req_short.valid());

  const auto kInvalidMaxBytes =
      CreateStaticByteBuffer(0x00, 0x00, 0x00, 0x00,  // ServiceRecordHandle: 0
                             0x00, 0x04,              // Maximum attribute byte count (4)
                             // Attribute ID List
                             0x35, 0x03,        // Sequence uint8 3 bytes
                             0x09, 0x00, 0x02,  // uint16_t (2)
                             0x00               // No continuation state
      );

  ServiceAttributeRequest req_minmax(kInvalidMaxBytes);

  EXPECT_FALSE(req_minmax.valid());

  // Invalid order of the attributes.
  const auto kInvalidAttributeListOrder =
      CreateStaticByteBuffer(0x00, 0x00, 0x00, 0x00,  // ServiceRecordHandle: 0
                             0xFF, 0xFF,              // Maximum attribute byte count: (max = 65535)
                             // Attribute ID List
                             0x35, 0x06,        // Sequence uint8 6 bytes
                             0x09, 0x00, 0x02,  // uint16_t (2)
                             0x09, 0x00, 0x01,  // uint16_t (1)
                             0x00               // No continuation state
      );

  ServiceAttributeRequest req_order(kInvalidAttributeListOrder);

  EXPECT_FALSE(req_short.valid());

  // AttributeID List has invalid items
  const auto kInvalidAttributeList =
      CreateStaticByteBuffer(0x00, 0x00, 0x00, 0x00,  // ServiceRecordHandle: 0
                             0xFF, 0xFF,              // Maximum attribute byte count: (max = 65535)
                             // Attribute ID List
                             0x35, 0x06,        // Sequence uint8 5 bytes
                             0x09, 0x00, 0x02,  // uint16_t (2)
                             0x19, 0x00, 0x01,  // UUID (0x0001)
                             0x00               // No continuation state
      );

  ServiceAttributeRequest req_baditems(kInvalidAttributeList);

  EXPECT_FALSE(req_baditems.valid());

  // AttributeID list is empty
  const auto kInvalidNoItems =
      CreateStaticByteBuffer(0x00, 0x00, 0x00, 0x00,  // ServiceRecordHandle: 0
                             0xFF, 0xFF,              // Maximum attribute byte count: max
                             0x35, 0x00,              // Sequence uint8 no bytes
                             0x00                     // No continuation state
      );

  ServiceAttributeRequest req_noitems(kInvalidNoItems);
  EXPECT_FALSE(req_noitems.valid());
}

TEST(PDUTest, ServiceAttributeRequestGetPDU) {
  ServiceAttributeRequest req;
  req.AddAttribute(0xF00F);
  req.AddAttributeRange(0x0001, 0xBECA);

  req.set_service_record_handle(0xEFFECACE);
  req.set_max_attribute_byte_count(32);

  const auto kExpected =
      CreateStaticByteBuffer(kServiceAttributeRequest, 0x12, 0x34,  // transaction id
                             0x00, 0x11,                            // Parameter Length (17 bytes)
                             0xEF, 0xFE, 0xCA, 0xCE,  // ServiceRecordHandle (0xEFFECACE)
                             0x00, 0x20,              // MaxAttributeByteCount (32)
                             // Attribute ID list
                             0x35, 0x08,                    // Sequence uint8 8 bytes
                             0x0A, 0x00, 0x01, 0xBE, 0xCA,  // uint32_t (0x0001BECA)
                             0x09, 0xF0, 0x0F,              // uint16_t (0xF00F)
                             0x00                           // No continuation state
      );

  auto pdu = req.GetPDU(0x1234);
  EXPECT_TRUE(ContainersEqual(kExpected, *pdu));
}

TEST(PDUTest, ServiceAttributeResponseParse) {
  const auto kValidResponseEmpty = CreateStaticByteBuffer(0x00, 0x02,  // AttributeListByteCount (2
                                                                       // bytes) Attribute List
                                                          0x35, 0x00,  // Sequence uint8 0 bytes
                                                          0x00         // No continuation state
  );

  ServiceAttributeResponse resp;
  auto status = resp.Parse(kValidResponseEmpty);

  EXPECT_TRUE(status);

  const auto kValidResponseItems = CreateStaticByteBuffer(
      0x00, 0x12,  // AttributeListByteCount (18 bytes)
      // Attribute List
      0x35, 0x10,                    // Sequence uint8 16 bytes
      0x09, 0x00, 0x00,              // Handle: uint16_t (0 = kServiceRecordHandle)
      0x0A, 0xFE, 0xED, 0xBE, 0xEF,  // Element: uint32_t (0xFEEDBEEF)
      0x09, 0x00, 0x01,              // Handle: uint16_t (1 = kServiceClassIdList)
      0x35, 0x03, 0x19, 0x11, 0x01,  // Element: Sequence (3) { UUID(0x1101) }
      0x00                           // No continuation state
  );

  ServiceAttributeResponse resp2;
  status = resp2.Parse(kValidResponseItems);

  EXPECT_TRUE(status);
  EXPECT_EQ(2u, resp2.attributes().size());
  auto it = resp2.attributes().find(0x00);
  EXPECT_NE(resp2.attributes().end(), it);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->second.type());

  it = resp2.attributes().find(0x01);
  EXPECT_NE(resp2.attributes().end(), it);
  EXPECT_EQ(DataElement::Type::kSequence, it->second.type());

  const auto kInvalidItemsWrongOrder = CreateStaticByteBuffer(
      0x00, 0x12,  // AttributeListByteCount (18 bytes)
      // Attribute List
      0x35, 0x10,                    // Sequence uint8 16 bytes
      0x09, 0x00, 0x01,              // Handle: uint16_t (1 = kServiceClassIdList)
      0x35, 0x03, 0x19, 0x11, 0x01,  // Element: Sequence (3) { UUID(0x1101) }
      0x09, 0x00, 0x00,              // Handle: uint16_t (0 = kServiceRecordHandle)
      0x0A, 0xFE, 0xED, 0xBE, 0xEF,  // Element: uint32_t (0xFEEDBEEF)
      0x00                           // No continuation state
  );

  ServiceAttributeResponse resp3;
  status = resp3.Parse(kInvalidItemsWrongOrder);

  EXPECT_FALSE(status);

  const auto kInvalidHandleWrongType = CreateStaticByteBuffer(
      0x00, 0x11,  // AttributeListByteCount (17 bytes)
      // Attribute List
      0x35, 0x0F,                    // Sequence uint8 15 bytes
      0x09, 0x00, 0x00,              // Handle: uint16_t (0 = kServiceRecordHandle)
      0x0A, 0xFE, 0xED, 0xBE, 0xEF,  // Element: uint32_t (0xFEEDBEEF)
      0x08, 0x01,                    // Handle: uint8_t (!) (1 = kServiceClassIdList)
      0x35, 0x03, 0x19, 0x11, 0x01,  // Element: Sequence (3) { UUID(0x1101) }
      0x00                           // No continuation state
  );

  ServiceAttributeResponse resp_wrongtype;
  status = resp_wrongtype.Parse(kInvalidHandleWrongType);

  EXPECT_FALSE(status);

  const auto kInvalidByteCount = CreateStaticByteBuffer(
      0x00, 0x12,  // AttributeListByteCount (18 bytes)
      // Attribute List (only 17 bytes long)
      0x35, 0x0F,                    // Sequence uint8 15 bytes
      0x09, 0x00, 0x01,              // Handle: uint16_t (1)
      0x35, 0x03, 0x19, 0x11, 0x01,  // Element: Sequence (3) { UUID(0x1101) }
      0x09, 0x00, 0x00,              // Handle: uint16_t (0)
      0x25, 0x02, 'h', 'i',          // Element: String ('hi')
      0x00                           // No continuation state
  );

  ServiceAttributeResponse resp4;
  status = resp4.Parse(kInvalidByteCount);
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kPacketMalformed, status.error());

  // Sending too much data eventually fails.
  auto kContinuationPacket = DynamicByteBuffer(4096);
  // Put the correct byte count at the front: 4096 - 2 (byte count) - 5 (continuation size)
  const auto kAttributeListByteCount = CreateStaticByteBuffer(0x0F, 0xF9);
  kContinuationPacket.Write(kAttributeListByteCount, 0);
  // Put the correct continuation at the end.
  const auto kContinuation = CreateStaticByteBuffer(0x04, 0xF0, 0x9F, 0x92, 0x96);
  kContinuationPacket.Write(kContinuation, kContinuationPacket.size() - 5);

  ServiceAttributeResponse resp5;
  status = resp5.Parse(kContinuationPacket);
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kInProgress, status.error());
  while (!status && status.error() == HostError::kInProgress) {
    status = resp5.Parse(kContinuationPacket);
  }
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kNotSupported, status.error());
}

TEST(PDUTest, ServiceAttributeResponseGetPDU_MaxSize) {
  ServiceAttributeResponse resp;

  // Even if set in the wrong order, attributes should be sorted in the PDU.
  resp.set_attribute(0x4000, DataElement(uint16_t{0xfeed}));
  resp.set_attribute(0x4001, DataElement(protocol::kSDP));
  resp.set_attribute(0x4002, DataElement(uint32_t{0xc0decade}));
  DataElement str;
  str.Set(std::string(u8"💖"));
  resp.set_attribute(0x4003, std::move(str));
  resp.set_attribute(0x4005, DataElement(uint32_t{0xC0DEB4BE}));
  resp.set_attribute(kServiceRecordHandle, DataElement(uint32_t{0}));

  const uint16_t kTransactionID = 0xfeed;

  const auto kExpectedContinuation = CreateStaticByteBuffer(
      0x05,                                                  // ServiceAttributeResponse
      UpperBits(kTransactionID), LowerBits(kTransactionID),  // Transaction ID
      0x00, 0x2B,                                            // Param Length (43 bytes)
      0x00, 0x24,                                            // AttributeListsByteCount (36 bytes)
      // AttributeLists
      0x35, 0x2d,                    // Sequence uint8 46 bytes
      0x09, 0x00, 0x00,              // uint16_t (handle) = kServiceRecordHandle
      0x0A, 0x00, 0x00, 0x00, 0x00,  // uint32_t, 0
      0x09, 0x40, 0x00,              // uint16_t (handle) = 0x4000
      0x09, 0xfe, 0xed,              // uint16_t (0xfeed)
      0x09, 0x40, 0x01,              // uint16_t (handle) = 0x4001
      0x19, 0x00, 0x01,              // UUID (kSDP)
      0x09, 0x40, 0x02,              // uint32_t (handle) = 0x4002
      0x0A, 0xc0, 0xde, 0xca, 0xde,  // uint32_t = 0xc0decade
      0x09, 0x40, 0x03,              // uint32_t (handle) = 0x4003
      0x25, 0x04, 0xf0,              // Partial String: '💖' (type, length, first char)
      0x04, 0x00, 0x00, 0x00, 0x24   // Continuation state (4 bytes: 36 bytes offset)
  );

  auto pdu = resp.GetPDU(0xFFFF /* no max */, kTransactionID, kMinMaxSize, BufferView());
  EXPECT_TRUE(ContainersEqual(kExpectedContinuation, *pdu));

  const auto kExpectedRemaining = CreateStaticByteBuffer(
      0x05,                                                  // ServiceSearchAttributeResponse
      UpperBits(kTransactionID), LowerBits(kTransactionID),  // Transaction ID
      0x00, 0x0e,                                            // Param Length (14 bytes)
      0x00, 0x0b,                                            // AttributeListsByteCount (12 bytes)
      // AttributeLists (continued)
      0x9f, 0x92, 0x96,              // Remaining 3 bytes of String: '💖'
      0x09, 0x40, 0x05,              // uint16_t (handle) = 0x4005
      0x0A, 0xc0, 0xde, 0xb4, 0xbe,  // value: uint32_t (0xC0DEB4BE)
      0x00                           // Continutation state (none)
  );

  pdu = resp.GetPDU(0xFFFF /* no max */, kTransactionID, kMinMaxSize,
                    CreateStaticByteBuffer(0x00, 0x00, 0x00, 0x24));
  EXPECT_TRUE(ContainersEqual(kExpectedRemaining, *pdu));
}

TEST(PDUTest, ServiceSearchAttributeRequestParse) {
  const auto kSDPL2CAPAllAttributes = CreateStaticByteBuffer(
      // Service search pattern
      0x35, 0x03,        // Sequence uint8 3 bytes
      0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
      0xF0, 0x0F,        // Maximum attribute byte count: (max = 61455)
      // Attribute ID List
      0x35, 0x05,              // Sequence uint8 5 bytes
      0x0A,                    // uint32_t
      0x00, 0x00, 0xFF, 0xFF,  // Attribute range: 0x000 - 0xFFFF (All of them)
      0x00                     // No continuation state
  );

  ServiceSearchAttributeRequest req(kSDPL2CAPAllAttributes);

  EXPECT_TRUE(req.valid());
  EXPECT_EQ(1u, req.service_search_pattern().size());
  EXPECT_EQ(1u, req.service_search_pattern().count(protocol::kL2CAP));
  EXPECT_EQ(1u, req.attribute_ranges().size());
  EXPECT_EQ(0x0000, req.attribute_ranges().front().start);
  EXPECT_EQ(0xFFFF, req.attribute_ranges().front().end);
  EXPECT_EQ(61455, req.max_attribute_byte_count());

  const auto kContinued =
      CreateStaticByteBuffer(0x35, 0x03,        // Sequence uint8 3 bytes
                             0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                             0x00, 0x0F,        // Maximum attribute byte count: (max = 15)
                             // Attribute ID List
                             0x35, 0x06,             // Sequence uint8 6 bytes
                             0x09,                   // uint16_t
                             0x00, 0x01,             // Attribute ID: 1
                             0x09,                   // uint16_t
                             0x00, 0x02,             // Attribute ID: 2
                             0x03, 0x12, 0x34, 0x56  // Continuation state
      );

  ServiceSearchAttributeRequest req_cont_state(kContinued);

  EXPECT_TRUE(req_cont_state.valid());
  EXPECT_EQ(1u, req.service_search_pattern().size());
  EXPECT_EQ(1u, req.service_search_pattern().count(protocol::kL2CAP));
  EXPECT_EQ(2u, req_cont_state.attribute_ranges().size());
  EXPECT_EQ(0x0001, req_cont_state.attribute_ranges().front().start);
  EXPECT_EQ(0x0001, req_cont_state.attribute_ranges().front().end);
  EXPECT_EQ(0x0002, req_cont_state.attribute_ranges().back().start);
  EXPECT_EQ(0x0002, req_cont_state.attribute_ranges().back().end);
  EXPECT_EQ(15, req_cont_state.max_attribute_byte_count());

  // Too short request.
  ServiceSearchAttributeRequest req_short((BufferView()));

  EXPECT_FALSE(req_short.valid());

  const auto kInvalidMaxBytes =
      CreateStaticByteBuffer(0x35, 0x03,        // Sequence uint8 3 bytes
                             0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                             0x00, 0x04,        // Maximum attribute byte count (4)
                             // Attribute ID List
                             0x35, 0x03,        // Sequence uint8 3 bytes
                             0x09, 0x00, 0x02,  // uint16_t (2)
                             0x00               // No continuation state
      );

  ServiceSearchAttributeRequest req_minmax(kInvalidMaxBytes);

  EXPECT_FALSE(req_minmax.valid());

  // Invalid order of the attributes.
  const auto kInvalidAttributeListOrder =
      CreateStaticByteBuffer(0x35, 0x03,        // Sequence uint8 3 bytes
                             0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                             0xFF, 0xFF,        // Maximum attribute byte count: (max = 65535)
                             // Attribute ID List
                             0x35, 0x06,        // Sequence uint8 6 bytes
                             0x09, 0x00, 0x02,  // uint16_t (2)
                             0x09, 0x00, 0x01,  // uint16_t (1)
                             0x00               // No continuation state
      );

  ServiceSearchAttributeRequest req_order(kInvalidAttributeListOrder);

  EXPECT_FALSE(req_short.valid());

  // AttributeID List has invalid items
  const auto kInvalidAttributeList =
      CreateStaticByteBuffer(0x35, 0x03,        // Sequence uint8 3 bytes
                             0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                             0xFF, 0xFF,        // Maximum attribute byte count: (max = 65535)
                             // Attribute ID List
                             0x35, 0x06,        // Sequence uint8 6 bytes
                             0x09, 0x00, 0x02,  // uint16_t (2)
                             0x19, 0x00, 0x01,  // UUID (0x0001)
                             0x00               // No continuation state
      );

  ServiceSearchAttributeRequest req_baditems(kInvalidAttributeList);

  EXPECT_FALSE(req_baditems.valid());

  const auto kInvalidNoItems =
      CreateStaticByteBuffer(0x35, 0x00,  // Sequence uint8 0 bytes
                             0xF0, 0x0F,  // Maximum attribute byte count (61455)
                             // Attribute ID List
                             0x35, 0x03,        // Sequence uint8 3 bytes
                             0x09, 0x00, 0x02,  // uint16_t (2)
                             0x00               // No continuation state
      );

  ServiceSearchAttributeRequest req_noitems(kInvalidNoItems);
  EXPECT_FALSE(req_noitems.valid());

  const auto kInvalidNoAttributes =
      CreateStaticByteBuffer(0x35, 0x03,        // Sequence uint8 3 bytes
                             0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                             0xF0, 0x0F,        // Maximum attribute byte count (61455)
                             // Attribute ID List
                             0x35, 0x00,  // Sequence uint8 0 bytes
                             0x00         // No continuation state
      );

  ServiceSearchAttributeRequest req_noattributes(kInvalidNoAttributes);
  EXPECT_FALSE(req_noattributes.valid());

  const auto kInvalidTooManyItems = CreateStaticByteBuffer(
      // ServiceSearchPattern
      0x35, 0x27,        // Sequence uint8 27 bytes
      0x19, 0x30, 0x01,  // 13 UUIDs in the search
      0x19, 0x30, 0x02, 0x19, 0x30, 0x03, 0x19, 0x30, 0x04, 0x19, 0x30, 0x05, 0x19, 0x30, 0x06,
      0x19, 0x30, 0x07, 0x19, 0x30, 0x08, 0x19, 0x30, 0x09, 0x19, 0x30, 0x10, 0x19, 0x30, 0x11,
      0x19, 0x30, 0x12, 0x19, 0x30, 0x13, 0xF0, 0x0F,  // Maximum attribute byte count (61455)
      0x35, 0x03,                                      // Sequence uint8 3 bytes
      0x09, 0x00, 0x02,                                // uint16_t (2)
      0x00                                             // No continuation state
  );

  ServiceSearchAttributeRequest req_toomany(kInvalidTooManyItems);
  EXPECT_FALSE(req_toomany.valid());
}

TEST(PDUTest, ServiceSearchAttributeRequestGetPDU) {
  ServiceSearchAttributeRequest req;
  req.AddAttribute(0xF00F);
  req.AddAttributeRange(0x0001, 0xBECA);

  req.set_search_pattern({protocol::kATT, protocol::kL2CAP});
  req.set_max_attribute_byte_count(32);

  const auto kExpected =
      CreateStaticByteBuffer(kServiceSearchAttributeRequest, 0x12, 0x34,  // transaction id
                             0x00, 0x15,  // Parameter Length (21 bytes)
                             // ServiceSearchPattern
                             0x35, 0x06,        // Sequence uint8 6 bytes
                             0x19, 0x00, 0x07,  // UUID (ATT)
                             0x19, 0x01, 0x00,  // UUID (L2CAP)
                             0x00, 0x20,        // MaxAttributeByteCount (32)
                             // Attribute ID list
                             0x35, 0x08,                    // Sequence uint8 8 bytes
                             0x0A, 0x00, 0x01, 0xBE, 0xCA,  // uint32_t (0x0001BECA)
                             0x09, 0xF0, 0x0F,              // uint16_t (0xF00F)
                             0x00                           // No continuation state
      );

  const auto kExpected2 =
      CreateStaticByteBuffer(kServiceSearchAttributeRequest, 0x12, 0x34,  // transaction id
                             0x00, 0x15,  // Parameter Length (21 bytes)
                             // ServiceSearchPattern
                             0x35, 0x06,        // Sequence uint8 6 bytes
                             0x19, 0x01, 0x00,  // UUID (L2CAP)
                             0x19, 0x00, 0x07,  // UUID (ATT)
                             0x00, 0x20,        // MaxAttributeByteCount (32)
                             // Attribute ID list
                             0x35, 0x08,                    // Sequence uint8 8 bytes
                             0x0A, 0x00, 0x01, 0xBE, 0xCA,  // uint32_t (0x0001BECA)
                             0x09, 0xF0, 0x0F,              // uint16_t (0xF00F)
                             0x00                           // No continuation state
      );

  auto pdu = req.GetPDU(0x1234);
  EXPECT_TRUE(MatchesOneOf(kExpected, kExpected2, *pdu));
}

TEST(PDUTest, ServiceSearchAttributeResponseParse) {
  const auto kValidResponseEmpty = CreateStaticByteBuffer(0x00, 0x02,  // AttributeListsByteCount (2
                                                                       // bytes) Attribute List
                                                          0x35, 0x00,  // Sequence uint8 0 bytes
                                                          0x00         // No continuation state
  );

  ServiceSearchAttributeResponse resp;
  auto status = resp.Parse(kValidResponseEmpty);

  EXPECT_TRUE(status);
  EXPECT_TRUE(resp.complete());
  EXPECT_EQ(0u, resp.num_attribute_lists());

  const auto kValidResponseItems = CreateStaticByteBuffer(
      0x00, 0x14,  // AttributeListsByteCount (20 bytes)
      // Wrapping Attribute List
      0x35, 0x12,  // Sequence uint8 18 bytes
      // Attribute List
      0x35, 0x10,                    // Sequence uint8 16 bytes
      0x09, 0x00, 0x00,              // Handle: uint16_t (0 = kServiceRecordHandle)
      0x0A, 0xFE, 0xED, 0xBE, 0xEF,  // Element: uint32_t (0xFEEDBEEF)
      0x09, 0x00, 0x01,              // Handle: uint16_t (1 = kServiceClassIdList)
      0x35, 0x03, 0x19, 0x11, 0x01,  // Element: Sequence (3) { UUID(0x1101) }
      0x00                           // No continuation state
  );

  status = resp.Parse(kValidResponseItems);

  EXPECT_TRUE(status);
  EXPECT_TRUE(resp.complete());
  EXPECT_EQ(1u, resp.num_attribute_lists());
  EXPECT_EQ(2u, resp.attributes(0).size());
  auto it = resp.attributes(0).find(0x0000);
  EXPECT_NE(resp.attributes(0).end(), it);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->second.type());

  it = resp.attributes(0).find(0x0001);
  EXPECT_NE(resp.attributes(0).end(), it);
  EXPECT_EQ(DataElement::Type::kSequence, it->second.type());

  const auto kValidResponseTwoLists = CreateStaticByteBuffer(
      0x00, 0x1E,  // AttributeListsByteCount (30 bytes)
      // Wrapping Attribute List
      0x35, 0x1C,  // Sequence uint8 28 bytes
      // Attribute List 0 (first service)
      0x35, 0x08,                    // Sequence uint8 8 bytes
      0x09, 0x00, 0x00,              // Handle: uint16_t (0 = kServiceRecordHandle)
      0x0A, 0xFE, 0xED, 0xBE, 0xEF,  // Element: uint32_t (0xFEEDBEEF)
      // Attribute List 1 (second service)
      0x35, 0x10,                    // Sequence uint8 16 bytes
      0x09, 0x00, 0x00,              // Handle: uint16_t (0 = kServiceRecordHandle)
      0x0A, 0xFE, 0xDB, 0xAC, 0x01,  // Element: uint32_t (0xFEDBAC01)
      0x09, 0x00, 0x01,              // Handle: uint16_t (1 = kServiceClassIdList)
      0x35, 0x03, 0x19, 0x11, 0x01,  // Element: Sequence (3) { UUID(0x1101) }
      0x00                           // No continuation state
  );

  ServiceSearchAttributeResponse resp2;
  status = resp2.Parse(kValidResponseTwoLists);

  EXPECT_TRUE(status);
  EXPECT_TRUE(resp2.complete());
  EXPECT_EQ(2u, resp2.num_attribute_lists());

  EXPECT_EQ(1u, resp2.attributes(0).size());
  it = resp2.attributes(0).find(0x0000);
  EXPECT_NE(resp2.attributes(0).end(), it);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->second.type());

  EXPECT_EQ(2u, resp2.attributes(1).size());
  it = resp2.attributes(1).find(0x0000);
  EXPECT_NE(resp2.attributes(1).end(), it);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->second.type());

  const auto kInvalidItemsWrongOrder = CreateStaticByteBuffer(
      0x00, 0x14,  // AttributeListByteCount (20 bytes)
      // Wrapping Attribute List
      0x35, 0x12,  // Sequence uint8 18 bytes
      // Attribute List
      0x35, 0x10,                    // Sequence uint8 16 bytes
      0x09, 0x00, 0x01,              // Handle: uint16_t (1 = kServiceClassIdList)
      0x35, 0x03, 0x19, 0x11, 0x01,  // Element: Sequence (3) { UUID(0x1101) }
      0x09, 0x00, 0x00,              // Handle: uint16_t (0 = kServiceRecordHandle)
      0x0A, 0xFE, 0xED, 0xBE, 0xEF,  // Element: uint32_t (0xFEEDBEEF)
      0x00                           // No continuation state
  );

  ServiceSearchAttributeResponse resp3;
  status = resp3.Parse(kInvalidItemsWrongOrder);

  EXPECT_FALSE(status);

  const auto kInvalidHandleWrongType = CreateStaticByteBuffer(
      0x00, 0x13,  // AttributeListByteCount (19 bytes)
      // Wrapping Attribute List
      0x35, 0x11,  // Sequence uint8 17 bytes
      // Attribute List
      0x35, 0x0F,                    // Sequence uint8 15 bytes
      0x09, 0x00, 0x00,              // Handle: uint16_t (0 = kServiceRecordHandle)
      0x0A, 0xFE, 0xED, 0xBE, 0xEF,  // Element: uint32_t (0xFEEDBEEF)
      0x08, 0x01,                    // Handle: uint8_t (!) (1 = kServiceClassIdList)
      0x35, 0x03, 0x19, 0x11, 0x01,  // Element: Sequence (3) { UUID(0x1101) }
      0x00                           // No continuation state
  );

  ServiceAttributeResponse resp_wrongtype;
  status = resp_wrongtype.Parse(kInvalidHandleWrongType);

  EXPECT_FALSE(status);

  const auto kInvalidByteCount = CreateStaticByteBuffer(
      0x00, 0x12,  // AttributeListsByteCount (18 bytes)
      0x35, 0x11,  // Sequence uint8 17 bytes
      // Attribute List
      0x35, 0x0F,                    // Sequence uint8 15 bytes
      0x09, 0x00, 0x01,              // Handle: uint16_t (1)
      0x35, 0x03, 0x19, 0x11, 0x01,  // Element: Sequence (3) { UUID(0x1101) }
      0x09, 0x00, 0x00,              // Handle: uint16_t (0)
      0x25, 0x02, 'h', 'i',          // Element: String ('hi')
      0x00                           // No continuation state
  );

  status = resp3.Parse(kInvalidByteCount);

  EXPECT_FALSE(status);

  // Sending too much data eventually fails.
  auto kContinuationPacket = DynamicByteBuffer(4096);
  // Put the correct byte count at the front: 4096 - 2 (byte count) - 5 (continuation size)
  const auto kAttributeListByteCount = CreateStaticByteBuffer(0x0F, 0xF9);
  kContinuationPacket.Write(kAttributeListByteCount, 0);
  // Put the correct continuation at the end.
  const auto kContinuation = CreateStaticByteBuffer(0x04, 0xF0, 0x9F, 0x92, 0x96);
  kContinuationPacket.Write(kContinuation, kContinuationPacket.size() - 5);

  ServiceSearchAttributeResponse resp4;
  status = resp4.Parse(kContinuationPacket);
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kInProgress, status.error());
  while (!status && status.error() == HostError::kInProgress) {
    status = resp4.Parse(kContinuationPacket);
  }
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kNotSupported, status.error());
}

TEST(PDUTest, ServiceSearchAttributeResponseGetPDU) {
  ServiceSearchAttributeResponse resp;

  // Even if set in the wrong order, attributes should be sorted in the PDU.
  resp.SetAttribute(0, 0x4000, DataElement(uint16_t{0xfeed}));
  resp.SetAttribute(0, 0x4001, DataElement(protocol::kSDP));
  resp.SetAttribute(0, kServiceRecordHandle, DataElement(uint32_t{0}));

  // Attributes do not need to be continuous
  resp.SetAttribute(5, kServiceRecordHandle, DataElement(uint32_t{0x10002000}));

  const uint16_t kTransactionID = 0xfeed;

  const auto kExpected = CreateStaticByteBuffer(
      0x07,                                                  // ServiceSearchAttributeResponse
      UpperBits(kTransactionID), LowerBits(kTransactionID),  // Transaction ID
      0x00, 0x25,                                            // Param Length (37 bytes)
      0x00, 0x22,                                            // AttributeListsByteCount (34 bytes)
      // AttributeLists
      0x35, 0x20,                    // Sequence uint8 32 bytes
      0x35, 0x14,                    // Sequence uint8 20 bytes
      0x09, 0x00, 0x00,              // uint16_t (handle) = kServiceRecordHandle
      0x0A, 0x00, 0x00, 0x00, 0x00,  // uint32_t, 0
      0x09, 0x40, 0x00,              // uint16_t (handle) = 0x4000
      0x09, 0xfe, 0xed,              // uint16_t (0xfeed)
      0x09, 0x40, 0x01,              // uint16_t (handle) = 0x4001
      0x19, 0x00, 0x01,              // UUID (kSDP)
      0x35, 0x08,                    // Sequence uint8 8 bytes
      0x09, 0x00, 0x00,              // uint16_t (handle) = kServiceRecordHandle
      0x0A, 0x10, 0x00, 0x20, 0x00,  // value: uint32_t (0x10002000)
      0x00                           // Continutation state (none)
  );

  auto pdu = resp.GetPDU(0xFFFF /* no max */, kTransactionID, kDefaultMaxSize, BufferView());

  EXPECT_TRUE(ContainersEqual(kExpected, *pdu));
}

TEST(PDUTest, ServiceSearchAttributeResponseGetPDU_MaxSize) {
  ServiceSearchAttributeResponse resp;

  // Even if set in the wrong order, attributes should be sorted in the PDU.
  resp.SetAttribute(0, 0x4000, DataElement(uint16_t{0xfeed}));
  resp.SetAttribute(0, 0x4001, DataElement(protocol::kSDP));
  resp.SetAttribute(0, 0x4002, DataElement(uint32_t{0xc0decade}));
  DataElement str;
  str.Set(std::string(u8"💖"));
  resp.SetAttribute(0, 0x4003, std::move(str));
  resp.SetAttribute(0, kServiceRecordHandle, DataElement(uint32_t{0}));

  // Attributes do not need to be continuous
  resp.SetAttribute(5, kServiceRecordHandle, DataElement(uint32_t{0x10002000}));

  const uint16_t kTransactionID = 0xfeed;

  const auto kExpectedContinuation = CreateStaticByteBuffer(
      0x07,                                                  // ServiceSearchAttributeResponse
      UpperBits(kTransactionID), LowerBits(kTransactionID),  // Transaction ID
      0x00, 0x2B,                                            // Param Length (43 bytes)
      0x00, 0x24,                                            // AttributeListsByteCount (36 bytes)
      // AttributeLists
      0x35, 0x31,                    // Sequence uint8 49 bytes = 37 + 2 + 8 + 2
      0x35, 0x25,                    // Sequence uint8 37 bytes
      0x09, 0x00, 0x00,              // uint16_t (handle) = kServiceRecordHandle
      0x0A, 0x00, 0x00, 0x00, 0x00,  // uint32_t, 0
      0x09, 0x40, 0x00,              // uint16_t (handle) = 0x4000
      0x09, 0xfe, 0xed,              // uint16_t (0xfeed)
      0x09, 0x40, 0x01,              // uint16_t (handle) = 0x4001
      0x19, 0x00, 0x01,              // UUID (kSDP)
      0x09, 0x40, 0x02,              // uint32_t (handle) = 0x4002
      0x0A, 0xc0, 0xde, 0xca, 0xde,  // uint32_t = 0xc0decade
      0x09, 0x40, 0x03,              // uint32_t (handle) = 0x4003
      0x25,                          // Partial String: '💖' (just the type)
      0x04, 0x00, 0x00, 0x00, 0x24   // Continuation state (4 bytes: 36 bytes offset)
  );

  auto pdu = resp.GetPDU(0xFFFF /* no max */, kTransactionID, kMinMaxSize, BufferView());
  EXPECT_TRUE(ContainersEqual(kExpectedContinuation, *pdu));

  const auto kExpectedRemaining = CreateStaticByteBuffer(
      0x07,                                                  // ServiceSearchAttributeResponse
      UpperBits(kTransactionID), LowerBits(kTransactionID),  // Transaction ID
      0x00, 0x12,                                            // Param Length (18 bytes)
      0x00, 0x0F,                                            // AttributeListsByteCount (14 bytes)
      // AttributeLists (continued)
      0x04, 0xf0, 0x9f, 0x92, 0x96,  // Remaining 5 bytes of String: '💖'
      0x35, 0x08,                    // Sequence uint8 8 bytes
      0x09, 0x00, 0x00,              // uint16_t (handle) = kServiceRecordHandle
      0x0A, 0x10, 0x00, 0x20, 0x00,  // value: uint32_t (0x10002000)
      0x00                           // Continutation state (none)
  );

  pdu = resp.GetPDU(0xFFFF /* no max */, kTransactionID, kMinMaxSize,
                    CreateStaticByteBuffer(0x00, 0x00, 0x00, 0x24));
  EXPECT_TRUE(ContainersEqual(kExpectedRemaining, *pdu));
}

TEST(PDUTest, ResponseOutOfRangeContinuation) {
  ServiceSearchResponse rsp_search;
  rsp_search.set_service_record_handle_list({1, 2, 3, 4});
  auto buf = rsp_search.GetPDU(0xFFFF, 0x0110, kDefaultMaxSize, BufferView());
  EXPECT_TRUE(buf);
  // Out of Range (continuation is zero-indexed)
  uint16_t handle_count = htobe16(rsp_search.service_record_handle_list().size());
  auto service_search_cont = DynamicByteBuffer(sizeof(uint16_t));
  service_search_cont.WriteObj(handle_count, 0);
  buf = rsp_search.GetPDU(0xFFFF, 0x0110, kDefaultMaxSize, service_search_cont);
  EXPECT_FALSE(buf);
  // Wrong size continuation state
  buf =
      rsp_search.GetPDU(0xFFFF, 0x0110, kDefaultMaxSize, CreateStaticByteBuffer(0x01, 0xFF, 0xFF));
  EXPECT_FALSE(buf);

  ServiceAttributeResponse rsp_attr;
  rsp_attr.set_attribute(1, DataElement(uint32_t{45}));

  buf = rsp_attr.GetPDU(0xFFFF, 0x0110, kDefaultMaxSize, BufferView());
  EXPECT_TRUE(buf);

  uint32_t rsp_size = htobe32(buf->size() + 5);
  auto too_large_cont = DynamicByteBuffer(sizeof(uint32_t));
  too_large_cont.WriteObj(rsp_size, 0);
  buf = rsp_attr.GetPDU(0xFFFF, 0x0110, kDefaultMaxSize, too_large_cont);

  EXPECT_FALSE(buf);

  ServiceSearchAttributeResponse rsp_search_attr;

  rsp_search_attr.SetAttribute(0, 0x4000, DataElement(uint16_t{0xfeed}));
  rsp_search_attr.SetAttribute(0, 0x4001, DataElement(protocol::kSDP));
  rsp_search_attr.SetAttribute(0, kServiceRecordHandle, DataElement(uint32_t{0}));
  rsp_search_attr.SetAttribute(5, kServiceRecordHandle, DataElement(uint32_t{0x10002000}));

  buf = rsp_search_attr.GetPDU(0xFFFF, 0x0110, kDefaultMaxSize, BufferView());

  EXPECT_TRUE(buf);

  rsp_size = htobe32(buf->size() + 5);
  too_large_cont.WriteObj(rsp_size, 0);
  buf = rsp_attr.GetPDU(0xFFFF, 0x0110, kDefaultMaxSize, too_large_cont);

  EXPECT_FALSE(buf);
}

}  // namespace
}  // namespace bt::sdp
