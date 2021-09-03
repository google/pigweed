# Copyright 2021 The Pigweed Authors
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
"""Facilities for generating TUF target metadata."""

import hashlib
from typing import Dict, Iterable

from pw_software_update.tuf_pb2 import (CommonMetadata, Hash, HashFunction,
                                        TargetFile, TargetsMetadata)

HASH_FACTORIES = {
    HashFunction.SHA256: hashlib.sha256,
}
DEFAULT_HASHES = (HashFunction.SHA256, )


def gen_targets_metadata(
    target_payloads: Dict[str, bytes],
    hash_funcs: Iterable['HashFunction.V'] = DEFAULT_HASHES
) -> TargetsMetadata:
    """Generates TargetsMetadata the given target payloads."""
    target_files = []
    for target_file_name, target_payload in target_payloads.items():
        target_files.append(
            TargetFile(file_name=target_file_name,
                       length=len(target_payload),
                       hashes=gen_hashes(target_payload, hash_funcs)))

    return TargetsMetadata(common_metadata=gen_commmon_metadata(),
                           target_files=target_files)


def gen_hashes(data: bytes,
               hash_funcs: Iterable['HashFunction.V']) -> Iterable[Hash]:
    """Computes all the specified hashes over the data."""
    result = []
    for func in hash_funcs:
        if func == HashFunction.UNKNOWN_HASH_FUNCTION:
            raise ValueError(
                'UNKNOWN_HASH_FUNCTION cannot be used to generate hashes.')
        digest = HASH_FACTORIES[func](data).digest()
        result.append(Hash(function=func, hash=digest))

    return result


def gen_commmon_metadata() -> CommonMetadata:
    """Generates CommonMetadata."""
    # TODO(jethier): Figure out where common metadata should actually come from.
    return CommonMetadata(spec_version="0.0.0", version=0)
