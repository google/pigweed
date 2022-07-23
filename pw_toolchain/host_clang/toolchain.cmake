# Copyright 2020 The Pigweed Authors
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

# Do not rely on the PW_ROOT environment variable being set through bootstrap.
# Regardless of whether it's set or not the following include will ensure it is.
include(${CMAKE_CURRENT_LIST_DIR}/../../pw_build/pigweed.cmake)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# TODO(pwbug/606): set up this facade in CMake
# Use logging-based test output on host.
# pw_set_backend(pw_unit_test.main pw_unit_test.logging_main)

# Configure backend for assert facade.
pw_set_backend(pw_assert pw_assert_basic)

# Configure backend for logging facade.
pw_set_backend(pw_log pw_log_basic)

# Configure backends for pw_sync's facades.
pw_set_backend(pw_sync.interrupt_spin_lock pw_sync_stl.interrupt_spin_lock)
pw_set_backend(pw_sync.binary_semaphore pw_sync_stl.binary_semaphore_backend)
pw_set_backend(pw_sync.counting_semaphore
               pw_sync_stl.counting_semaphore_backend)
pw_set_backend(pw_sync.mutex pw_sync_stl.mutex_backend)
pw_set_backend(pw_sync.timed_mutex pw_sync_stl.timed_mutex_backend)
pw_set_backend(pw_sync.thread_notification
               pw_sync.binary_semaphore_thread_notification_backend)
pw_set_backend(pw_sync.timed_thread_notification
               pw_sync.binary_semaphore_timed_thread_notification_backend)

# Configure backend for pw_sys_io facade.
pw_set_backend(pw_sys_io pw_sys_io_stdio)

# Configure backend for pw_rpc_system_server.
pw_set_backend(pw_rpc.system_server targets.host.system_rpc_server)
# TODO(hepler): set config to use global mutex

# Configure backend for pw_chrono's facades.
pw_set_backend(pw_chrono.system_clock pw_chrono_stl.system_clock)
pw_set_backend(pw_chrono.system_timer pw_chrono_stl.system_timer)

# Configure backends for pw_thread's facades.
pw_set_backend(pw_thread.id pw_thread_stl.id)
pw_set_backend(pw_thread.yield pw_thread_stl.yield)
pw_set_backend(pw_thread.sleep pw_thread_stl.sleep)
pw_set_backend(pw_thread.thread pw_thread_stl.thread)

# TODO: Migrate this to match GN's tokenized trace setup.
pw_set_backend(pw_trace pw_trace.null)

# The CIPD provided Clang/LLVM toolchain must link against the matched
# libc++ which is also from CIPD. However, by default, Clang on Mac (but
# not on Linux) will fall back to the system libc++, which is
# incompatible due to an ABI change.
#
# Pull the appropriate paths from our Pigweed env setup.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib++ $ENV{PW_PIGWEED_CIPD_INSTALL_DIR}/lib/libc++.a")
endif()

set(pw_build_WARNINGS pw_build.strict_warnings pw_build.extra_strict_warnings
    CACHE STRING "" FORCE)
