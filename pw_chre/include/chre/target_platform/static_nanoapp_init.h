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

#pragma once

#include "chre/core/static_nanoapps.h"
#include "chre/platform/fatal_error.h"
#include "chre/platform/shared/nanoapp_support_lib_dso.h"

#define CHRE_STATIC_NANOAPP_INIT(appName, appId_, appVersion_, appPerms)     \
  namespace chre {                                                           \
                                                                             \
  UniquePtr<Nanoapp> initializeStaticNanoapp##appName() {                    \
    UniquePtr<Nanoapp> nanoapp = MakeUnique<Nanoapp>();                      \
    static struct chreNslNanoappInfo appInfo;                                \
    appInfo.magic = CHRE_NSL_NANOAPP_INFO_MAGIC;                             \
    appInfo.structMinorVersion = CHRE_NSL_NANOAPP_INFO_STRUCT_MINOR_VERSION; \
    appInfo.targetApiVersion = CHRE_API_VERSION;                             \
    appInfo.vendor = "Google";                                               \
    appInfo.name = #appName;                                                 \
    appInfo.isSystemNanoapp = true;                                          \
    appInfo.isTcmNanoapp = false;                                            \
    appInfo.appId = appId_;                                                  \
    appInfo.appVersion = appVersion_;                                        \
    appInfo.entryPoints.start = nanoappStart;                                \
    appInfo.entryPoints.handleEvent = nanoappHandleEvent;                    \
    appInfo.entryPoints.end = nanoappEnd;                                    \
    appInfo.appVersionString = "<undefined>";                                \
    appInfo.appPermissions = appPerms;                                       \
    if (nanoapp.isNull()) {                                                  \
      FATAL_ERROR("Failed to allocate nanoapp " #appName);                   \
    } else {                                                                 \
      nanoapp->loadStatic(&appInfo);                                         \
    }                                                                        \
                                                                             \
    return nanoapp;                                                          \
  }                                                                          \
                                                                             \
  }  // namespace chre
