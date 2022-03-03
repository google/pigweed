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
"""Install and remove optional packages for Pigweed."""

import sys

from pw_package import package_manager
from pw_package.packages import arduino_core  # pylint: disable=unused-import
from pw_package.packages import boringssl  # pylint: disable=unused-import
from pw_package.packages import chromium_verifier  # pylint: disable=unused-import
from pw_package.packages import crlset  # pylint: disable=unused-import
from pw_package.packages import freertos  # pylint: disable=unused-import
from pw_package.packages import googletest  # pylint: disable=unused-import
from pw_package.packages import mbedtls  # pylint: disable=unused-import
from pw_package.packages import micro_ecc  # pylint: disable=unused-import
from pw_package.packages import nanopb
from pw_package.packages import pico_sdk  # pylint: disable=unused-import
from pw_package.packages import protobuf  # pylint: disable=unused-import
from pw_package.packages import smartfusion_mss  # pylint: disable=unused-import
from pw_package.packages import stm32cube  # pylint: disable=unused-import


def initialize():
    package_manager.register(nanopb.NanoPB)


def main(argv=None) -> int:
    initialize()
    return package_manager.run(**vars(package_manager.parse_args(argv)))


if __name__ == '__main__':
    sys.exit(main())
