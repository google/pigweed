// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

/* eslint-env browser, jasmine */
import 'jasmine';

import {Message} from 'test_protos_tspb/test_protos_tspb_pb/pw_protobuf_compiler/pw_protobuf_compiler_protos/nested/more_nesting/test_pb';

import {Library} from './proto_lib';

const DESCRIPTOR_BINARY_PATH =
    'pw_protobuf_compiler/test_protos-descriptor-set.proto.bin';
const TEST_LIB_NAME = 'test_protos_tspb';

describe('Library', () => {
  it('constructed from invalid paths rejects promise', async () => {
    await expectAsync(Library.fromFileDescriptorSet('garbage', TEST_LIB_NAME))
        .toBeRejected();
    await expectAsync(
        Library.fromFileDescriptorSet(DESCRIPTOR_BINARY_PATH, 'garbage'))
        .toBeRejected();
  });

  it('getMessageType returns message', async () => {
    const lib = await Library.fromFileDescriptorSet(
        DESCRIPTOR_BINARY_PATH, TEST_LIB_NAME);

    let fetched = lib.getMessageCreator('pw.protobuf_compiler.test.Message');
    expect(fetched).toEqual(Message);
  });

  it('getMessageType for invalid identifier returns undefined', async () => {
    const lib = await Library.fromFileDescriptorSet(
        DESCRIPTOR_BINARY_PATH, TEST_LIB_NAME);

    let fetched = lib.getMessageCreator('pw');
    expect(fetched).toBeUndefined();
    fetched = lib.getMessageCreator('pw.test1.Garbage');
    expect(fetched).toBeUndefined();
  });
});
