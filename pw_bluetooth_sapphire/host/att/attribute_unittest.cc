// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attribute.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt::att {
namespace {

constexpr PeerId kTestPeerId(1);
constexpr Handle kTestHandle = 0x0001;
constexpr UUID kTestType1(uint16_t{0x0001});
constexpr UUID kTestType2(uint16_t{0x0002});
constexpr UUID kTestType3(uint16_t{0x0003});
constexpr UUID kTestType4(uint16_t{0x0004});

const auto kTestValue = CreateStaticByteBuffer('t', 'e', 's', 't');

TEST(ATT_AttributeTest, AccessRequirementsDefault) {
  AccessRequirements reqs;
  EXPECT_FALSE(reqs.allowed());
  EXPECT_FALSE(reqs.encryption_required());
  EXPECT_FALSE(reqs.authentication_required());
  EXPECT_FALSE(reqs.authorization_required());
}

TEST(ATT_AttributeTest, AccessRequirements) {
  AccessRequirements reqs0(true, false, false);
  EXPECT_TRUE(reqs0.allowed());
  EXPECT_TRUE(reqs0.encryption_required());
  EXPECT_FALSE(reqs0.authentication_required());
  EXPECT_FALSE(reqs0.authorization_required());
  EXPECT_EQ(reqs0.min_enc_key_size(), sm::kMaxEncryptionKeySize);

  AccessRequirements reqs1(true, false, false, 8);
  EXPECT_TRUE(reqs1.allowed());
  EXPECT_TRUE(reqs1.encryption_required());
  EXPECT_FALSE(reqs1.authentication_required());
  EXPECT_FALSE(reqs1.authorization_required());
  EXPECT_EQ(reqs1.min_enc_key_size(), 8u);

  AccessRequirements reqs2(false, true, false);
  EXPECT_TRUE(reqs2.allowed());
  EXPECT_FALSE(reqs2.encryption_required());
  EXPECT_TRUE(reqs2.authentication_required());
  EXPECT_FALSE(reqs2.authorization_required());

  AccessRequirements reqs3(false, false, true);
  EXPECT_TRUE(reqs3.allowed());
  EXPECT_FALSE(reqs3.encryption_required());
  EXPECT_FALSE(reqs3.authentication_required());
  EXPECT_TRUE(reqs3.authorization_required());

  AccessRequirements reqs4(false, false, false);
  EXPECT_TRUE(reqs4.allowed());
  EXPECT_FALSE(reqs4.encryption_required());
  EXPECT_FALSE(reqs4.authentication_required());
  EXPECT_FALSE(reqs4.authorization_required());
}

TEST(ATT_AttributeTest, GroupingDeclAttr) {
  constexpr size_t kAttrCount = 0u;

  AttributeGrouping group(kTestType1, kTestHandle, kAttrCount, kTestValue);

  // The grouping consists of just the group declaration and is thus complete.
  EXPECT_TRUE(group.complete());
  EXPECT_EQ(kTestType1, group.group_type());
  EXPECT_EQ(kTestHandle, group.start_handle());
  EXPECT_EQ(kTestHandle, group.end_handle());
  EXPECT_EQ(1u, group.attributes().size());

  // The grouping is already complete.
  EXPECT_FALSE(group.AddAttribute(kTestType2, AccessRequirements(), AccessRequirements()));

  const Attribute& decl_attr = group.attributes()[0];
  EXPECT_EQ(kTestHandle, decl_attr.handle());
  EXPECT_EQ(kTestType1, decl_attr.type());
  ASSERT_TRUE(decl_attr.value());
  EXPECT_TRUE(ContainersEqual(kTestValue, *decl_attr.value()));
  EXPECT_TRUE(decl_attr.read_reqs().allowed());
  EXPECT_FALSE(decl_attr.read_reqs().encryption_required());
  EXPECT_FALSE(decl_attr.read_reqs().authentication_required());
  EXPECT_FALSE(decl_attr.read_reqs().authorization_required());
  EXPECT_FALSE(decl_attr.write_reqs().allowed());
  EXPECT_EQ(&group, &decl_attr.group());
}

TEST(ATT_AttributeTest, GroupingAddAttribute) {
  constexpr size_t kAttrCount = 2;

  AttributeGrouping group(kTestType1, kTestHandle, kAttrCount, kTestValue);
  EXPECT_FALSE(group.complete());
  EXPECT_EQ(kTestHandle, group.start_handle());
  EXPECT_EQ(kTestHandle + kAttrCount, group.end_handle());

  Attribute* attr = group.AddAttribute(kTestType2, AccessRequirements(), AccessRequirements());
  ASSERT_TRUE(attr);
  EXPECT_EQ(kTestType2, attr->type());
  EXPECT_EQ(kTestHandle + 1, attr->handle());
  EXPECT_EQ(&group, &attr->group());

  // The attribute should have no value until assigned.
  EXPECT_FALSE(attr->value());
  attr->SetValue(kTestValue);
  ASSERT_TRUE(attr->value());
  EXPECT_TRUE(ContainersEqual(kTestValue, *attr->value()));

  // The group is not complete until |kAttrCount| attributes have been added.
  EXPECT_FALSE(group.complete());
  EXPECT_EQ(2u, group.attributes().size());

  attr = group.AddAttribute(kTestType3, AccessRequirements(), AccessRequirements());
  ASSERT_TRUE(attr);
  EXPECT_EQ(kTestType3, attr->type());

  EXPECT_TRUE(group.complete());
  EXPECT_EQ(group.end_handle(), attr->handle());
  EXPECT_EQ(3u, group.attributes().size());

  EXPECT_FALSE(group.AddAttribute(kTestType4, AccessRequirements(), AccessRequirements()));
}

TEST(ATT_AttributeTest, ReadAsyncReadNotAllowed) {
  AttributeGrouping group(kTestType1, kTestHandle, 1, kTestValue);
  Attribute* attr = group.AddAttribute(kTestType2, AccessRequirements(), AccessRequirements());
  EXPECT_FALSE(attr->ReadAsync(kTestPeerId, 0, [](auto, const auto&) {}));
}

TEST(ATT_AttributeTest, ReadAsyncReadNoHandler) {
  AttributeGrouping group(kTestType1, kTestHandle, 1, kTestValue);
  Attribute* attr =
      group.AddAttribute(kTestType2, AccessRequirements(false, false, false),  // read (no security)
                         AccessRequirements());                                // write not allowed
  EXPECT_FALSE(attr->ReadAsync(kTestPeerId, 0, [](auto, const auto&) {}));
}

TEST(ATT_AttributeTest, ReadAsync) {
  constexpr uint16_t kTestOffset = 5;
  constexpr ErrorCode kTestStatus = ErrorCode::kNoError;

  AttributeGrouping group(kTestType1, kTestHandle, 1, kTestValue);
  Attribute* attr =
      group.AddAttribute(kTestType2, AccessRequirements(false, false, false),  // read (no security)
                         AccessRequirements());                                // write not allowed

  bool callback_called = false;
  auto callback = [&](ErrorCode status, const auto& value) {
    EXPECT_EQ(kTestStatus, status);
    EXPECT_TRUE(ContainersEqual(CreateStaticByteBuffer('h', 'i'), value));
    callback_called = true;
  };

  auto handler = [&](PeerId peer_id, Handle handle, uint16_t offset, const auto& result_cb) {
    EXPECT_EQ(kTestPeerId, peer_id);
    EXPECT_EQ(attr->handle(), handle);
    EXPECT_EQ(kTestOffset, offset);
    EXPECT_TRUE(result_cb);

    result_cb(kTestStatus, CreateStaticByteBuffer('h', 'i'));
  };

  attr->set_read_handler(handler);
  EXPECT_TRUE(attr->ReadAsync(kTestPeerId, kTestOffset, callback));
  EXPECT_TRUE(callback_called);
}

TEST(ATT_AttributeTest, WriteAsyncWriteNotAllowed) {
  AttributeGrouping group(kTestType1, kTestHandle, 1, kTestValue);
  Attribute* attr = group.AddAttribute(kTestType2, AccessRequirements(), AccessRequirements());
  EXPECT_FALSE(attr->WriteAsync(kTestPeerId, 0, BufferView(), [](auto) {}));
}

TEST(ATT_AttributeTest, WriteAsyncWriteNoHandler) {
  AttributeGrouping group(kTestType1, kTestHandle, 1, kTestValue);
  Attribute* attr =
      group.AddAttribute(kTestType2,
                         AccessRequirements(),                      // read not allowed
                         AccessRequirements(false, false, false));  // write no security
  EXPECT_FALSE(attr->WriteAsync(kTestPeerId, 0, BufferView(), [](auto) {}));
}

TEST(ATT_AttributeTest, WriteAsync) {
  constexpr uint16_t kTestOffset = 5;
  constexpr ErrorCode kTestStatus = ErrorCode::kNoError;

  AttributeGrouping group(kTestType1, kTestHandle, 1, kTestValue);
  Attribute* attr =
      group.AddAttribute(kTestType2,
                         AccessRequirements(),                      // read not allowed
                         AccessRequirements(false, false, false));  // write no security

  bool callback_called = false;
  auto callback = [&](ErrorCode status) {
    EXPECT_EQ(kTestStatus, status);
    callback_called = true;
  };

  auto handler = [&](PeerId peer_id, Handle handle, uint16_t offset, const auto& value,
                     const auto& result_cb) {
    EXPECT_EQ(kTestPeerId, peer_id);
    EXPECT_EQ(attr->handle(), handle);
    EXPECT_EQ(kTestOffset, offset);
    EXPECT_TRUE(ContainersEqual(CreateStaticByteBuffer('h', 'i'), value));
    EXPECT_TRUE(result_cb);

    result_cb(kTestStatus);
  };

  attr->set_write_handler(handler);
  EXPECT_TRUE(
      attr->WriteAsync(kTestPeerId, kTestOffset, CreateStaticByteBuffer('h', 'i'), callback));
  EXPECT_TRUE(callback_called);
}

}  // namespace
}  // namespace bt::att
