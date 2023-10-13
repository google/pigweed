# Copyright 2023 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
include($ENV{PW_ROOT}/pw_async/backend.cmake)

pw_add_library(pw_async.types INTERFACE
  HEADERS
    public/pw_async/context.h
    public/pw_async/task_function.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_function
    pw_status
)

pw_add_facade(pw_async.task INTERFACE
  BACKEND
    pw_async.task_BACKEND
  HEADERS
    public/pw_async/task.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_async.types
    pw_function
    pw_status
)

pw_add_facade(pw_async.dispatcher INTERFACE
  BACKEND
    pw_async.dispatcher_BACKEND
  HEADERS
    public/pw_async/dispatcher.h
    public/pw_async/function_dispatcher.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_async.types
    pw_chrono.system_clock
    pw_function
    pw_status
)
