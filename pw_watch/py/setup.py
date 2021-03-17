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
"""pw_watch"""

import setuptools  # type: ignore

setuptools.setup(
    name='pw_watch',
    version='0.0.1',
    author='Pigweed Authors',
    author_email='pigweed-developers@googlegroups.com',
    description='Pigweed automatic builder',
    packages=setuptools.find_packages(),
    package_data={'pw_watch': ['py.typed']},
    zip_safe=False,
    install_requires=[
        'pw_cli',
        # Fixes the watchdog version to 0.10.3, released 2020-06-25
        # as versions later than this ignore the 'recursive' argument
        # on MacOS. This was causing us to trigger on any file within
        # the source tree, even those that should have been ignored.
        # See https://github.com/gorakhargosh/watchdog/issues/771.
        'watchdog==0.10.3',
    ],
)
