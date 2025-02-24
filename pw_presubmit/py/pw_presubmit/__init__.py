# Copyright 2019 The Pigweed Authors
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
"""The pw_presubmit package provides tools for running presubmit checks."""

# This Python package contains generated Python modules that overlap with
# this `__init__.py` file's import namespace, so this package's import path
# must be extended for the generated modules to be discoverable.
#
# Note: This needs to be done in every nested `__init__.py` that will contain
# overlapping generated files.
__path__ = __import__('pkgutil').extend_path(__path__, __name__)

from pw_presubmit.tools import log_run
from pw_presubmit.presubmit import *
