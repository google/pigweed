// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_gatt_server.h"

#include <endian.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/att/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"

#include "fake_controller.h"
#include "fake_device.h"

namespace bt {
namespace testing {

using common::ByteBuffer;
using common::CreateStaticByteBuffer;
using common::StaticByteBuffer;

FakeGattServer::FakeGattServer(FakeDevice* dev) : dev_(dev) {
  ZX_DEBUG_ASSERT(dev_);
}

void FakeGattServer::HandlePdu(hci::ConnectionHandle conn,
                               const ByteBuffer& pdu) {
  if (pdu.size() < sizeof(att::OpCode)) {
    bt_log(WARN, "fake-hci", "malformed ATT packet!");
    return;
  }

  att::OpCode opcode = le16toh(pdu.As<att::OpCode>());
  switch (opcode) {
    case att::kExchangeMTURequest:
      // Always reply back with the default ATT_MTU.
      Send(conn, CreateStaticByteBuffer(att::kExchangeMTUResponse,
                                        att::kLEMinMTU, 0x00));
      break;
    case att::kReadByGroupTypeRequest:
      HandleReadByGrpType(conn, pdu.view(sizeof(att::OpCode)));
      break;
    default:
      SendErrorRsp(conn, opcode, 0, att::ErrorCode::kRequestNotSupported);
      break;
  }
}

void FakeGattServer::HandleReadByGrpType(hci::ConnectionHandle conn,
                                         const ByteBuffer& bytes) {
  // Don't support 128-bit group types.
  if (bytes.size() != sizeof(att::ReadByGroupTypeRequestParams16)) {
    SendErrorRsp(conn, att::kReadByGroupTypeRequest, 0,
                 att::ErrorCode::kInvalidPDU);
    return;
  }

  const auto& params = bytes.As<att::ReadByGroupTypeRequestParams16>();
  att::Handle start = le16toh(params.start_handle);
  att::Handle end = le16toh(params.end_handle);
  if (!start || end < start) {
    SendErrorRsp(conn, att::kReadByGroupTypeRequest, start,
                 att::ErrorCode::kInvalidHandle);
    return;
  }

  // Only support primary service discovery.
  uint16_t grp_type = le16toh(params.type);
  if (grp_type != gatt::types::kPrimaryService16 || start > 1) {
    SendErrorRsp(conn, att::kReadByGroupTypeRequest, start,
                 att::ErrorCode::kAttributeNotFound);
    return;
  }

  // We report back the standard services.
  // TODO(armansito): Support standard characteristics and more services.
  auto rsp = CreateStaticByteBuffer(att::kReadByGroupTypeResponse,  // opcode
                                    6,           // entry length
                                    0x01, 0x00,  // start handle: 1
                                    0x01, 0x00,  // end handle: 1
                                    0x00, 0x18,  // "GAP Service" UUID: 0x1800
                                    0x02, 0x00,  // start handle: 2
                                    0x02, 0x00,  // end handle: 2
                                    0x01, 0x18   // "GATT Service" UUID: 0x1801
  );
  Send(conn, rsp);
}

void FakeGattServer::Send(hci::ConnectionHandle conn, const ByteBuffer& pdu) {
  if (dev_->ctrl()) {
    dev_->ctrl()->SendL2CAPBFrame(conn, l2cap::kATTChannelId, pdu);
  } else {
    bt_log(WARN, "fake-hci", "no assigned FakeController!");
  }
}

void FakeGattServer::SendErrorRsp(hci::ConnectionHandle conn,
                                  att::OpCode opcode, att::Handle handle,
                                  att::ErrorCode ecode) {
  StaticByteBuffer<sizeof(att::ErrorResponseParams) + sizeof(att::OpCode)>
      buffer;
  att::PacketWriter writer(att::kErrorResponse, &buffer);
  auto* params = writer.mutable_payload<att::ErrorResponseParams>();
  params->request_opcode = htole16(opcode);
  params->attribute_handle = htole16(handle);
  params->error_code = ecode;

  Send(conn, buffer);
}

}  // namespace testing
}  // namespace bt
