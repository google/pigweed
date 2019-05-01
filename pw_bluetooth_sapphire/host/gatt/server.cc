// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "server.h"

#include <lib/fit/function.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/att/database.h"
#include "src/connectivity/bluetooth/core/bt-host/att/permissions.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/lib/fxl/strings/string_printf.h"

#include "gatt_defs.h"

namespace bt {
namespace gatt {

Server::Server(DeviceId peer_id, fxl::RefPtr<att::Database> database,
               fxl::RefPtr<att::Bearer> bearer)
    : peer_id_(peer_id), db_(database), att_(bearer), weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(db_);
  ZX_DEBUG_ASSERT(att_);

  exchange_mtu_id_ = att_->RegisterHandler(
      att::kExchangeMTURequest, fit::bind_member(this, &Server::OnExchangeMTU));
  find_information_id_ =
      att_->RegisterHandler(att::kFindInformationRequest,
                            fit::bind_member(this, &Server::OnFindInformation));
  read_by_group_type_id_ =
      att_->RegisterHandler(att::kReadByGroupTypeRequest,
                            fit::bind_member(this, &Server::OnReadByGroupType));
  read_by_type_id_ = att_->RegisterHandler(
      att::kReadByTypeRequest, fit::bind_member(this, &Server::OnReadByType));
  read_req_id_ = att_->RegisterHandler(
      att::kReadRequest, fit::bind_member(this, &Server::OnReadRequest));
  write_req_id_ = att_->RegisterHandler(
      att::kWriteRequest, fit::bind_member(this, &Server::OnWriteRequest));
  write_cmd_id_ = att_->RegisterHandler(
      att::kWriteCommand, fit::bind_member(this, &Server::OnWriteCommand));
  read_blob_req_id_ =
      att_->RegisterHandler(att::kReadBlobRequest,
                            fit::bind_member(this, &Server::OnReadBlobRequest));
  find_by_type_value_ = att_->RegisterHandler(
      att::kFindByTypeValueRequest,
      fit::bind_member(this, &Server::OnFindByTypeValueRequest));
}

Server::~Server() {
  att_->UnregisterHandler(read_blob_req_id_);
  att_->UnregisterHandler(write_cmd_id_);
  att_->UnregisterHandler(write_req_id_);
  att_->UnregisterHandler(read_req_id_);
  att_->UnregisterHandler(read_by_type_id_);
  att_->UnregisterHandler(read_by_group_type_id_);
  att_->UnregisterHandler(find_information_id_);
  att_->UnregisterHandler(exchange_mtu_id_);
}

void Server::SendNotification(att::Handle handle,
                              const common::ByteBuffer& value, bool indicate) {
  auto buffer = common::NewSlabBuffer(sizeof(att::Header) + sizeof(handle) +
                                      value.size());
  ZX_ASSERT(buffer);

  att::PacketWriter writer(indicate ? att::kIndication : att::kNotification,
                           buffer.get());
  auto rsp_params = writer.mutable_payload<att::AttributeData>();
  rsp_params->handle = htole16(handle);
  writer.mutable_payload_data().Write(value, sizeof(att::AttributeData));

  if (!indicate) {
    att_->SendWithoutResponse(std::move(buffer));
    return;
  }

  att_->StartTransaction(
      std::move(buffer),
      [](const auto&) { bt_log(SPEW, "gatt", "got confirmation!"); },
      [](att::Status status, att::Handle handle) {
        bt_log(TRACE, "gatt", "indication failed (result %s, handle: %#.4x)",
               status.ToString().c_str(), handle);
      });
}

void Server::OnExchangeMTU(att::Bearer::TransactionId tid,
                           const att::PacketReader& packet) {
  ZX_DEBUG_ASSERT(packet.opcode() == att::kExchangeMTURequest);

  if (packet.payload_size() != sizeof(att::ExchangeMTURequestParams)) {
    att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
    return;
  }

  const auto& params = packet.payload<att::ExchangeMTURequestParams>();
  uint16_t client_mtu = le16toh(params.client_rx_mtu);
  uint16_t server_mtu = att_->preferred_mtu();

  auto buffer = common::NewSlabBuffer(sizeof(att::Header) +
                                      sizeof(att::ExchangeMTUResponseParams));
  ZX_ASSERT(buffer);

  att::PacketWriter writer(att::kExchangeMTUResponse, buffer.get());
  auto rsp_params = writer.mutable_payload<att::ExchangeMTUResponseParams>();
  rsp_params->server_rx_mtu = htole16(server_mtu);

  att_->Reply(tid, std::move(buffer));

  // If the minimum value is less than the default MTU, then go with the default
  // MTU (Vol 3, Part F, 3.4.2.2).
  // TODO(armansito): This needs to use on kBREDRMinATTMTU for BR/EDR. Make the
  // default MTU configurable.
  att_->set_mtu(std::max(att::kLEMinMTU, std::min(client_mtu, server_mtu)));
}

void Server::OnFindInformation(att::Bearer::TransactionId tid,
                               const att::PacketReader& packet) {
  ZX_DEBUG_ASSERT(packet.opcode() == att::kFindInformationRequest);

  if (packet.payload_size() != sizeof(att::FindInformationRequestParams)) {
    att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
    return;
  }

  const auto& params = packet.payload<att::FindInformationRequestParams>();
  att::Handle start = le16toh(params.start_handle);
  att::Handle end = le16toh(params.end_handle);

  constexpr size_t kRspStructSize = sizeof(att::FindInformationResponseParams);
  constexpr size_t kHeaderSize = sizeof(att::Header) + kRspStructSize;
  ZX_DEBUG_ASSERT(kHeaderSize <= att_->mtu());

  if (start == att::kInvalidHandle || start > end) {
    att_->ReplyWithError(tid, start, att::ErrorCode::kInvalidHandle);
    return;
  }

  // Find all attributes within range with the same compact UUID size that can
  // fit within the current MTU.
  size_t max_payload_size = att_->mtu() - kHeaderSize;
  size_t uuid_size;
  size_t entry_size;
  std::list<const att::Attribute*> results;
  for (auto it = db_->GetIterator(start, end); !it.AtEnd(); it.Advance()) {
    const auto* attr = it.get();
    ZX_DEBUG_ASSERT(attr);

    // GATT does not allow 32-bit UUIDs
    size_t compact_size = attr->type().CompactSize(false /* allow_32bit */);
    if (results.empty()) {
      // |uuid_size| is determined by the first attribute.
      uuid_size = compact_size;
      entry_size = std::min(uuid_size + sizeof(att::Handle), max_payload_size);
    } else if (compact_size != uuid_size || entry_size > max_payload_size) {
      break;
    }

    results.push_back(attr);
    max_payload_size -= entry_size;
  }

  if (results.empty()) {
    att_->ReplyWithError(tid, start, att::ErrorCode::kAttributeNotFound);
    return;
  }

  ZX_DEBUG_ASSERT(!results.empty());

  size_t pdu_size = kHeaderSize + entry_size * results.size();

  auto buffer = common::NewSlabBuffer(pdu_size);
  ZX_ASSERT(buffer);

  att::PacketWriter writer(att::kFindInformationResponse, buffer.get());
  auto rsp_params =
      writer.mutable_payload<att::FindInformationResponseParams>();
  rsp_params->format =
      (entry_size == 4) ? att::UUIDType::k16Bit : att::UUIDType::k128Bit;

  // |out_entries| initially references |params->information_data|. The loop
  // below modifies it as entries are written into the list.
  auto out_entries = writer.mutable_payload_data().mutable_view(kRspStructSize);
  for (const auto& attr : results) {
    *reinterpret_cast<att::Handle*>(out_entries.mutable_data()) =
        htole16(attr->handle());
    auto uuid_view = out_entries.mutable_view(sizeof(att::Handle));
    attr->type().ToBytes(&uuid_view, false /* allow32_bit */);

    // advance
    out_entries = out_entries.mutable_view(entry_size);
  }

  att_->Reply(tid, std::move(buffer));
}

void Server::OnFindByTypeValueRequest(att::Bearer::TransactionId tid,
                                      const att::PacketReader& packet) {
  ZX_DEBUG_ASSERT(packet.opcode() == att::kFindByTypeValueRequest);

  if (packet.payload_size() < sizeof(att::FindByTypeValueRequestParams)) {
    att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
    return;
  }

  const auto& params = packet.payload<att::FindByTypeValueRequestParams>();
  att::Handle start = le16toh(params.start_handle);
  att::Handle end = le16toh(params.end_handle);
  common::UUID type = common::UUID(params.type);
  constexpr size_t kParamsSize = sizeof(att::FindByTypeValueRequestParams);

  common::BufferView value = packet.payload_data().view(
      kParamsSize, packet.payload_size() - kParamsSize);

  if (start == att::kInvalidHandle || start > end) {
    att_->ReplyWithError(tid, att::kInvalidHandle,
                         att::ErrorCode::kInvalidHandle);
    return;
  }

  auto iter = db_->GetIterator(start, end, &type, false);
  if (iter.AtEnd()) {
    att_->ReplyWithError(tid, att::kInvalidHandle,
                         att::ErrorCode::kAttributeNotFound);
    return;
  }

  std::list<const att::Attribute*> results;

  // Filter for identical values
  for (; !iter.AtEnd(); iter.Advance()) {
    const auto* attr = iter.get();
    ZX_DEBUG_ASSERT(attr);

    // Only support static values for this Request type
    if (attr->value()) {
      if (*attr->value() == value) {
        results.push_back(attr);
      }
    }
  }

  // No attributes match the value
  if (results.size() == 0) {
    att_->ReplyWithError(tid, att::kInvalidHandle,
                         att::ErrorCode::kAttributeNotFound);
    return;
  }

  constexpr size_t kRspStructSize = sizeof(att::HandlesInformationList);
  size_t pdu_size = sizeof(att::Header) + kRspStructSize * results.size();
  auto buffer = common::NewSlabBuffer(pdu_size);
  ZX_ASSERT(buffer);

  att::PacketWriter writer(att::kFindByTypeValueResponse, buffer.get());

  // Points to the next entry in the target PDU.
  auto next_entry = writer.mutable_payload_data();
  for (const auto& attr : results) {
    auto* entry = reinterpret_cast<att::HandlesInformationList*>(
        next_entry.mutable_data());
    entry->handle = htole16(attr->handle());
    if (attr->group().active()) {
      entry->group_end_handle = htole16(attr->group().end_handle());
    } else {
      entry->group_end_handle = htole16(attr->handle());
    }
    next_entry = next_entry.mutable_view(kRspStructSize);
  }

  att_->Reply(tid, std::move(buffer));
}

void Server::OnReadByGroupType(att::Bearer::TransactionId tid,
                               const att::PacketReader& packet) {
  ZX_DEBUG_ASSERT(packet.opcode() == att::kReadByGroupTypeRequest);

  att::Handle start, end;
  common::UUID group_type;

  // The group type is represented as either a 16-bit or 128-bit UUID.
  if (packet.payload_size() == sizeof(att::ReadByTypeRequestParams16)) {
    const auto& params = packet.payload<att::ReadByTypeRequestParams16>();
    start = le16toh(params.start_handle);
    end = le16toh(params.end_handle);
    group_type = common::UUID(le16toh(params.type));
  } else if (packet.payload_size() == sizeof(att::ReadByTypeRequestParams128)) {
    const auto& params = packet.payload<att::ReadByTypeRequestParams128>();
    start = le16toh(params.start_handle);
    end = le16toh(params.end_handle);
    group_type = common::UUID(params.type);
  } else {
    att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
    return;
  }

  if (group_type != types::kPrimaryService &&
      group_type != types::kSecondaryService) {
    att_->ReplyWithError(tid, start, att::ErrorCode::kUnsupportedGroupType);
    return;
  }

  constexpr size_t kRspStructSize = sizeof(att::ReadByGroupTypeResponseParams);
  constexpr size_t kHeaderSize = sizeof(att::Header) + kRspStructSize;
  ZX_DEBUG_ASSERT(kHeaderSize <= att_->mtu());

  size_t value_size;
  std::list<const att::Attribute*> results;
  auto error_code = ReadByTypeHelper(
      start, end, group_type, true /* group_type */, att_->mtu() - kHeaderSize,
      att::kMaxReadByGroupTypeValueLength, sizeof(att::AttributeGroupDataEntry),
      &value_size, &results);
  if (error_code != att::ErrorCode::kNoError) {
    att_->ReplyWithError(tid, start, error_code);
    return;
  }

  ZX_DEBUG_ASSERT(!results.empty());

  size_t entry_size = value_size + sizeof(att::AttributeGroupDataEntry);
  size_t pdu_size = kHeaderSize + entry_size * results.size();
  ZX_DEBUG_ASSERT(pdu_size <= att_->mtu());

  auto buffer = common::NewSlabBuffer(pdu_size);
  ZX_ASSERT(buffer);

  att::PacketWriter writer(att::kReadByGroupTypeResponse, buffer.get());
  auto params = writer.mutable_payload<att::ReadByGroupTypeResponseParams>();

  ZX_DEBUG_ASSERT(entry_size <= std::numeric_limits<uint8_t>::max());
  params->length = static_cast<uint8_t>(entry_size);

  // Points to the next entry in the target PDU.
  auto next_entry = writer.mutable_payload_data().mutable_view(kRspStructSize);
  for (const auto& attr : results) {
    auto* entry = reinterpret_cast<att::AttributeGroupDataEntry*>(
        next_entry.mutable_data());
    entry->start_handle = htole16(attr->group().start_handle());
    entry->group_end_handle = htole16(attr->group().end_handle());
    next_entry.Write(attr->group().decl_value().view(0, value_size),
                     sizeof(att::AttributeGroupDataEntry));

    next_entry = next_entry.mutable_view(entry_size);
  }

  att_->Reply(tid, std::move(buffer));
}

void Server::OnReadByType(att::Bearer::TransactionId tid,
                          const att::PacketReader& packet) {
  ZX_DEBUG_ASSERT(packet.opcode() == att::kReadByTypeRequest);

  att::Handle start, end;
  common::UUID type;

  // The attribute type is represented as either a 16-bit or 128-bit UUID.
  if (packet.payload_size() == sizeof(att::ReadByTypeRequestParams16)) {
    const auto& params = packet.payload<att::ReadByTypeRequestParams16>();
    start = le16toh(params.start_handle);
    end = le16toh(params.end_handle);
    type = common::UUID(le16toh(params.type));
  } else if (packet.payload_size() == sizeof(att::ReadByTypeRequestParams128)) {
    const auto& params = packet.payload<att::ReadByTypeRequestParams128>();
    start = le16toh(params.start_handle);
    end = le16toh(params.end_handle);
    type = common::UUID(params.type);
  } else {
    att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
    return;
  }

  constexpr size_t kRspStructSize = sizeof(att::ReadByTypeResponseParams);
  constexpr size_t kHeaderSize = sizeof(att::Header) + kRspStructSize;
  ZX_DEBUG_ASSERT(kHeaderSize <= att_->mtu());

  size_t value_size;
  std::list<const att::Attribute*> results;
  auto error_code = ReadByTypeHelper(
      start, end, type, false /* group_type */, att_->mtu() - kHeaderSize,
      att::kMaxReadByTypeValueLength, sizeof(att::AttributeData), &value_size,
      &results);
  if (error_code != att::ErrorCode::kNoError) {
    att_->ReplyWithError(tid, start, error_code);
    return;
  }

  ZX_DEBUG_ASSERT(!results.empty());

  // If the value is dynamic, then delegate the read to any registered handler.
  if (!results.front()->value()) {
    ZX_DEBUG_ASSERT(results.size() == 1u);

    const size_t kMaxValueSize =
        std::min(att_->mtu() - kHeaderSize - sizeof(att::AttributeData),
                 static_cast<size_t>(att::kMaxReadByTypeValueLength));

    att::Handle handle = results.front()->handle();
    auto self = weak_ptr_factory_.GetWeakPtr();
    auto result_cb = [self, tid, handle, kMaxValueSize, kHeaderSize](
                         att::ErrorCode ecode, const auto& value) {
      if (!self)
        return;

      if (ecode != att::ErrorCode::kNoError) {
        self->att_->ReplyWithError(tid, handle, ecode);
        return;
      }

      // Respond with just a single entry.
      size_t value_size = std::min(value.size(), kMaxValueSize);
      size_t entry_size = value_size + sizeof(att::AttributeData);
      auto buffer = common::NewSlabBuffer(entry_size + kHeaderSize);
      att::PacketWriter writer(att::kReadByTypeResponse, buffer.get());

      auto params = writer.mutable_payload<att::ReadByTypeResponseParams>();
      params->length = static_cast<uint8_t>(entry_size);
      params->attribute_data_list->handle = htole16(handle);
      writer.mutable_payload_data().Write(
          value.data(), value_size, sizeof(params->length) + sizeof(handle));

      self->att_->Reply(tid, std::move(buffer));
    };

    // Respond with an error if no read handler was registered.
    if (!results.front()->ReadAsync(peer_id_, 0, result_cb)) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kReadNotPermitted);
    }
    return;
  }

  size_t entry_size = sizeof(att::AttributeData) + value_size;
  ZX_DEBUG_ASSERT(entry_size <= std::numeric_limits<uint8_t>::max());

  size_t pdu_size = kHeaderSize + entry_size * results.size();
  ZX_DEBUG_ASSERT(pdu_size <= att_->mtu());

  auto buffer = common::NewSlabBuffer(pdu_size);
  ZX_ASSERT(buffer);

  att::PacketWriter writer(att::kReadByTypeResponse, buffer.get());
  auto params = writer.mutable_payload<att::ReadByTypeResponseParams>();
  params->length = static_cast<uint8_t>(entry_size);

  // Points to the next entry in the target PDU.
  auto next_entry = writer.mutable_payload_data().mutable_view(kRspStructSize);
  for (const auto& attr : results) {
    auto* entry =
        reinterpret_cast<att::AttributeData*>(next_entry.mutable_data());
    entry->handle = htole16(attr->handle());
    next_entry.Write(attr->value()->view(0, value_size), sizeof(entry->handle));

    next_entry = next_entry.mutable_view(entry_size);
  }

  att_->Reply(tid, std::move(buffer));
}

void Server::OnReadBlobRequest(att::Bearer::TransactionId tid,
                               const att::PacketReader& packet) {
  ZX_DEBUG_ASSERT(packet.opcode() == att::kReadBlobRequest);

  if (packet.payload_size() != sizeof(att::ReadBlobRequestParams)) {
    att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
    return;
  }

  const auto& params = packet.payload<att::ReadBlobRequestParams>();
  att::Handle handle = le16toh(params.handle);
  uint16_t offset = le16toh(params.offset);

  const auto* attr = db_->FindAttribute(handle);
  if (!attr) {
    att_->ReplyWithError(tid, handle, att::ErrorCode::kInvalidHandle);
    return;
  }

  att::ErrorCode ecode =
      att::CheckReadPermissions(attr->read_reqs(), att_->security());
  if (ecode != att::ErrorCode::kNoError) {
    att_->ReplyWithError(tid, handle, ecode);
    return;
  }

  constexpr size_t kHeaderSize = sizeof(att::Header);

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto callback = [self, tid, offset, handle](att::ErrorCode ecode,
                                              const auto& value) {
    if (!self)
      return;

    if (ecode != att::ErrorCode::kNoError) {
      self->att_->ReplyWithError(tid, handle, ecode);
      return;
    }

    size_t value_size = std::min(value.size(), self->att_->mtu() - kHeaderSize);
    auto buffer = common::NewSlabBuffer(value_size + kHeaderSize);
    ZX_ASSERT(buffer);

    att::PacketWriter writer(att::kReadBlobResponse, buffer.get());
    writer.mutable_payload_data().Write(value.view(0, value_size));

    self->att_->Reply(tid, std::move(buffer));
  };

  // Use the cached value if there is one.
  if (attr->value()) {
    if (offset >= attr->value()->size()) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kInvalidOffset);
      return;
    }
    size_t value_size =
        std::min(attr->value()->size(), self->att_->mtu() - kHeaderSize);
    callback(att::ErrorCode::kNoError, attr->value()->view(offset, value_size));
    return;
  }

  // TODO(bwb): Add a timeout to this as per NET-434
  if (!attr->ReadAsync(peer_id_, offset, callback)) {
    att_->ReplyWithError(tid, handle, att::ErrorCode::kReadNotPermitted);
  }
}

void Server::OnReadRequest(att::Bearer::TransactionId tid,
                           const att::PacketReader& packet) {
  ZX_DEBUG_ASSERT(packet.opcode() == att::kReadRequest);

  if (packet.payload_size() != sizeof(att::ReadRequestParams)) {
    att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
    return;
  }

  const auto& params = packet.payload<att::WriteRequestParams>();
  att::Handle handle = le16toh(params.handle);

  const auto* attr = db_->FindAttribute(handle);
  if (!attr) {
    att_->ReplyWithError(tid, handle, att::ErrorCode::kInvalidHandle);
    return;
  }

  att::ErrorCode ecode =
      att::CheckReadPermissions(attr->read_reqs(), att_->security());
  if (ecode != att::ErrorCode::kNoError) {
    att_->ReplyWithError(tid, handle, ecode);
    return;
  }

  constexpr size_t kHeaderSize = sizeof(att::Header);

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto callback = [self, tid, handle](att::ErrorCode ecode, const auto& value) {
    if (!self)
      return;

    if (ecode != att::ErrorCode::kNoError) {
      self->att_->ReplyWithError(tid, handle, ecode);
      return;
    }

    size_t value_size = std::min(value.size(), self->att_->mtu() - kHeaderSize);
    auto buffer = common::NewSlabBuffer(value_size + kHeaderSize);
    ZX_ASSERT(buffer);

    att::PacketWriter writer(att::kReadResponse, buffer.get());
    writer.mutable_payload_data().Write(value.view(0, value_size));

    self->att_->Reply(tid, std::move(buffer));
  };

  // Use the cached value if there is one.
  if (attr->value()) {
    callback(att::ErrorCode::kNoError, *attr->value());
    return;
  }

  if (!attr->ReadAsync(peer_id_, 0, callback)) {
    att_->ReplyWithError(tid, handle, att::ErrorCode::kReadNotPermitted);
  }
}

void Server::OnWriteCommand(att::Bearer::TransactionId tid,
                            const att::PacketReader& packet) {
  ZX_DEBUG_ASSERT(packet.opcode() == att::kWriteCommand);

  if (packet.payload_size() < sizeof(att::WriteRequestParams)) {
    // Ignore if wrong size, no response allowed
    return;
  }

  const auto& params = packet.payload<att::WriteRequestParams>();
  att::Handle handle = le16toh(params.handle);
  const auto* attr = db_->FindAttribute(handle);

  // Attributes can be invalid if the handle is invalid
  if (!attr) {
    return;
  }

  att::ErrorCode ecode =
      att::CheckWritePermissions(attr->write_reqs(), att_->security());
  if (ecode != att::ErrorCode::kNoError) {
    return;
  }

  // Attributes with a static value cannot be written.
  if (attr->value()) {
    return;
  }

  auto value_view = packet.payload_data().view(sizeof(params.handle));
  if (value_view.size() > att::kMaxAttributeValueLength) {
    return;
  }

  // No response allowed for commands, ignore the cb
  attr->WriteAsync(peer_id_, 0, value_view, nullptr);
}

void Server::OnWriteRequest(att::Bearer::TransactionId tid,
                            const att::PacketReader& packet) {
  ZX_DEBUG_ASSERT(packet.opcode() == att::kWriteRequest);

  if (packet.payload_size() < sizeof(att::WriteRequestParams)) {
    att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
    return;
  }

  const auto& params = packet.payload<att::WriteRequestParams>();
  att::Handle handle = le16toh(params.handle);

  const auto* attr = db_->FindAttribute(handle);
  if (!attr) {
    att_->ReplyWithError(tid, handle, att::ErrorCode::kInvalidHandle);
    return;
  }

  att::ErrorCode ecode =
      att::CheckWritePermissions(attr->write_reqs(), att_->security());
  if (ecode != att::ErrorCode::kNoError) {
    att_->ReplyWithError(tid, handle, ecode);
    return;
  }

  // Attributes with a static value cannot be written.
  if (attr->value()) {
    att_->ReplyWithError(tid, handle, att::ErrorCode::kWriteNotPermitted);
    return;
  }

  auto value_view = packet.payload_data().view(sizeof(params.handle));
  if (value_view.size() > att::kMaxAttributeValueLength) {
    att_->ReplyWithError(tid, handle,
                         att::ErrorCode::kInvalidAttributeValueLength);
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto result_cb = [self, tid, handle](att::ErrorCode error_code) {
    if (!self)
      return;

    if (error_code != att::ErrorCode::kNoError) {
      self->att_->ReplyWithError(tid, handle, error_code);
      return;
    }

    auto buffer = common::NewSlabBuffer(1);
    (*buffer)[0] = att::kWriteResponse;
    self->att_->Reply(tid, std::move(buffer));
  };

  if (!attr->WriteAsync(peer_id_, 0, value_view, result_cb)) {
    att_->ReplyWithError(tid, handle, att::ErrorCode::kWriteNotPermitted);
  }
}

att::ErrorCode Server::ReadByTypeHelper(
    att::Handle start, att::Handle end, const common::UUID& type,
    bool group_type, size_t max_data_list_size, size_t max_value_size,
    size_t entry_prefix_size, size_t* out_value_size,
    std::list<const att::Attribute*>* out_results) {
  ZX_DEBUG_ASSERT(out_results);
  ZX_DEBUG_ASSERT(out_value_size);

  if (start == att::kInvalidHandle || start > end)
    return att::ErrorCode::kInvalidHandle;

  auto iter = db_->GetIterator(start, end, &type, group_type);
  if (iter.AtEnd())
    return att::ErrorCode::kAttributeNotFound;

  // |value_size| is the size of the complete attribute value for each result
  // entry. |entry_size| = |value_size| + |entry_prefix_size|. We store these
  // separately to avoid recalculating one every it gets checked.
  size_t value_size;
  size_t entry_size;
  std::list<const att::Attribute*> results;

  for (; !iter.AtEnd(); iter.Advance()) {
    const auto* attr = iter.get();
    ZX_DEBUG_ASSERT(attr);

    att::ErrorCode security_result =
        att::CheckReadPermissions(attr->read_reqs(), att_->security());
    if (security_result != att::ErrorCode::kNoError) {
      // Return error only if this is the first result that matched. We simply
      // stop the search otherwise.
      if (results.empty()) {
        return security_result;
      }
      break;
    }

    // The first result determines |value_size| and |entry_size|.
    if (results.empty()) {
      if (!attr->value()) {
        // If the first value is dynamic then this is the only attribute that
        // this call will return. No need to calculate the value size.
        results.push_back(attr);
        break;
      }

      value_size = attr->value()->size();  // untruncated value size
      entry_size =
          std::min(std::min(value_size, max_value_size) + entry_prefix_size,
                   max_data_list_size);

      // Actual value size to include in a PDU.
      *out_value_size = entry_size - entry_prefix_size;

    } else if (!attr->value() || attr->value()->size() != value_size ||
               entry_size > max_data_list_size) {
      // Stop the search and exclude this attribute because either:
      // a. we ran into a dynamic value in a result that contains static values,
      // b. the matching attribute has a different value size than the first
      //    attribute,
      // c. there is no remaning space in the response PDU.
      break;
    }

    results.push_back(attr);
    max_data_list_size -= entry_size;
  }

  if (results.empty())
    return att::ErrorCode::kAttributeNotFound;

  *out_results = std::move(results);
  return att::ErrorCode::kNoError;
}

}  // namespace gatt
}  // namespace bt
