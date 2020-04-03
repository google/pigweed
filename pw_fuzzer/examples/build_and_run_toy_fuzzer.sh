#! /bin/bash

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

# TODO(pwbug/177): Pythonize this, or otherwise replace it.

set -e

OUT_DIR="out/host"

confirm() {
  echo
  while [ -n "$1" ] ; do
    echo "$1"
    shift
  done
  echo
  read -p "Do you wish to continue? [yN]" confirm
  case $confirm in
      [Yy]* ) ;;
      * ) exit;;
  esac
  echo
}

confirm "This script builds and runs a example fuzzer." \
        "This script is about to delete $OUT_DIR."
set -x
rm -rf $OUT_DIR

mkdir -p $OUT_DIR
echo 'pw_target_toolchain="//pw_toolchain:host_clang_og"' > $OUT_DIR/args.gn
echo 'pw_sanitizer="address"' >> $OUT_DIR/args.gn
gn gen $OUT_DIR
ninja -C $OUT_DIR pw_module_fuzzers

set +x
confirm "The toy_fuzzer was built successfully!" \
        "This script is about to start fuzzing."
set -x
$OUT_DIR/obj/pw_fuzzer/toy_fuzzer
