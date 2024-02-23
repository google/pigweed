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
"""pw_log Python libraries."""

# The generated protos for this module overlap this `__init__.py` file's import
# namespace, so we need to use extend_path() for them to be discoverable.
# Note: this needs to be done in every nested `__init__.py` file as well (if
# any exist).
__path__ = __import__('pkgutil').extend_path(__path__, __name__)
