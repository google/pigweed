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
#include "chre/platform/platform_nanoapp.h"

#include "chre/util/system/napp_permissions.h"
#include "chre_api/chre/version.h"

namespace chre {

PlatformNanoapp::~PlatformNanoapp() {}

bool PlatformNanoapp::start() { return app_info_->entryPoints.start(); }

void PlatformNanoapp::handleEvent(uint32_t SenderInstanceId,
                                  uint16_t eventType,
                                  const void* eventData) {
  app_info_->entryPoints.handleEvent(SenderInstanceId, eventType, eventData);
}

void PlatformNanoapp::end() { app_info_->entryPoints.end(); }

uint64_t PlatformNanoapp::getAppId() const {
  return (app_info_ == nullptr) ? 0 : app_info_->appId;
}

uint32_t PlatformNanoapp::getAppVersion() const {
  return app_info_->appVersion;
}

uint32_t PlatformNanoapp::getTargetApiVersion() const {
  return CHRE_API_VERSION;
}

const char* PlatformNanoapp::getAppName() const {
  return (app_info_ != nullptr) ? app_info_->name : "Unknown";
}

bool PlatformNanoapp::supportsAppPermissions() const {
  return (app_info_ != nullptr) ? (app_info_->structMinorVersion >=
                                   CHRE_NSL_NANOAPP_INFO_STRUCT_MINOR_VERSION)
                                : false;
}

uint32_t PlatformNanoapp::getAppPermissions() const {
  return (supportsAppPermissions())
             ? app_info_->appPermissions
             : static_cast<uint32_t>(chre::NanoappPermissions::CHRE_PERMS_NONE);
}

bool PlatformNanoapp::isSystemNanoapp() const {
  return (app_info_ != nullptr && app_info_->isSystemNanoapp);
}

void PlatformNanoapp::logStateToBuffer(DebugDumpWrapper& debugDump) const {
  if (!app_info_) {
    return;
  }
  debugDump.print("%s: %s", app_info_->name, app_info_->vendor);
}

void PlatformNanoappBase::loadStatic(
    const struct chreNslNanoappInfo* app_info) {
  app_info_ = app_info;
}

}  // namespace chre
