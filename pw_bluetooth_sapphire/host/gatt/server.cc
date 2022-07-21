// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "server.h"

#include <lib/fit/function.h>
#include <lib/trace/event.h>
#include <zircon/assert.h>

#include "gatt_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/att/database.h"
#include "src/connectivity/bluetooth/core/bt-host/att/permissions.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"

namespace bt::gatt {

class AttBasedServer final : public Server {
 public:
  AttBasedServer(PeerId peer_id, fxl::WeakPtr<LocalServiceManager> local_services,
                 fxl::WeakPtr<att::Bearer> bearer)
      : peer_id_(peer_id),
        local_services_(std::move(local_services)),
        att_(std::move(bearer)),
        weak_ptr_factory_(this) {
    ZX_ASSERT(local_services_);
    ZX_DEBUG_ASSERT(att_);

    exchange_mtu_id_ = att_->RegisterHandler(
        att::kExchangeMTURequest, fit::bind_member<&AttBasedServer::OnExchangeMTU>(this));
    find_information_id_ = att_->RegisterHandler(
        att::kFindInformationRequest, fit::bind_member<&AttBasedServer::OnFindInformation>(this));
    read_by_group_type_id_ = att_->RegisterHandler(
        att::kReadByGroupTypeRequest, fit::bind_member<&AttBasedServer::OnReadByGroupType>(this));
    read_by_type_id_ = att_->RegisterHandler(att::kReadByTypeRequest,
                                             fit::bind_member<&AttBasedServer::OnReadByType>(this));
    read_req_id_ = att_->RegisterHandler(att::kReadRequest,
                                         fit::bind_member<&AttBasedServer::OnReadRequest>(this));
    write_req_id_ = att_->RegisterHandler(att::kWriteRequest,
                                          fit::bind_member<&AttBasedServer::OnWriteRequest>(this));
    write_cmd_id_ = att_->RegisterHandler(att::kWriteCommand,
                                          fit::bind_member<&AttBasedServer::OnWriteCommand>(this));
    read_blob_req_id_ = att_->RegisterHandler(
        att::kReadBlobRequest, fit::bind_member<&AttBasedServer::OnReadBlobRequest>(this));
    find_by_type_value_id_ =
        att_->RegisterHandler(att::kFindByTypeValueRequest,
                              fit::bind_member<&AttBasedServer::OnFindByTypeValueRequest>(this));
    prepare_write_id_ = att_->RegisterHandler(
        att::kPrepareWriteRequest, fit::bind_member<&AttBasedServer::OnPrepareWriteRequest>(this));
    exec_write_id_ = att_->RegisterHandler(
        att::kExecuteWriteRequest, fit::bind_member<&AttBasedServer::OnExecuteWriteRequest>(this));
  }

  ~AttBasedServer() override {
    att_->UnregisterHandler(exec_write_id_);
    att_->UnregisterHandler(prepare_write_id_);
    att_->UnregisterHandler(find_by_type_value_id_);
    att_->UnregisterHandler(read_blob_req_id_);
    att_->UnregisterHandler(write_cmd_id_);
    att_->UnregisterHandler(write_req_id_);
    att_->UnregisterHandler(read_req_id_);
    att_->UnregisterHandler(read_by_type_id_);
    att_->UnregisterHandler(read_by_group_type_id_);
    att_->UnregisterHandler(find_information_id_);
    att_->UnregisterHandler(exchange_mtu_id_);
  }

 private:
  // Convenience "alias"
  inline fxl::WeakPtr<att::Database> db() { return local_services_->database(); }

  // Server overrides:
  void SendUpdate(IdType service_id, IdType chrc_id, BufferView value,
                  IndicationCallback indicate_cb) override {
    auto buffer = NewSlabBuffer(sizeof(att::Header) + sizeof(att::Handle) + value.size());
    ZX_ASSERT(buffer);

    LocalServiceManager::ClientCharacteristicConfig config;
    if (!local_services_->GetCharacteristicConfig(service_id, chrc_id, peer_id_, &config)) {
      bt_log(TRACE, "gatt", "peer has not configured characteristic: %s", bt_str(peer_id_));
      if (indicate_cb) {
        indicate_cb(ToResult(HostError::kNotSupported));
      }
      return;
    }

    // Make sure that the client has subscribed to the requested protocol method.
    if ((indicate_cb && !config.indicate) || (!indicate_cb && !config.notify)) {
      bt_log(TRACE, "gatt", "peer has not enabled (%s): %s",
             (indicate_cb ? "indications" : "notifications"), bt_str(peer_id_));
      if (indicate_cb) {
        indicate_cb(ToResult(HostError::kNotSupported));
      }
      return;
    }

    att::PacketWriter writer(indicate_cb ? att::kIndication : att::kNotification, buffer.get());
    auto rsp_params = writer.mutable_payload<att::AttributeData>();
    rsp_params->handle = htole16(config.handle);
    writer.mutable_payload_data().Write(value, sizeof(att::AttributeData));

    if (!indicate_cb) {
      [[maybe_unused]] bool _ = att_->SendWithoutResponse(std::move(buffer));
      return;
    }
    auto transaction_cb = [indicate_cb = std::move(indicate_cb)](
                              att::Bearer::TransactionResult result) mutable {
      if (result.is_ok()) {
        bt_log(DEBUG, "gatt", "got indication ACK");
        indicate_cb(fitx::ok());
      } else {
        const auto& [error, handle] = result.error_value();
        bt_log(WARN, "gatt", "indication failed (error %s, handle: %#.4x)", bt_str(error), handle);
        indicate_cb(fitx::error(error));
      }
    };
    att_->StartTransaction(std::move(buffer), std::move(transaction_cb));
  }

  void ShutDown() override { att_->ShutDown(); }

  // ATT protocol request handlers:
  void OnExchangeMTU(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kExchangeMTURequest);

    if (packet.payload_size() != sizeof(att::ExchangeMTURequestParams)) {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
      return;
    }

    const auto& params = packet.payload<att::ExchangeMTURequestParams>();
    uint16_t client_mtu = le16toh(params.client_rx_mtu);
    uint16_t server_mtu = att_->preferred_mtu();

    auto buffer = NewSlabBuffer(sizeof(att::Header) + sizeof(att::ExchangeMTUResponseParams));
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

  void OnFindInformation(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kFindInformationRequest);
    TRACE_DURATION("bluetooth", "gatt::Server::OnFindInformation");

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
    for (auto it = db()->GetIterator(start, end); !it.AtEnd(); it.Advance()) {
      const auto* attr = it.get();
      ZX_DEBUG_ASSERT(attr);

      // GATT does not allow 32-bit UUIDs
      size_t compact_size = attr->type().CompactSize(/*allow_32bit=*/false);
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

    auto buffer = NewSlabBuffer(pdu_size);
    ZX_ASSERT(buffer);

    att::PacketWriter writer(att::kFindInformationResponse, buffer.get());
    auto rsp_params = writer.mutable_payload<att::FindInformationResponseParams>();
    rsp_params->format = (entry_size == 4) ? att::UUIDType::k16Bit : att::UUIDType::k128Bit;

    // |out_entries| initially references |params->information_data|. The loop
    // below modifies it as entries are written into the list.
    auto out_entries = writer.mutable_payload_data().mutable_view(kRspStructSize);
    for (const auto& attr : results) {
      *reinterpret_cast<att::Handle*>(out_entries.mutable_data()) = htole16(attr->handle());
      auto uuid_view = out_entries.mutable_view(sizeof(att::Handle));
      attr->type().ToBytes(&uuid_view, /*allow_32bit=*/false);

      // advance
      out_entries = out_entries.mutable_view(entry_size);
    }

    att_->Reply(tid, std::move(buffer));
  }

  void OnFindByTypeValueRequest(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kFindByTypeValueRequest);

    if (packet.payload_size() < sizeof(att::FindByTypeValueRequestParams)) {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
      return;
    }

    const auto& params = packet.payload<att::FindByTypeValueRequestParams>();
    att::Handle start = le16toh(params.start_handle);
    att::Handle end = le16toh(params.end_handle);
    UUID type(params.type);
    constexpr size_t kParamsSize = sizeof(att::FindByTypeValueRequestParams);

    BufferView value = packet.payload_data().view(kParamsSize, packet.payload_size() - kParamsSize);

    if (start == att::kInvalidHandle || start > end) {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidHandle);
      return;
    }

    auto iter = db()->GetIterator(start, end, &type, /*groups_only=*/false);
    if (iter.AtEnd()) {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kAttributeNotFound);
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
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kAttributeNotFound);
      return;
    }

    constexpr size_t kRspStructSize = sizeof(att::HandlesInformationList);
    size_t pdu_size = sizeof(att::Header) + kRspStructSize * results.size();
    auto buffer = NewSlabBuffer(pdu_size);
    ZX_ASSERT(buffer);

    att::PacketWriter writer(att::kFindByTypeValueResponse, buffer.get());

    // Points to the next entry in the target PDU.
    auto next_entry = writer.mutable_payload_data();
    for (const auto& attr : results) {
      auto* entry = reinterpret_cast<att::HandlesInformationList*>(next_entry.mutable_data());
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

  void OnReadByGroupType(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kReadByGroupTypeRequest);
    TRACE_DURATION("bluetooth", "gatt::Server::OnReadByGroupType");

    att::Handle start, end;
    UUID group_type;

    // The group type is represented as either a 16-bit or 128-bit UUID.
    if (packet.payload_size() == sizeof(att::ReadByTypeRequestParams16)) {
      const auto& params = packet.payload<att::ReadByTypeRequestParams16>();
      start = le16toh(params.start_handle);
      end = le16toh(params.end_handle);
      group_type = UUID(le16toh(params.type));
    } else if (packet.payload_size() == sizeof(att::ReadByTypeRequestParams128)) {
      const auto& params = packet.payload<att::ReadByTypeRequestParams128>();
      start = le16toh(params.start_handle);
      end = le16toh(params.end_handle);
      group_type = UUID(params.type);
    } else {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
      return;
    }

    if (group_type != types::kPrimaryService && group_type != types::kSecondaryService) {
      att_->ReplyWithError(tid, start, att::ErrorCode::kUnsupportedGroupType);
      return;
    }

    constexpr size_t kRspStructSize = sizeof(att::ReadByGroupTypeResponseParams);
    constexpr size_t kHeaderSize = sizeof(att::Header) + kRspStructSize;
    ZX_DEBUG_ASSERT(kHeaderSize <= att_->mtu());

    size_t value_size;
    std::list<const att::Attribute*> results;
    fitx::result<att::ErrorCode> status =
        ReadByTypeHelper(start, end, group_type, /*group_type=*/true, att_->mtu() - kHeaderSize,
                         att::kMaxReadByGroupTypeValueLength, sizeof(att::AttributeGroupDataEntry),
                         &value_size, &results);
    if (status.is_error()) {
      att_->ReplyWithError(tid, start, status.error_value());
      return;
    }

    ZX_DEBUG_ASSERT(!results.empty());

    size_t entry_size = value_size + sizeof(att::AttributeGroupDataEntry);
    size_t pdu_size = kHeaderSize + entry_size * results.size();
    ZX_DEBUG_ASSERT(pdu_size <= att_->mtu());

    auto buffer = NewSlabBuffer(pdu_size);
    ZX_ASSERT(buffer);

    att::PacketWriter writer(att::kReadByGroupTypeResponse, buffer.get());
    auto params = writer.mutable_payload<att::ReadByGroupTypeResponseParams>();

    ZX_DEBUG_ASSERT(entry_size <= std::numeric_limits<uint8_t>::max());
    params->length = static_cast<uint8_t>(entry_size);

    // Points to the next entry in the target PDU.
    auto next_entry = writer.mutable_payload_data().mutable_view(kRspStructSize);
    for (const auto& attr : results) {
      auto* entry = reinterpret_cast<att::AttributeGroupDataEntry*>(next_entry.mutable_data());
      entry->start_handle = htole16(attr->group().start_handle());
      entry->group_end_handle = htole16(attr->group().end_handle());
      next_entry.Write(attr->group().decl_value().view(0, value_size),
                       sizeof(att::AttributeGroupDataEntry));

      next_entry = next_entry.mutable_view(entry_size);
    }

    att_->Reply(tid, std::move(buffer));
  }

  void OnReadByType(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kReadByTypeRequest);
    TRACE_DURATION("bluetooth", "gatt::Server::OnReadByType");

    att::Handle start, end;
    UUID type;

    // The attribute type is represented as either a 16-bit or 128-bit UUID.
    if (packet.payload_size() == sizeof(att::ReadByTypeRequestParams16)) {
      const auto& params = packet.payload<att::ReadByTypeRequestParams16>();
      start = le16toh(params.start_handle);
      end = le16toh(params.end_handle);
      type = UUID(le16toh(params.type));
    } else if (packet.payload_size() == sizeof(att::ReadByTypeRequestParams128)) {
      const auto& params = packet.payload<att::ReadByTypeRequestParams128>();
      start = le16toh(params.start_handle);
      end = le16toh(params.end_handle);
      type = UUID(params.type);
    } else {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
      return;
    }

    constexpr size_t kRspStructSize = sizeof(att::ReadByTypeResponseParams);
    constexpr size_t kHeaderSize = sizeof(att::Header) + kRspStructSize;
    ZX_DEBUG_ASSERT(kHeaderSize <= att_->mtu());

    size_t value_size;
    std::list<const att::Attribute*> results;
    fitx::result<att::ErrorCode> status = ReadByTypeHelper(
        start, end, type, /*group_type=*/false, att_->mtu() - kHeaderSize,
        att::kMaxReadByTypeValueLength, sizeof(att::AttributeData), &value_size, &results);
    if (status.is_error()) {
      att_->ReplyWithError(tid, start, status.error_value());
      return;
    }

    ZX_DEBUG_ASSERT(!results.empty());

    // If the value is dynamic, then delegate the read to any registered handler.
    if (!results.front()->value()) {
      ZX_DEBUG_ASSERT(results.size() == 1u);

      const size_t kMaxValueSize = std::min(att_->mtu() - kHeaderSize - sizeof(att::AttributeData),
                                            static_cast<size_t>(att::kMaxReadByTypeValueLength));

      att::Handle handle = results.front()->handle();
      auto self = weak_ptr_factory_.GetWeakPtr();
      auto result_cb = [self, tid, handle, kMaxValueSize](fitx::result<att::ErrorCode> status,
                                                          const auto& value) {
        if (!self)
          return;

        if (status.is_error()) {
          self->att_->ReplyWithError(tid, handle, status.error_value());
          return;
        }

        // Respond with just a single entry.
        size_t value_size = std::min(value.size(), kMaxValueSize);
        size_t entry_size = value_size + sizeof(att::AttributeData);
        auto buffer = NewSlabBuffer(entry_size + kHeaderSize);
        att::PacketWriter writer(att::kReadByTypeResponse, buffer.get());

        auto params = writer.mutable_payload<att::ReadByTypeResponseParams>();
        params->length = static_cast<uint8_t>(entry_size);
        params->attribute_data_list->handle = htole16(handle);
        writer.mutable_payload_data().Write(value.data(), value_size,
                                            sizeof(params->length) + sizeof(handle));

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

    auto buffer = NewSlabBuffer(pdu_size);
    ZX_ASSERT(buffer);

    att::PacketWriter writer(att::kReadByTypeResponse, buffer.get());
    auto params = writer.mutable_payload<att::ReadByTypeResponseParams>();
    params->length = static_cast<uint8_t>(entry_size);

    // Points to the next entry in the target PDU.
    auto next_entry = writer.mutable_payload_data().mutable_view(kRspStructSize);
    for (const auto& attr : results) {
      auto* entry = reinterpret_cast<att::AttributeData*>(next_entry.mutable_data());
      entry->handle = htole16(attr->handle());
      next_entry.Write(attr->value()->view(0, value_size), sizeof(entry->handle));

      next_entry = next_entry.mutable_view(entry_size);
    }

    att_->Reply(tid, std::move(buffer));
  }

  void OnReadBlobRequest(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kReadBlobRequest);

    if (packet.payload_size() != sizeof(att::ReadBlobRequestParams)) {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
      return;
    }

    const auto& params = packet.payload<att::ReadBlobRequestParams>();
    att::Handle handle = le16toh(params.handle);
    uint16_t offset = le16toh(params.offset);

    const auto* attr = db()->FindAttribute(handle);
    if (!attr) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kInvalidHandle);
      return;
    }

    fitx::result<att::ErrorCode> status =
        att::CheckReadPermissions(attr->read_reqs(), att_->security());
    if (status.is_error()) {
      att_->ReplyWithError(tid, handle, status.error_value());
      return;
    }

    constexpr size_t kHeaderSize = sizeof(att::Header);

    auto self = weak_ptr_factory_.GetWeakPtr();
    auto callback = [self, tid, handle](fitx::result<att::ErrorCode> status, const auto& value) {
      if (!self)
        return;

      if (status.is_error()) {
        self->att_->ReplyWithError(tid, handle, status.error_value());
        return;
      }

      size_t value_size = std::min(value.size(), self->att_->mtu() - kHeaderSize);
      auto buffer = NewSlabBuffer(value_size + kHeaderSize);
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
      size_t value_size = std::min(attr->value()->size(), self->att_->mtu() - kHeaderSize);
      callback(fitx::ok(), attr->value()->view(offset, value_size));
      return;
    }

    // TODO(fxbug.dev/636): Add a timeout to this
    if (!attr->ReadAsync(peer_id_, offset, callback)) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kReadNotPermitted);
    }
  }

  void OnReadRequest(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kReadRequest);

    if (packet.payload_size() != sizeof(att::ReadRequestParams)) {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
      return;
    }

    const auto& params = packet.payload<att::WriteRequestParams>();
    att::Handle handle = le16toh(params.handle);

    const auto* attr = db()->FindAttribute(handle);
    if (!attr) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kInvalidHandle);
      return;
    }

    fitx::result<att::ErrorCode> status =
        att::CheckReadPermissions(attr->read_reqs(), att_->security());
    if (status.is_error()) {
      att_->ReplyWithError(tid, handle, status.error_value());
      return;
    }

    constexpr size_t kHeaderSize = sizeof(att::Header);

    auto self = weak_ptr_factory_.GetWeakPtr();
    auto callback = [self, tid, handle](fitx::result<att::ErrorCode> status, const auto& value) {
      if (!self)
        return;

      if (status.is_error()) {
        self->att_->ReplyWithError(tid, handle, status.error_value());
        return;
      }

      size_t value_size = std::min(value.size(), self->att_->mtu() - kHeaderSize);
      auto buffer = NewSlabBuffer(value_size + kHeaderSize);
      ZX_ASSERT(buffer);

      att::PacketWriter writer(att::kReadResponse, buffer.get());
      writer.mutable_payload_data().Write(value.view(0, value_size));

      self->att_->Reply(tid, std::move(buffer));
    };

    // Use the cached value if there is one.
    if (attr->value()) {
      callback(fitx::ok(), *attr->value());
      return;
    }

    if (!attr->ReadAsync(peer_id_, 0, callback)) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kReadNotPermitted);
    }
  }

  void OnWriteCommand(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kWriteCommand);

    if (packet.payload_size() < sizeof(att::WriteRequestParams)) {
      // Ignore if wrong size, no response allowed
      return;
    }

    const auto& params = packet.payload<att::WriteRequestParams>();
    att::Handle handle = le16toh(params.handle);
    const auto* attr = db()->FindAttribute(handle);

    // Attributes can be invalid if the handle is invalid
    if (!attr) {
      return;
    }

    fitx::result<att::ErrorCode> status =
        att::CheckWritePermissions(attr->write_reqs(), att_->security());
    if (status.is_error()) {
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

  void OnWriteRequest(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kWriteRequest);

    if (packet.payload_size() < sizeof(att::WriteRequestParams)) {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
      return;
    }

    const auto& params = packet.payload<att::WriteRequestParams>();
    att::Handle handle = le16toh(params.handle);

    const auto* attr = db()->FindAttribute(handle);
    if (!attr) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kInvalidHandle);
      return;
    }

    fitx::result<att::ErrorCode> status =
        att::CheckWritePermissions(attr->write_reqs(), att_->security());
    if (status.is_error()) {
      att_->ReplyWithError(tid, handle, status.error_value());
      return;
    }

    // Attributes with a static value cannot be written.
    if (attr->value()) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kWriteNotPermitted);
      return;
    }

    auto value_view = packet.payload_data().view(sizeof(params.handle));
    if (value_view.size() > att::kMaxAttributeValueLength) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kInvalidAttributeValueLength);
      return;
    }

    auto self = weak_ptr_factory_.GetWeakPtr();
    auto result_cb = [self, tid, handle](fitx::result<att::ErrorCode> status) {
      if (!self)
        return;

      if (status.is_error()) {
        self->att_->ReplyWithError(tid, handle, status.error_value());
        return;
      }

      auto buffer = NewSlabBuffer(1);
      (*buffer)[0] = att::kWriteResponse;
      self->att_->Reply(tid, std::move(buffer));
    };

    if (!attr->WriteAsync(peer_id_, 0, value_view, result_cb)) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kWriteNotPermitted);
    }
  }

  void OnPrepareWriteRequest(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kPrepareWriteRequest);

    if (packet.payload_size() < sizeof(att::PrepareWriteRequestParams)) {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
      return;
    }

    const auto& params = packet.payload<att::PrepareWriteRequestParams>();
    att::Handle handle = le16toh(params.handle);
    uint16_t offset = le16toh(params.offset);
    auto value_view = packet.payload_data().view(sizeof(params.handle) + sizeof(params.offset));

    if (prepare_queue_.size() >= att::kPrepareQueueMaxCapacity) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kPrepareQueueFull);
      return;
    }

    // Validate attribute handle and perform security checks (see Vol 3, Part F,
    // 3.4.6.1 for required checks)
    const auto* attr = db()->FindAttribute(handle);
    if (!attr) {
      att_->ReplyWithError(tid, handle, att::ErrorCode::kInvalidHandle);
      return;
    }

    fitx::result<att::ErrorCode> status =
        att::CheckWritePermissions(attr->write_reqs(), att_->security());
    if (status.is_error()) {
      att_->ReplyWithError(tid, handle, status.error_value());
      return;
    }

    prepare_queue_.push(att::QueuedWrite(handle, offset, value_view));

    // Reply back with the request payload.
    auto buffer = std::make_unique<DynamicByteBuffer>(packet.size());
    att::PacketWriter writer(att::kPrepareWriteResponse, buffer.get());
    writer.mutable_payload_data().Write(packet.payload_data());

    att_->Reply(tid, std::move(buffer));
  }

  void OnExecuteWriteRequest(att::Bearer::TransactionId tid, const att::PacketReader& packet) {
    ZX_DEBUG_ASSERT(packet.opcode() == att::kExecuteWriteRequest);

    if (packet.payload_size() != sizeof(att::ExecuteWriteRequestParams)) {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
      return;
    }

    const auto& params = packet.payload<att::ExecuteWriteRequestParams>();
    if (params.flags == att::ExecuteWriteFlag::kCancelAll) {
      prepare_queue_ = {};

      auto buffer = std::make_unique<DynamicByteBuffer>(1);
      att::PacketWriter writer(att::kExecuteWriteResponse, buffer.get());
      att_->Reply(tid, std::move(buffer));
      return;
    }

    if (params.flags != att::ExecuteWriteFlag::kWritePending) {
      att_->ReplyWithError(tid, att::kInvalidHandle, att::ErrorCode::kInvalidPDU);
      return;
    }

    auto self = weak_ptr_factory_.GetWeakPtr();
    auto result_cb = [self,
                      tid](fitx::result<std::tuple<att::Handle, att::ErrorCode>> result) mutable {
      if (!self)
        return;

      if (result.is_error()) {
        auto [handle, ecode] = result.error_value();
        self->att_->ReplyWithError(tid, handle, ecode);
        return;
      }

      auto rsp = std::make_unique<DynamicByteBuffer>(1);
      att::PacketWriter writer(att::kExecuteWriteResponse, rsp.get());
      self->att_->Reply(tid, std::move(rsp));
    };
    db()->ExecuteWriteQueue(peer_id_, std::move(prepare_queue_), att_->security(),
                            std::move(result_cb));
  }

  // Helper function to serve the Read By Type and Read By Group Type requests. This searches |db|
  // for attributes with the given |type| and adds as many attributes as it can fit within the given
  // |max_data_list_size|. The attribute value that should be included within each attribute data
  // list entry is returned in |out_value_size|.
  //
  // If the result is a dynamic attribute, |out_results| will contain at most one entry.
  // |out_value_size| will point to an undefined value in that case.
  //
  // On error, returns an error code that can be used in a ATT Error Response.
  fitx::result<att::ErrorCode> ReadByTypeHelper(att::Handle start, att::Handle end,
                                                const UUID& type, bool group_type,
                                                size_t max_data_list_size, size_t max_value_size,
                                                size_t entry_prefix_size, size_t* out_value_size,
                                                std::list<const att::Attribute*>* out_results) {
    ZX_DEBUG_ASSERT(out_results);
    ZX_DEBUG_ASSERT(out_value_size);

    if (start == att::kInvalidHandle || start > end)
      return fitx::error(att::ErrorCode::kInvalidHandle);

    auto iter = db()->GetIterator(start, end, &type, group_type);
    if (iter.AtEnd())
      return fitx::error(att::ErrorCode::kAttributeNotFound);

    // |value_size| is the size of the complete attribute value for each result
    // entry. |entry_size| = |value_size| + |entry_prefix_size|. We store these
    // separately to avoid recalculating one every it gets checked.
    size_t value_size;
    size_t entry_size;
    std::list<const att::Attribute*> results;

    for (; !iter.AtEnd(); iter.Advance()) {
      const auto* attr = iter.get();
      ZX_DEBUG_ASSERT(attr);

      fitx::result<att::ErrorCode> security_result =
          att::CheckReadPermissions(attr->read_reqs(), att_->security());
      if (security_result.is_error()) {
        // Return error only if this is the first result that matched. We simply stop the search
        // otherwise.
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
            std::min(std::min(value_size, max_value_size) + entry_prefix_size, max_data_list_size);

        // Actual value size to include in a PDU.
        *out_value_size = entry_size - entry_prefix_size;

      } else if (!attr->value() || attr->value()->size() != value_size ||
                 entry_size > max_data_list_size) {
        // Stop the search and exclude this attribute because either:
        // a. we ran into a dynamic value in a result that contains static values,
        // b. the matching attribute has a different value size than the first
        //    attribute,
        // c. there is no remaining space in the response PDU.
        break;
      }

      results.push_back(attr);
      max_data_list_size -= entry_size;
    }

    if (results.empty())
      return fitx::error(att::ErrorCode::kAttributeNotFound);

    *out_results = std::move(results);
    return fitx::ok();
  }

  PeerId peer_id_;
  fxl::WeakPtr<LocalServiceManager> local_services_;
  fxl::WeakPtr<att::Bearer> att_;

  // The queue data structure used for queued writes (see Vol 3, Part F, 3.4.6).
  att::PrepareWriteQueue prepare_queue_;

  // ATT protocol request handler IDs
  // TODO(armansito): Storing all these IDs here feels wasteful. Provide a way
  // to unregister GATT server callbacks collectively from an att::Bearer, given
  // that it's server-role functionalities are uniquely handled by this class.
  att::Bearer::HandlerId exchange_mtu_id_;
  att::Bearer::HandlerId find_information_id_;
  att::Bearer::HandlerId read_by_group_type_id_;
  att::Bearer::HandlerId read_by_type_id_;
  att::Bearer::HandlerId read_req_id_;
  att::Bearer::HandlerId write_req_id_;
  att::Bearer::HandlerId write_cmd_id_;
  att::Bearer::HandlerId read_blob_req_id_;
  att::Bearer::HandlerId find_by_type_value_id_;
  att::Bearer::HandlerId prepare_write_id_;
  att::Bearer::HandlerId exec_write_id_;

  fxl::WeakPtrFactory<AttBasedServer> weak_ptr_factory_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AttBasedServer);
};

// static
std::unique_ptr<Server> Server::Create(PeerId peer_id,
                                       fxl::WeakPtr<LocalServiceManager> local_services,
                                       fxl::WeakPtr<att::Bearer> bearer) {
  return std::make_unique<AttBasedServer>(peer_id, std::move(local_services), std::move(bearer));
}
}  // namespace bt::gatt
