// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/hci/advertising_packet_filter.h"

#include "pw_bluetooth/hci_android.emb.h"
#include "pw_bluetooth_sapphire/internal/host/common/uint128.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/vendor_protocol.h"

namespace bt::hci {

namespace android_emb = pw::bluetooth::vendor::android_hci;
namespace android_hci = hci_spec::vendor::android;

AdvertisingPacketFilter::AdvertisingPacketFilter(
    const Config& packet_filter_config, Transport::WeakPtr hci)
    : config_(packet_filter_config), hci_(std::move(hci)) {
  PW_DCHECK(hci_.is_alive());
  bt_log(INFO,
         "hci",
         "advertising packet filter initialized with offloading enabled: %s, "
         "max_filters: %d",
         packet_filter_config.offloading_enabled() ? "yes" : "no",
         packet_filter_config.max_filters());

  hci_cmd_runner_ = std::make_unique<SequentialCommandRunner>(
      hci_->command_channel()->AsWeakPtr());

  ResetOpenSlots();
}

void AdvertisingPacketFilter::SetPacketFilters(
    ScanId scan_id, const std::vector<DiscoveryFilter>& filters) {
  UnsetPacketFiltersInternal(scan_id, false);

  bt_log(INFO,
         "hci",
         "setting packet filters for scan id: %d, filters: %zu",
         scan_id,
         filters.size());

  scan_ids_.insert(scan_id);
  scan_id_to_filters_[scan_id] = filters;

  if (!config_.offloading_enabled()) {
    return;
  }

  if (filters.empty()) {
    return;
  }

  // NOTE(b/448475405): We suspect there is a bug with advertising packet
  // filtering where we don't get scan results on time from the Controller. So
  // as not to affect others who use Bluetooth scanning, disable advertising
  // packet filtering for now while we investigate.
  bt_log(INFO,
         "hci-le",
         "pre-emptively disabling advertising packet filtering while we "
         "investigate a bug within it");
  return;

  // If none of our filters are offloadable and we turn on scan filter
  // offloading, we will get no results.
  bool any_filters_offloadable = false;
  for (const DiscoveryFilter& filter : filters) {
    if (IsOffloadable(filter)) {
      any_filters_offloadable = true;
      break;
    }
  }
  if (!any_filters_offloadable) {
    bt_log(INFO, "hci-le", "no filters can be offloaded");
    return;
  }

  if (!MemoryAvailableForFilters(filters)) {
    bt_log(INFO,
           "hci-le",
           "controller out of offloaded filter memory (scan id: %d)",
           scan_id);
    DisableOffloadedFiltering();
    return;
  }

  if (!offloaded_filtering_enabled_) {
    bt_log(INFO, "hci-le", "controller filter memory available");
    EnableOffloadedFiltering();
    return;
  }

  for (const DiscoveryFilter& filter : filters) {
    bool success = Offload(scan_id, filter);
    if (!success) {
      bt_log(WARN, "hci-le", "failed enabling offloaded filtering");
      DisableOffloadedFiltering();
      return;
    }
  }

  if (!hci_cmd_runner_->IsReady()) {
    return;
  }

  hci_cmd_runner_->RunCommands([this, scan_id](Result<> result) {
    if (bt_is_error(result,
                    WARN,
                    "hci-le",
                    "failed offloading filters (scan id: %d)",
                    scan_id)) {
      DisableOffloadedFiltering();
    }
  });
}

void AdvertisingPacketFilter::UnsetPacketFiltersInternal(ScanId scan_id,
                                                         bool run_commands) {
  if (scan_id_to_filters_.count(scan_id) == 0) {
    return;
  }

  bt_log(INFO, "hci", "removing packet filters for scan id: %d", scan_id);
  scan_ids_.erase(scan_id);
  scan_id_to_filters_.erase(scan_id);

  if (!config_.offloading_enabled()) {
    return;
  }

  if (!offloaded_filtering_enabled_) {
    bt_log(INFO, "hci-le", "controller filter memory available");
    if (MemoryAvailable()) {
      EnableOffloadedFiltering();
    }

    return;
  }

  if (!scan_id_to_index_.contains(scan_id)) {
    return;
  }

  bt_log(INFO, "hci-le", "deleting offloaded filters (scan id: %d)", scan_id);
  const std::unordered_set<FilterIndex>& filter_indexes =
      scan_id_to_index_.get(scan_id).value();
  for (FilterIndex filter_index : filter_indexes) {
    CommandPacket packet = BuildUnsetParametersCommand(filter_index);
    hci_cmd_runner_->QueueCommand(std::move(packet));
  }
  scan_id_to_index_.remove(scan_id);

  if (!hci_cmd_runner_->IsReady()) {
    return;
  }

  if (!run_commands) {
    return;
  }

  hci_cmd_runner_->RunCommands([this, scan_id](Result<> result) {
    bt_is_error(result,
                WARN,
                "hci-le",
                "failed removing offloaded filters (scan id: %d)",
                scan_id);
    DisableOffloadedFiltering();
  });
}

std::unordered_set<AdvertisingPacketFilter::ScanId>
AdvertisingPacketFilter::Matches(const AdvertisingData::ParseResult& ad,
                                 bool connectable,
                                 int8_t rssi) const {
  std::unordered_set<ScanId> result;

  for (uint16_t scan_id : scan_ids_) {
    if (Matches(scan_id, ad, connectable, rssi)) {
      result.insert(scan_id);
    }
  }

  return result;
}

bool AdvertisingPacketFilter::Matches(ScanId scan_id,
                                      const AdvertisingData::ParseResult& ad,
                                      bool connectable,
                                      int8_t rssi) const {
  if (scan_id_to_filters_.count(scan_id) == 0) {
    return true;
  }

  auto iter = scan_id_to_filters_.find(scan_id);
  const std::vector<DiscoveryFilter>& filters = iter->second;
  if (filters.empty()) {
    return true;
  }

  std::optional<std::reference_wrapper<const AdvertisingData>> data;
  if (ad.is_ok()) {
    data.emplace(ad.value());
  }

  for (const DiscoveryFilter& filter : filters) {
    if (filter.Matches(data, connectable, rssi)) {
      return true;
    }
  }

  return false;
}

std::optional<AdvertisingPacketFilter::FilterIndex>
AdvertisingPacketFilter::NextFilterIndex() {
  if (scan_id_to_index_.size_many() >= config_.max_filters()) {
    return std::nullopt;
  }

  FilterIndex value = last_filter_index_;
  do {
    value = (value + 1) % config_.max_filters();
  } while (scan_id_to_index_.contains(value));

  last_filter_index_ = value;
  return value;
}

bool AdvertisingPacketFilter::MemoryAvailable() const {
  if (!config_.offloading_enabled()) {
    return false;
  }

  size_t total_filters = 0;
  for (const auto& [_, filters] : scan_id_to_filters_) {
    total_filters += filters.size();
  }

  size_t num_filters_offloaded = scan_id_to_index_.size_many();
  if (num_filters_offloaded + total_filters > config_.max_filters()) {
    return false;
  }

  std::unordered_map<OffloadedFilterType, uint8_t> new_slots = {
      {OffloadedFilterType::kServiceUUID, 0},
      {OffloadedFilterType::kServiceDataUUID, 0},
      {OffloadedFilterType::kSolicitationUUID, 0},
      {OffloadedFilterType::kLocalName, 0},
      {OffloadedFilterType::kManufacturerCode, 0},
  };

  for (const auto& [scan_id, filters] : scan_id_to_filters_) {
    for (const DiscoveryFilter& filter : filters) {
      if (!MemoryAvailableForFilter(filter, new_slots)) {
        return false;
      }
    }
  }

  return true;
}

bool AdvertisingPacketFilter::MemoryAvailableForFilter(
    const DiscoveryFilter& filter,
    std::unordered_map<OffloadedFilterType, uint8_t>& new_slots) const {
  if (!config_.offloading_enabled()) {
    return false;
  }

  size_t num_filters_offloaded = scan_id_to_index_.size_many();
  if (num_filters_offloaded + 1 > config_.max_filters()) {
    return false;
  }

  if (!filter.service_uuids().empty()) {
    uint8_t value = ++new_slots[OffloadedFilterType::kServiceUUID];
    if (!MemoryAvailableForSlots(OffloadedFilterType::kServiceUUID, value)) {
      return false;
    }
  }

  if (!filter.service_data_uuids().empty()) {
    uint8_t value = ++new_slots[OffloadedFilterType::kServiceDataUUID];
    if (!MemoryAvailableForSlots(OffloadedFilterType::kServiceDataUUID,
                                 value)) {
      return false;
    }
  }

  if (!filter.solicitation_uuids().empty()) {
    uint8_t value = ++new_slots[OffloadedFilterType::kSolicitationUUID];
    if (!MemoryAvailableForSlots(OffloadedFilterType::kSolicitationUUID,
                                 value)) {
      return false;
    }
  }

  if (!filter.name_substring().empty()) {
    uint8_t value = ++new_slots[OffloadedFilterType::kLocalName];
    if (!MemoryAvailableForSlots(OffloadedFilterType::kLocalName, value)) {
      return false;
    }
  }

  if (filter.manufacturer_code().has_value()) {
    uint8_t value = ++new_slots[OffloadedFilterType::kManufacturerCode];
    if (!MemoryAvailableForSlots(OffloadedFilterType::kManufacturerCode,
                                 value)) {
      return false;
    }
  }

  return true;
}

bool AdvertisingPacketFilter::MemoryAvailableForFilters(
    const std::vector<DiscoveryFilter>& filters) const {
  if (!config_.offloading_enabled()) {
    return false;
  }

  size_t num_filters_offloaded = scan_id_to_index_.size_many();
  if (num_filters_offloaded + filters.size() > config_.max_filters()) {
    return false;
  }

  std::unordered_map<OffloadedFilterType, uint8_t> new_slots = {
      {OffloadedFilterType::kServiceUUID, 0},
      {OffloadedFilterType::kServiceDataUUID, 0},
      {OffloadedFilterType::kSolicitationUUID, 0},
      {OffloadedFilterType::kLocalName, 0},
      {OffloadedFilterType::kManufacturerCode, 0},
  };

  for (const DiscoveryFilter& filter : filters) {
    if (!MemoryAvailableForFilter(filter, new_slots)) {
      return false;
    }
  }

  return true;
}

bool AdvertisingPacketFilter::MemoryAvailableForSlots(
    OffloadedFilterType filter_type, uint8_t slots) const {
  if (!config_.offloading_enabled()) {
    return false;
  }

  auto itr = open_slots_.find(filter_type);
  if (itr == open_slots_.end()) {
    return false;
  }

  uint8_t available = itr->second;
  if (available - slots < 0) {
    return false;
  }

  return true;
}

void AdvertisingPacketFilter::EnableOffloadedFiltering() {
  if (offloaded_filtering_enabled_) {
    return;
  }

  if (!hci_cmd_runner_->IsReady()) {
    hci_cmd_runner_->Cancel();
  }

  bt_log(INFO, "hci-le", "enabling offloaded controller packet filtering");
  hci_cmd_runner_->QueueCommand(BuildEnableCommand(true));

  for (const auto& [scan_id, filters] : scan_id_to_filters_) {
    for (const DiscoveryFilter& filter : filters) {
      bool success = Offload(scan_id, filter);
      if (!success) {
        bt_log(WARN, "hci-le", "failed enabling offloaded filtering");
        DisableOffloadedFiltering();
        return;
      }
    }
  }

  hci_cmd_runner_->RunCommands([this](Result<> result) {
    if (bt_is_error(
            result, WARN, "hci-le", "failed enabling offloaded filtering")) {
      DisableOffloadedFiltering();
    }
  });

  offloaded_filtering_enabled_ = true;
}

void AdvertisingPacketFilter::DisableOffloadedFiltering() {
  if (!offloaded_filtering_enabled_) {
    return;
  }

  if (!hci_cmd_runner_->IsReady()) {
    hci_cmd_runner_->Cancel();
  }

  bt_log(INFO, "hci-le", "disabling offloaded filtering, using host filtering");
  hci_cmd_runner_->QueueCommand(BuildClearParametersCommand());
  hci_cmd_runner_->QueueCommand(BuildEnableCommand(false));

  hci_cmd_runner_->RunCommands([](Result<> result) {
    bt_is_error(result, WARN, "hci-le", "failed disabling offloaded filtering");
  });

  ResetOpenSlots();
  last_filter_index_ = kStartFilterIndex;
  scan_id_to_index_.clear();
  offloaded_filtering_enabled_ = false;
}

bool AdvertisingPacketFilter::IsOffloadable(const DiscoveryFilter& filter) {
  return !filter.service_uuids().empty() ||
         !filter.service_data_uuids().empty() ||
         !filter.solicitation_uuids().empty() ||
         !filter.name_substring().empty() ||
         filter.manufacturer_code().has_value();
}

bool AdvertisingPacketFilter::Offload(ScanId scan_id,
                                      const DiscoveryFilter& filter) {
  std::optional<FilterIndex> filter_index_opt = NextFilterIndex();
  if (!filter_index_opt.has_value()) {
    bt_log(WARN,
           "hci-le",
           "filter index unavailable, unable to offload filter (scan id: %d)",
           scan_id);
    return false;
  }

  FilterIndex filter_index = filter_index_opt.value();
  scan_id_to_index_.put(scan_id, filter_index);

  CommandPacket set_parameters =
      BuildSetParametersCommand(filter_index, filter);
  hci_cmd_runner_->QueueCommand(set_parameters);

  if (!filter.service_uuids().empty()) {
    std::vector<CommandPacket> packets =
        BuildSetServiceUUIDCommands(filter_index, filter.service_uuids());
    for (const CommandPacket& packet : packets) {
      hci_cmd_runner_->QueueCommand(packet, [this](const EventPacket& event) {
        hci::Result<> result = event.ToResult();
        if (bt_is_error(result, WARN, "hci-le", "failed offloading filter")) {
          DisableOffloadedFiltering();
          return;
        }
        auto view = event.view<android_emb::LEApcfCommandCompleteEventView>();
        uint8_t available_spaces = view.available_spaces().Read();
        open_slots_[OffloadedFilterType::kServiceUUID] = available_spaces;
      });
    }
  }

  if (!filter.service_data_uuids().empty()) {
    std::vector<CommandPacket> packets = BuildSetServiceDataUUIDCommands(
        filter_index, filter.service_data_uuids());
    for (const CommandPacket& packet : packets) {
      hci_cmd_runner_->QueueCommand(packet, [this](const EventPacket& event) {
        hci::Result<> result = event.ToResult();
        if (bt_is_error(result, WARN, "hci-le", "failed offloading filter")) {
          DisableOffloadedFiltering();
          return;
        }
        auto view = event.view<android_emb::LEApcfCommandCompleteEventView>();
        uint8_t available_spaces = view.available_spaces().Read();
        open_slots_[OffloadedFilterType::kServiceDataUUID] = available_spaces;
      });
    }
  }

  if (!filter.solicitation_uuids().empty()) {
    std::vector<CommandPacket> packets = BuildSetSolicitationUUIDCommands(
        filter_index, filter.solicitation_uuids());
    for (const CommandPacket& packet : packets) {
      hci_cmd_runner_->QueueCommand(packet, [this](const EventPacket& event) {
        hci::Result<> result = event.ToResult();
        if (bt_is_error(result, WARN, "hci-le", "failed offloading filter")) {
          DisableOffloadedFiltering();
          return;
        }
        auto view = event.view<android_emb::LEApcfCommandCompleteEventView>();
        uint8_t available_spaces = view.available_spaces().Read();
        open_slots_[OffloadedFilterType::kSolicitationUUID] = available_spaces;
      });
    }
  }

  if (!filter.name_substring().empty()) {
    std::optional<CommandPacket> packet =
        BuildSetLocalNameCommand(filter_index, filter.name_substring());
    if (packet) {
      hci_cmd_runner_->QueueCommand(
          packet.value(), [this](const EventPacket& event) {
            hci::Result<> result = event.ToResult();
            if (bt_is_error(
                    result, WARN, "hci-le", "failed offloading filter")) {
              DisableOffloadedFiltering();
              return;
            }
            auto view =
                event.view<android_emb::LEApcfCommandCompleteEventView>();
            uint8_t available_spaces = view.available_spaces().Read();
            open_slots_[OffloadedFilterType::kLocalName] = available_spaces;
          });
    }
  }

  if (filter.manufacturer_code().has_value()) {
    std::optional<CommandPacket> packet = BuildSetManufacturerCodeCommand(
        filter_index, filter.manufacturer_code().value());
    if (packet) {
      hci_cmd_runner_->QueueCommand(
          packet.value(), [this](const EventPacket& event) {
            hci::Result<> result = event.ToResult();
            if (bt_is_error(
                    result, WARN, "hci-le", "failed offloading filter")) {
              DisableOffloadedFiltering();
              return;
            }
            auto view =
                event.view<android_emb::LEApcfCommandCompleteEventView>();
            uint8_t available_spaces = view.available_spaces().Read();
            open_slots_[OffloadedFilterType::kManufacturerCode] =
                available_spaces;
          });
    }
  }

  return true;
}

void AdvertisingPacketFilter::ResetOpenSlots() {
  open_slots_[OffloadedFilterType::kServiceUUID] = config_.max_filters();
  open_slots_[OffloadedFilterType::kServiceDataUUID] = config_.max_filters();
  open_slots_[OffloadedFilterType::kSolicitationUUID] = config_.max_filters();
  open_slots_[OffloadedFilterType::kLocalName] = config_.max_filters();
  open_slots_[OffloadedFilterType::kManufacturerCode] = config_.max_filters();
}

CommandPacket AdvertisingPacketFilter::BuildEnableCommand(bool enabled) const {
  auto packet = hci::CommandPacket::New<android_emb::LEApcfEnableCommandWriter>(
      android_hci::kLEApcf);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(android_hci::kLEApcfEnableSubopcode);

  if (enabled) {
    view.enabled().Write(hci_spec::GenericEnableParam::ENABLE);
  } else {
    view.enabled().Write(hci_spec::GenericEnableParam::DISABLE);
  }

  return packet;
}

CommandPacket AdvertisingPacketFilter::BuildSetParametersCommand(
    FilterIndex filter_index, const DiscoveryFilter& filter) {
  auto packet = hci::CommandPacket::New<
      android_emb::LEApcfSetFilteringParametersCommandWriter>(
      android_hci::kLEApcf);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfSetFilteringParametersSubopcode);
  view.action().Write(android_emb::ApcfAction::ADD);
  view.filter_index().Write(filter_index);

  // Sapphire's scan filter API can be seen as an or operation across all
  // filters and an and operation within each individual filter. Skip
  // setting the list_logic_type field to maintain the or configuration
  // across all filters. Set the filter_logic_type to the and configuration
  // within each individual filter.
  view.filter_logic_type().Write(android_emb::ApcfFeatureFilterLogic::AND);

  if (!filter.service_uuids().empty()) {
    view.feature_selection().service_uuid().Write(true);
  }

  if (!filter.service_data_uuids().empty()) {
    view.feature_selection().service_data().Write(true);
  }

  if (!filter.solicitation_uuids().empty()) {
    view.feature_selection().service_solicitation_uuid().Write(true);
  }

  if (!filter.name_substring().empty()) {
    view.feature_selection().local_name().Write(true);
  }

  if (filter.manufacturer_code().has_value()) {
    view.feature_selection().manufacturer_data().Write(true);
  }

  if (filter.rssi().has_value() && !filter.pathloss().has_value()) {
    view.rssi_high_threshold().Write(filter.rssi().value());
  }

  view.delivery_mode().Write(android_emb::ApcfDeliveryMode::IMMEDIATE);

  // The rest of the packet contains configuration for, and is only valid
  // when, the delivery mode is ON_FOUND. We aren't using that delivery mode
  // so we don't set those fields.

  return packet;
}

CommandPacket AdvertisingPacketFilter::BuildClearParametersCommand() const {
  auto packet = hci::CommandPacket::New<
      android_emb::LEApcfSetFilteringParametersCommandWriter>(
      android_hci::kLEApcf);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfSetFilteringParametersSubopcode);
  view.action().Write(android_emb::ApcfAction::CLEAR);

  return packet;
}

CommandPacket AdvertisingPacketFilter::BuildUnsetParametersCommand(
    FilterIndex filter_index) const {
  auto packet = hci::CommandPacket::New<
      android_emb::LEApcfSetFilteringParametersCommandWriter>(
      android_hci::kLEApcf);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfSetFilteringParametersSubopcode);
  view.action().Write(android_emb::ApcfAction::DELETE);
  view.filter_index().Write(filter_index);

  return packet;
}

std::optional<CommandPacket>
AdvertisingPacketFilter::BuildSetServiceUUID16Command(FilterIndex filter_index,
                                                      const UUID& uuid) const {
  std::optional<uint16_t> opt = uuid.As16Bit();
  if (!opt) {
    return std::nullopt;
  }
  uint16_t value = opt.value();

  auto packet =
      hci::CommandPacket::New<android_emb::LEApcfServiceUUID16CommandWriter>(
          android_hci::kLEApcf);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfServiceUUIDSubopcode);
  view.filter_index().Write(filter_index);

  view.uuid().BackingStorage().WriteLittleEndianUInt<16>(value);
  view.uuid_mask().BackingStorage().WriteLittleEndianUInt<16>(
      std::numeric_limits<uint16_t>::max());

  return packet;
}

std::optional<CommandPacket>
AdvertisingPacketFilter::BuildSetServiceUUID32Command(FilterIndex filter_index,
                                                      const UUID& uuid) const {
  std::optional<uint32_t> opt = uuid.As32Bit();
  if (!opt) {
    return std::nullopt;
  }
  uint32_t value = opt.value();

  auto packet =
      hci::CommandPacket::New<android_emb::LEApcfServiceUUID32CommandWriter>(
          android_hci::kLEApcf);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfServiceUUIDSubopcode);
  view.filter_index().Write(filter_index);

  view.uuid().BackingStorage().WriteLittleEndianUInt<32>(value);
  view.uuid_mask().BackingStorage().WriteLittleEndianUInt<32>(
      std::numeric_limits<uint32_t>::max());

  return packet;
}

CommandPacket AdvertisingPacketFilter::BuildSetServiceUUID128Command(
    FilterIndex filter_index, const UUID& uuid) const {
  const UInt128& value = uuid.value();

  auto packet =
      hci::CommandPacket::New<android_emb::LEApcfServiceUUID128CommandWriter>(
          android_hci::kLEApcf);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfServiceUUIDSubopcode);
  view.filter_index().Write(filter_index);

  std::copy(value.begin(), value.end(), view.uuid().BackingStorage().data());

  UInt128 mask;
  mask.fill(std::numeric_limits<uint8_t>::max());
  std::copy(mask.begin(), mask.end(), view.uuid_mask().BackingStorage().data());

  return packet;
}

std::vector<CommandPacket> AdvertisingPacketFilter::BuildSetServiceUUIDCommands(
    FilterIndex filter_index, const std::vector<UUID>& uuids) const {
  std::vector<CommandPacket> packets;
  packets.reserve(uuids.size());

  for (const UUID& uuid : uuids) {
    switch (uuid.type()) {
      case UUID::Type::k16Bit: {
        auto packet = BuildSetServiceUUID16Command(filter_index, uuid);
        if (!packet) {
          return packets;
        }
        packets.push_back(std::move(packet.value()));
        break;
      }
      case UUID::Type::k32Bit: {
        auto packet = BuildSetServiceUUID32Command(filter_index, uuid);
        if (!packet) {
          return packets;
        }
        packets.push_back(std::move(packet.value()));
        break;
      }
      case UUID::Type::k128Bit: {
        auto packet = BuildSetServiceUUID128Command(filter_index, uuid);
        packets.push_back(std::move(packet));
      }
    }
  }

  return packets;
}

std::optional<CommandPacket>
AdvertisingPacketFilter::BuildSetSolicitationUUID16Command(
    FilterIndex filter_index, const UUID& uuid) const {
  std::optional<uint16_t> opt = uuid.As16Bit();
  if (!opt) {
    return std::nullopt;
  }
  uint16_t value = opt.value();

  auto packet = hci::CommandPacket::New<
      android_emb::LEApcfSolicitationUUID16CommandWriter>(android_hci::kLEApcf);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfServiceSolicitationUUIDSubopcode);
  view.filter_index().Write(filter_index);

  view.uuid().BackingStorage().WriteLittleEndianUInt<16>(value);
  view.uuid_mask().BackingStorage().WriteLittleEndianUInt<16>(
      std::numeric_limits<uint16_t>::max());

  return packet;
}

std::optional<CommandPacket>
AdvertisingPacketFilter::BuildSetSolicitationUUID32Command(
    FilterIndex filter_index, const UUID& uuid) const {
  std::optional<uint32_t> opt = uuid.As32Bit();
  if (!opt) {
    return std::nullopt;
  }
  uint32_t value = opt.value();

  auto packet = hci::CommandPacket::New<
      android_emb::LEApcfSolicitationUUID32CommandWriter>(android_hci::kLEApcf);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfServiceSolicitationUUIDSubopcode);
  view.filter_index().Write(filter_index);

  view.uuid().BackingStorage().WriteLittleEndianUInt<32>(value);
  view.uuid_mask().BackingStorage().WriteLittleEndianUInt<32>(
      std::numeric_limits<uint32_t>::max());

  return packet;
}

CommandPacket AdvertisingPacketFilter::BuildSetSolicitationUUID128Command(
    FilterIndex filter_index, const UUID& uuid) const {
  const UInt128& value = uuid.value();

  auto packet = hci::CommandPacket::New<
      android_emb::LEApcfSolicitationUUID128CommandWriter>(
      android_hci::kLEApcf);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfServiceSolicitationUUIDSubopcode);
  view.filter_index().Write(filter_index);

  std::copy(value.begin(), value.end(), view.uuid().BackingStorage().data());

  UInt128 mask;
  mask.fill(std::numeric_limits<uint8_t>::max());
  std::copy(mask.begin(), mask.end(), view.uuid_mask().BackingStorage().data());

  return packet;
}

std::vector<CommandPacket>
AdvertisingPacketFilter::BuildSetSolicitationUUIDCommands(
    FilterIndex filter_index, const std::vector<UUID>& uuids) const {
  std::vector<CommandPacket> packets;
  packets.reserve(uuids.size());

  for (const UUID& uuid : uuids) {
    switch (uuid.type()) {
      case UUID::Type::k16Bit: {
        auto packet = BuildSetSolicitationUUID16Command(filter_index, uuid);
        if (!packet) {
          return packets;
        }
        packets.push_back(std::move(packet.value()));
        break;
      }
      case UUID::Type::k32Bit: {
        auto packet = BuildSetSolicitationUUID32Command(filter_index, uuid);
        if (!packet) {
          return packets;
        }
        packets.push_back(std::move(packet.value()));
        break;
      }
      case UUID::Type::k128Bit: {
        auto packet = BuildSetSolicitationUUID128Command(filter_index, uuid);
        packets.push_back(std::move(packet));
      }
    }
  }

  return packets;
}

std::optional<CommandPacket>
AdvertisingPacketFilter::BuildSetServiceDataUUID16Command(
    FilterIndex filter_index, const UUID& uuid) const {
  std::optional<uint16_t> opt = uuid.As16Bit();
  if (!opt) {
    return std::nullopt;
  }
  uint16_t value = opt.value();

  size_t packet_size = android_emb::LEApcfServiceDataCommand::MinSizeInBytes() +
                       sizeof(uint16_t) * 2;
  auto packet =
      hci::CommandPacket::New<android_emb::LEApcfServiceDataCommandWriter>(
          android_hci::kLEApcf, packet_size);
  auto view = packet.view_t(sizeof(uint16_t));

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfServiceDataSubopcode);
  view.filter_index().Write(filter_index);

  view.service_data().BackingStorage().WriteLittleEndianUInt<16>(value);
  view.service_data_mask().BackingStorage().WriteLittleEndianUInt<16>(
      std::numeric_limits<uint16_t>::max());

  return packet;
}

std::optional<CommandPacket>
AdvertisingPacketFilter::BuildSetServiceDataUUID32Command(
    FilterIndex filter_index, const UUID& uuid) const {
  std::optional<uint32_t> opt = uuid.As32Bit();
  if (!opt) {
    return std::nullopt;
  }
  uint32_t value = opt.value();

  size_t packet_size = android_emb::LEApcfServiceDataCommand::MinSizeInBytes() +
                       sizeof(uint32_t) * 2;
  auto packet =
      hci::CommandPacket::New<android_emb::LEApcfServiceDataCommandWriter>(
          android_hci::kLEApcf, packet_size);
  auto view = packet.view_t(sizeof(uint32_t));

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfServiceDataSubopcode);
  view.filter_index().Write(filter_index);

  view.service_data().BackingStorage().WriteLittleEndianUInt<32>(value);
  view.service_data_mask().BackingStorage().WriteLittleEndianUInt<32>(
      std::numeric_limits<uint32_t>::max());

  return packet;
}

CommandPacket AdvertisingPacketFilter::BuildSetServiceDataUUID128Command(
    FilterIndex filter_index, const UUID& uuid) const {
  const UInt128& value = uuid.value();

  size_t packet_size = android_emb::LEApcfServiceDataCommand::MinSizeInBytes() +
                       kUInt128Size * 2;
  auto packet =
      hci::CommandPacket::New<android_emb::LEApcfServiceDataCommandWriter>(
          android_hci::kLEApcf, packet_size);
  auto view = packet.view_t(kUInt128Size);

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfServiceDataSubopcode);
  view.filter_index().Write(filter_index);

  std::copy(
      value.begin(), value.end(), view.service_data().BackingStorage().data());

  UInt128 mask;
  mask.fill(std::numeric_limits<uint8_t>::max());
  std::copy(mask.begin(),
            mask.end(),
            view.service_data_mask().BackingStorage().data());

  return packet;
}

std::vector<CommandPacket>
AdvertisingPacketFilter::BuildSetServiceDataUUIDCommands(
    FilterIndex filter_index, const std::vector<UUID>& uuids) const {
  std::vector<CommandPacket> packets;
  packets.reserve(uuids.size());

  for (const UUID& uuid : uuids) {
    switch (uuid.type()) {
      case UUID::Type::k16Bit: {
        auto packet = BuildSetServiceDataUUID16Command(filter_index, uuid);
        if (!packet) {
          return packets;
        }
        packets.push_back(std::move(packet.value()));
        break;
      }
      case UUID::Type::k32Bit: {
        auto packet = BuildSetServiceDataUUID32Command(filter_index, uuid);
        if (!packet) {
          return packets;
        }
        packets.push_back(std::move(packet.value()));
        break;
      }
      case UUID::Type::k128Bit: {
        auto packet = BuildSetServiceDataUUID128Command(filter_index, uuid);
        packets.push_back(std::move(packet));
      }
    }
  }

  return packets;
}

CommandPacket AdvertisingPacketFilter::BuildSetLocalNameCommand(
    FilterIndex filter_index, const std::string& local_name) const {
  size_t packet_size =
      android_emb::LEApcfLocalNameCommand::MinSizeInBytes() + local_name.size();
  auto packet =
      hci::CommandPacket::New<android_emb::LEApcfLocalNameCommandWriter>(
          android_hci::kLEApcf, packet_size);
  auto view = packet.view_t(local_name.size());

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfLocalNameSubopcode);
  view.filter_index().Write(filter_index);

  std::copy(local_name.begin(),
            local_name.end(),
            view.local_name().BackingStorage().begin());

  return packet;
}

CommandPacket AdvertisingPacketFilter::BuildSetManufacturerCodeCommand(
    FilterIndex filter_index, uint16_t manufacturer_code) const {
  size_t packet_size =
      android_emb::LEApcfManufacturerDataCommand::MinSizeInBytes() +
      sizeof(manufacturer_code) * 2;
  auto packet =
      hci::CommandPacket::New<android_emb::LEApcfManufacturerDataCommandWriter>(
          android_hci::kLEApcf, packet_size);
  auto view = packet.view_t(sizeof(manufacturer_code));

  view.vendor_command().sub_opcode().Write(
      android_hci::kLEApcfManufacturerDataSubopcode);
  view.filter_index().Write(filter_index);

  view.manufacturer_data().BackingStorage().WriteLittleEndianUInt<16>(
      manufacturer_code);
  view.manufacturer_data_mask().BackingStorage().WriteLittleEndianUInt<16>(
      std::numeric_limits<uint16_t>::max());

  return packet;
}

}  // namespace bt::hci
