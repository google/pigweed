#!/bin/bash

# Copyright 2024 The Pigweed Authors
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

# See:
# - https://bazel.build/docs/user-manual#workspace-status-command
# - https://github.com/bazelbuild/bazel/blob/a95f37bb038f/tools/buildstamp/get_workspace_status

# This script will be run bazel when building process starts to
# generate key-value information that represents the status of the
# workspace. The output should be like
#
# KEY1 VALUE1
# KEY2 VALUE2
#
# If the script exits with non-zero code, it's considered as a failure
# and the output will be discarded.
#
# Note that keys starting with "STABLE_" are part of the stable set, which if
# changed, invalidate any stampted targets. Keys which do not start with
# "STABLE_" are part of the volatile set, which will be used but do not
# invalidate stamped targets.

# Current git commit
git_rev=$(git rev-parse HEAD) || exit 1
echo "STABLE_GIT_COMMIT ${git_rev}"

# Check whether there are any uncommitted changes
git diff-index --quiet HEAD --; diff_rc=$?
case $diff_rc in
    0) dirty=0 ;;
    1) dirty=1 ;;
    *) exit 1 ;;
esac
echo "STABLE_GIT_TREE_DIRTY ${dirty}"
