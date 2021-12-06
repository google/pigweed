// Copyright 2020 The Pigweed Authors
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

// clang-format off
#include "pw_log_rpc/internal/log_config.h" // PW_LOG_* macros must be first.

#include "pw_log_rpc/log_service.h"
// clang-format on

#include "pw_log/log.h"
#include "pw_log/proto/log.pwpb.h"
#include "pw_log_rpc/log_filter.h"
#include "pw_protobuf/decoder.h"

namespace pw::log_rpc {

void LogService::Listen(ConstByteSpan, rpc::RawServerWriter& writer) {
  uint32_t channel_id = writer.channel_id();
  Result<RpcLogDrain*> drain = drains_.GetDrainFromChannelId(channel_id);
  if (!drain.ok()) {
    return;
  }

  if (const Status status = drain.value()->Open(writer); !status.ok()) {
    PW_LOG_DEBUG("Could not start new log stream. %d",
                 static_cast<int>(status.code()));
  }
}

StatusWithSize LogService::SetFilter(ConstByteSpan request, ByteSpan) {
  if (filters_ == nullptr) {
    return StatusWithSize::NotFound();
  }

  protobuf::Decoder decoder(request);
  PW_TRY_WITH_SIZE(decoder.Next());
  if (static_cast<log::SetFilterRequest::Fields>(decoder.FieldNumber()) !=
      log::SetFilterRequest::Fields::FILTER_ID) {
    return StatusWithSize::InvalidArgument();
  }
  ConstByteSpan filter_id;
  PW_TRY_WITH_SIZE(decoder.ReadBytes(&filter_id));
  Result<Filter*> filter = filters_->GetFilterFromId(filter_id);
  if (!filter.ok()) {
    return StatusWithSize::NotFound();
  }

  PW_TRY_WITH_SIZE(decoder.Next());
  ConstByteSpan filter_buffer;
  if (static_cast<log::SetFilterRequest::Fields>(decoder.FieldNumber()) !=
      log::SetFilterRequest::Fields::FILTER) {
    return StatusWithSize::InvalidArgument();
  }
  PW_TRY_WITH_SIZE(decoder.ReadBytes(&filter_buffer));
  PW_TRY_WITH_SIZE(filter.value()->UpdateRulesFromProto(filter_buffer));
  return StatusWithSize();
}

StatusWithSize LogService::GetFilter(ConstByteSpan request, ByteSpan response) {
  if (filters_ == nullptr) {
    return StatusWithSize::NotFound();
  }
  protobuf::Decoder decoder(request);
  PW_TRY_WITH_SIZE(decoder.Next());
  if (static_cast<log::GetFilterRequest::Fields>(decoder.FieldNumber()) !=
      log::GetFilterRequest::Fields::FILTER_ID) {
    return StatusWithSize::InvalidArgument();
  }
  ConstByteSpan filter_id;
  PW_TRY_WITH_SIZE(decoder.ReadBytes(&filter_id));
  Result<Filter*> filter = filters_->GetFilterFromId(filter_id);
  if (!filter.ok()) {
    return StatusWithSize::NotFound();
  }

  log::Filter::MemoryEncoder encoder(response);
  for (auto& rule : (*filter)->rules()) {
    log::FilterRule::StreamEncoder rule_encoder = encoder.GetRuleEncoder();
    rule_encoder.WriteLevelGreaterThanOrEqual(rule.level_greater_than_or_equal)
        .IgnoreError();
    rule_encoder.WriteModuleEquals(rule.module_equals).IgnoreError();
    rule_encoder.WriteAnyFlagsSet(rule.any_flags_set).IgnoreError();
    rule_encoder.WriteAction(static_cast<log::FilterRule::Action>(rule.action))
        .IgnoreError();
    PW_TRY_WITH_SIZE(rule_encoder.status());
  }
  PW_TRY_WITH_SIZE(encoder.status());

  return StatusWithSize(encoder.size());
}

StatusWithSize LogService::ListFilterIds(ConstByteSpan, ByteSpan response) {
  if (filters_ == nullptr) {
    return StatusWithSize::NotFound();
  }
  log::FilterIdListResponse::MemoryEncoder encoder(response);
  for (auto& filter : filters_->filters()) {
    PW_TRY_WITH_SIZE(encoder.WriteFilterId(filter.id()));
  }
  return StatusWithSize(encoder.size());
}

}  // namespace pw::log_rpc
