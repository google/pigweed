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

FROM ubuntu:19.10
RUN apt-get update &&\
    apt-get install -y \
        python \
        git

ENV CIPD_CACHE_DIR /pigweed-cache/cipd-cache-dir
# This is only for seeding the environment, not meant to be used. Running
# bootstrap inside another checkout will reset PW_ROOT but leave
# PW_ENVIRONMENT_ROOT alone.
ENV PW_ROOT /pigweed-cache/pigweed
ENV PW_ENVIRONMENT_ROOT /pigweed-cache/environment
ENV PW_CIPD_PACKAGE_FILES "$PW_ROOT/pw_env_setup/py/pw_env_setup/cipd_setup/*.json"

# --shell-file is required, but we're going to ignore it.
RUN git clone https://pigweed.googlesource.com/pigweed/pigweed $PW_ROOT && \
    $PW_ROOT/pw_env_setup/py/pw_env_setup/env_setup.py \
    --shell-file $PW_ROOT/init.sh \
    --pw-root $PW_ROOT \
    --install-dir $PW_ENVIRONMENT_ROOT

CMD /bin/bash
