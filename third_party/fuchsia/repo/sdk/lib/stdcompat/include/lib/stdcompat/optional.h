// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_STDCOMPAT_OPTIONAL_H_
#define LIB_STDCOMPAT_OPTIONAL_H_

#include <optional>

#include "utility.h"
#include "version.h"

namespace cpp17 {

using std::bad_optional_access;
using std::make_optional;
using std::nullopt;
using std::nullopt_t;
using std::optional;

}  // namespace cpp17

#endif  // LIB_STDCOMPAT_OPTIONAL_H_
