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

import {RpcPacket} from 'packet_proto_tspb/packet_proto_tspb_pb/pw_rpc/internal/packet_pb'
import {Library} from 'pigweed/pw_protobuf_compiler/ts/proto_lib';

import {Client} from './client';
import {Channel} from './descriptors';

const TEST_PROTO_PATH = 'pw_rpc/ts/test_protos-descriptor-set.proto.bin';

describe('Client', () => {
  let lib: Library;
  let client: Client;

  beforeEach(async () => {
    lib = await Library.fromFileDescriptorSet(
        TEST_PROTO_PATH, 'test_protos_tspb');
    const channels = [new Channel(1), new Channel(5)];
    client = Client.fromProtoSet(channels, lib);
  });

  it('channel returns undefined for empty list', () => {
    const channels = Array<Channel>();
    const emptyChannelClient = Client.fromProtoSet(channels, lib);
    expect(emptyChannelClient.channel()).toBeUndefined();
  });

  it('fetches channel or returns undefined', () => {
    expect(client.channel(1)!.channel.id).toEqual(1);
    expect(client.channel(5)!.channel.id).toEqual(5);
    expect(client.channel()!.channel.id).toEqual(1);
    expect(client.channel(2)).toBeUndefined();
  });

  it('ChannelClient fetches method by name', () => {
    const channel = client.channel()!;
    const stub = channel.methodStub('pw.rpc.test1.TheTestService.SomeUnary')!;
    expect(stub.method.name).toEqual('SomeUnary');
  })

  it('ChannelClient for unknown name returns undefined', () => {
    const channel = client.channel()!;
    expect(channel.methodStub('')).toBeUndefined();
    expect(channel.methodStub('pw.rpc.test1.Garbage.SomeUnary'))
        .toBeUndefined();
    expect(channel.methodStub('pw.rpc.test1.TheTestService.Garbage'))
        .toBeUndefined();
  })
})
