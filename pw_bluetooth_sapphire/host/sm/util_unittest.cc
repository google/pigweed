// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util.h"

#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {

namespace sm {
namespace util {
namespace {

TEST(SMP_UtilTest, ConvertSmIoCapabilityToHci) {
  EXPECT_EQ(hci::IOCapability::kDisplayOnly,
            IOCapabilityForHci(IOCapability::kDisplayOnly));
  EXPECT_EQ(hci::IOCapability::kDisplayYesNo,
            IOCapabilityForHci(IOCapability::kDisplayYesNo));
  EXPECT_EQ(hci::IOCapability::kKeyboardOnly,
            IOCapabilityForHci(IOCapability::kKeyboardOnly));
  EXPECT_EQ(hci::IOCapability::kNoInputNoOutput,
            IOCapabilityForHci(IOCapability::kNoInputNoOutput));
  EXPECT_EQ(hci::IOCapability::kDisplayYesNo,
            IOCapabilityForHci(IOCapability::kKeyboardDisplay));

  // Test remaining invalid values for sm::IOCapability.
  for (int i = 0x05; i < 0xff; i++) {
    EXPECT_EQ(hci::IOCapability::kNoInputNoOutput,
              IOCapabilityForHci(static_cast<IOCapability>(i)));
  }
}

TEST(SMP_UtilTest, SelectPairingMethodOOB) {
  // In SC OOB is selected if either device has OOB data.
  EXPECT_EQ(PairingMethod::kOutOfBand,
            SelectPairingMethod(true /* sc */, true /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kOutOfBand,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                true /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));
  EXPECT_NE(PairingMethod::kOutOfBand,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));

  // In legacy OOB is selected if both devices have OOB data.
  EXPECT_EQ(PairingMethod::kOutOfBand,
            SelectPairingMethod(false /* sc */, true /* local_oob */,
                                true /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));
  EXPECT_NE(PairingMethod::kOutOfBand,
            SelectPairingMethod(false /* sc */, false /* local_oob */,
                                true /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));
  EXPECT_NE(PairingMethod::kOutOfBand,
            SelectPairingMethod(false /* sc */, true /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));
  EXPECT_NE(PairingMethod::kOutOfBand,
            SelectPairingMethod(false /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));
}

TEST(SMP_UtilTest, SelectPairingMethodNoMITM) {
  // The pairing method should be "Just Works" if neither device requires MITM
  // protection, regardless of other parameters.
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, false /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));

  // Shouldn't default to "Just Works" if at least one device requires MITM
  // protection.
  EXPECT_NE(PairingMethod::kJustWorks,
            SelectPairingMethod(false /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));
}

// Tests all combinations that result in the "Just Works" pairing method.
TEST(SMP_UtilTest, SelectPairingMethodJustWorks) {
  // Local: DisplayOnly
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayOnly /* local */,
                                IOCapability::kDisplayOnly /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayOnly /* local */,
                                IOCapability::kDisplayYesNo /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayOnly /* local */,
                                IOCapability::kNoInputNoOutput /* peer */,
                                true /* local_initiator */));

  // Local: DisplayYesNo
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayYesNo /* local */,
                                IOCapability::kDisplayOnly /* peer */,
                                true /* local_initiator */));
  // If both devices are DisplayYesNo, then "Just Works" is selected for LE
  // legacy pairing (i.e. at least one device doesn't support Secure
  // Connections).
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(false /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayYesNo /* local */,
                                IOCapability::kDisplayYesNo /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayYesNo /* local */,
                                IOCapability::kNoInputNoOutput /* peer */,
                                true /* local_initiator */));

  // Local: KeyboardOnly
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardOnly /* local */,
                                IOCapability::kNoInputNoOutput /* peer */,
                                true /* local_initiator */));

  // Local: NoInputNoOutput. Always "Just Works".
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kNoInputNoOutput /* local */,
                                IOCapability::kDisplayOnly /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kNoInputNoOutput /* local */,
                                IOCapability::kDisplayYesNo /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kNoInputNoOutput /* local */,
                                IOCapability::kKeyboardOnly /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kNoInputNoOutput /* local */,
                                IOCapability::kNoInputNoOutput /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kNoInputNoOutput /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));

  // Local: KeyboardDisplay
  EXPECT_EQ(PairingMethod::kJustWorks,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kNoInputNoOutput /* peer */,
                                true /* local_initiator */));
}

// Tests all combinations that result in the "Passkey Entry (input)" pairing
// method.
TEST(SMP_UtilTest, SelectPairingMethodPasskeyEntryInput) {
  // Local: KeyboardOnly
  EXPECT_EQ(PairingMethod::kPasskeyEntryInput,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardOnly /* local */,
                                IOCapability::kDisplayOnly /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kPasskeyEntryInput,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardOnly /* local */,
                                IOCapability::kDisplayYesNo /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kPasskeyEntryInput,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardOnly /* local */,
                                IOCapability::kKeyboardOnly /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kPasskeyEntryInput,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardOnly /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));

  // Local: KeyboardDisplay
  EXPECT_EQ(PairingMethod::kPasskeyEntryInput,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kDisplayOnly /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kPasskeyEntryInput,
            SelectPairingMethod(false /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kDisplayYesNo /* peer */,
                                true /* local_initiator */));

  // If both devices have the KeyboardDisplay capability then the responder
  // inputs.
  EXPECT_EQ(PairingMethod::kPasskeyEntryInput,
            SelectPairingMethod(false /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                false /* local_initiator */));
}

// Tests all combinations that result in the "Passkey Entry (display)" pairing
// method.
TEST(SMP_UtilTest, SelectPairingMethodPasskeyEntryDisplay) {
  // Local: DisplayOnly
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayOnly /* local */,
                                IOCapability::kKeyboardOnly /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayOnly /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));

  // Local: DisplayYesNo
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayYesNo /* local */,
                                IOCapability::kKeyboardOnly /* peer */,
                                true /* local_initiator */));
  // If the peer has a display then use "Passkey Entry" only for LE Legacy
  // pairing.
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay,
            SelectPairingMethod(false /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayYesNo /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));

  // Local: KeyboardDisplay
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardOnly /* peer */,
                                true /* local_initiator */));

  // If both devices have the KeyboardDisplay capability then the initiator
  // displays.
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay,
            SelectPairingMethod(false /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));
}

// Tests all combinations that result in the "Numeric Comparison" pairing
// method. This will be selected in certain I/O capability combinations only if
// both devices support Secure Connections.
TEST(SMP_UtilTest, SelectPairingMethodNumericComparison) {
  // Local: DisplayYesNo
  EXPECT_EQ(PairingMethod::kNumericComparison,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayYesNo /* local */,
                                IOCapability::kDisplayYesNo /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kNumericComparison,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kDisplayYesNo /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));

  // Local: KeyboardDisplay
  EXPECT_EQ(PairingMethod::kNumericComparison,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kDisplayYesNo /* peer */,
                                true /* local_initiator */));
  EXPECT_EQ(PairingMethod::kNumericComparison,
            SelectPairingMethod(true /* sc */, false /* local_oob */,
                                false /* peer_oob */, true /* mitm */,
                                IOCapability::kKeyboardDisplay /* local */,
                                IOCapability::kKeyboardDisplay /* peer */,
                                true /* local_initiator */));
}

// Tests "c1" using the sample data from Vol 3, Part H, 2.2.3.
TEST(SMP_UtilTest, C1) {
  const UInt128 tk{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
  const UInt128 r{{0xE0, 0x2E, 0x70, 0xC6, 0x4E, 0x27, 0x88, 0x63, 0x0E, 0x6F,
                   0xAD, 0x56, 0x21, 0xD5, 0x83, 0x57}};
  const auto preq =
      CreateStaticByteBuffer(0x01, 0x01, 0x00, 0x00, 0x10, 0x07, 0x07);
  const auto pres =
      CreateStaticByteBuffer(0x02, 0x03, 0x00, 0x00, 0x08, 0x00, 0x05);
  const DeviceAddress initiator_addr(DeviceAddress::Type::kLERandom,
                                     "A1:A2:A3:A4:A5:A6");
  const DeviceAddress responder_addr(DeviceAddress::Type::kLEPublic,
                                     "B1:B2:B3:B4:B5:B6");

  const UInt128 kExpected{{0x86, 0x3B, 0xF1, 0xBE, 0xC5, 0x4D, 0xA7, 0xD2, 0xEA,
                           0x88, 0x89, 0x87, 0xEF, 0x3F, 0x1E, 0x1E}};

  UInt128 result;
  C1(tk, r, preq, pres, initiator_addr, responder_addr, &result);
  EXPECT_TRUE(ContainersEqual(kExpected, result));
}

// Tests "s1" using the sample data from Vol 3, Part H, 2.2.4.
TEST(SMP_UtilTest, S1) {
  const UInt128 tk{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
  const UInt128 r1{{0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x09, 0xA0,
                    0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00}};
  const UInt128 r2{{0x00, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x08, 0x07,
                    0x06, 0x05, 0x04, 0x03, 0x02, 0x01}};

  const UInt128 kExpected{{0x62, 0xA0, 0x6D, 0x79, 0xAE, 0x16, 0x42, 0x5B, 0x9B,
                           0xF4, 0xB0, 0xE8, 0xF0, 0xE1, 0x1F, 0x9A}};

  UInt128 result;
  S1(tk, r1, r2, &result);
  EXPECT_TRUE(ContainersEqual(kExpected, result));
}

// Test "ah" using the sample data from Vol 3, Part H, Appendix D.7.
TEST(SMP_UtilTest, Ah) {
  const UInt128 irk{{0x9B, 0x7D, 0x39, 0x0A, 0xA6, 0x10, 0x10, 0x34, 0x05, 0xAD,
                     0xC8, 0x57, 0xA3, 0x34, 0x02, 0xEC}};
  const uint32_t prand = 0x708194;
  const uint32_t kExpected = 0x0DFBAA;

  EXPECT_EQ(kExpected, Ah(irk, prand));
}

TEST(SMP_UtilTest, IrkCanResolveRpa) {
  // Using the sample data from Vol 3, Part H, Appendix D.7.
  const UInt128 kIRK{{0x9B, 0x7D, 0x39, 0x0A, 0xA6, 0x10, 0x10, 0x34, 0x05,
                      0xAD, 0xC8, 0x57, 0xA3, 0x34, 0x02, 0xEC}};
  const DeviceAddress kStaticRandom(DeviceAddress::Type::kLERandom,
                                    "F0:81:94:0D:FB:A9");
  const DeviceAddress kNonResolvable(DeviceAddress::Type::kLERandom,
                                     "00:81:94:0D:FB:A9");
  const DeviceAddress kNonMatchingResolvable(DeviceAddress::Type::kLERandom,
                                             "70:81:94:0D:FB:A9");
  const DeviceAddress kMatchingResolvable(DeviceAddress::Type::kLERandom,
                                          "70:81:94:0D:FB:AA");

  ASSERT_FALSE(kStaticRandom.IsResolvablePrivate());
  ASSERT_FALSE(kNonResolvable.IsResolvablePrivate());
  ASSERT_TRUE(kNonMatchingResolvable.IsResolvablePrivate());
  ASSERT_TRUE(kMatchingResolvable.IsResolvablePrivate());

  EXPECT_FALSE(IrkCanResolveRpa(kIRK, kStaticRandom));
  EXPECT_FALSE(IrkCanResolveRpa(kIRK, kNonResolvable));
  EXPECT_FALSE(IrkCanResolveRpa(kIRK, kNonMatchingResolvable));
  EXPECT_TRUE(IrkCanResolveRpa(kIRK, kMatchingResolvable));
}

TEST(SMP_UtilTest, GenerateRpa) {
  const UInt128 irk{{'s', 'o', 'm', 'e', ' ', 'r', 'a', 'n', 'd', 'o', 'm', ' ',
                     'd', 'a', 't', 'a'}};

  DeviceAddress rpa = GenerateRpa(irk);

  EXPECT_EQ(DeviceAddress::Type::kLERandom, rpa.type());
  EXPECT_TRUE(rpa.IsResolvablePrivate());

  // It should be possible to resolve the RPA with the IRK used to generate it.
  EXPECT_TRUE(IrkCanResolveRpa(irk, rpa));
}

TEST(SMP_UtilTest, GenerateRandomAddress) {
  DeviceAddress addr = GenerateRandomAddress(false);
  EXPECT_EQ(DeviceAddress::Type::kLERandom, addr.type());
  EXPECT_TRUE(addr.IsNonResolvablePrivate());

  addr = GenerateRandomAddress(true);
  EXPECT_EQ(DeviceAddress::Type::kLERandom, addr.type());
  EXPECT_TRUE(addr.IsStaticRandom());
}

}  // namespace
}  // namespace util
}  // namespace sm
}  // namespace bt
