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

import {Message} from 'google-protobuf';
import {PacketType, RpcPacket} from 'packet_proto_tspb/packet_proto_tspb_pb/pw_rpc/internal/packet_pb'
import {Library, MessageCreator} from 'pigweed/pw_protobuf_compiler/ts/proto_lib';
import {Status} from 'pigweed/pw_status/ts/status';
import {Request} from 'test_protos_tspb/test_protos_tspb_pb/pw_rpc/ts/test2_pb'

import {Client} from './client';
import {Channel, Method} from './descriptors';
import {ClientStreamingMethodStub, ServerStreamingMethodStub, UnaryMethodStub} from './method';
import * as packets from './packets';

const TEST_PROTO_PATH = 'pw_rpc/ts/test_protos-descriptor-set.proto.bin';

describe('Client', () => {
  let lib: Library;
  let client: Client;
  let lastPacketSent: RpcPacket;

  beforeEach(async () => {
    lib = await Library.fromFileDescriptorSet(
        TEST_PROTO_PATH, 'test_protos_tspb');
    const channels = [new Channel(1, savePacket), new Channel(5)];
    client = Client.fromProtoSet(channels, lib);
  });

  function savePacket(packetBytes: Uint8Array): void {
    lastPacketSent = RpcPacket.deserializeBinary(packetBytes);
  }

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

  it('processPacket with invalid proto data', () => {
    const textEncoder = new TextEncoder();
    const data = textEncoder.encode('NOT a packet!');
    expect(client.processPacket(data)).toEqual(Status.DATA_LOSS);
  });

  it('processPacket not for client', () => {
    const packet = new RpcPacket();
    packet.setType(PacketType.REQUEST);
    const processStatus = client.processPacket(packet.serializeBinary());
    expect(processStatus).toEqual(Status.INVALID_ARGUMENT);
  });

  it('processPacket for unrecognized channel', () => {
    const packet = packets.encodeResponse([123, 456, 789], new Request());
    expect(client.processPacket(packet)).toEqual(Status.NOT_FOUND);
  });

  it('processPacket for unrecognized service', () => {
    const packet = packets.encodeResponse([1, 456, 789], new Request());
    const status = client.processPacket(packet);
    expect(client.processPacket(packet)).toEqual(Status.OK);

    expect(lastPacketSent.getChannelId()).toEqual(1);
    expect(lastPacketSent.getServiceId()).toEqual(456);
    expect(lastPacketSent.getMethodId()).toEqual(789);
    expect(lastPacketSent.getType()).toEqual(PacketType.CLIENT_ERROR);
    expect(lastPacketSent.getStatus()).toEqual(Status.NOT_FOUND);
  });

  it('processPacket for unrecognized method', () => {
    const service = client.services.values().next().value;

    const packet = packets.encodeResponse([1, service.id, 789], new Request());
    const status = client.processPacket(packet);
    expect(client.processPacket(packet)).toEqual(Status.OK);

    expect(lastPacketSent.getChannelId()).toEqual(1);
    expect(lastPacketSent.getServiceId()).toEqual(service.id);
    expect(lastPacketSent.getMethodId()).toEqual(789);
    expect(lastPacketSent.getType()).toEqual(PacketType.CLIENT_ERROR);
    expect(lastPacketSent.getStatus()).toEqual(Status.NOT_FOUND);
  });

  it('processPacket for non-pending method', () => {
    const service = client.services.values().next().value;
    const method = service.methods.values().next().value;

    const packet =
        packets.encodeResponse([1, service.id, method.id], new Request());
    const status = client.processPacket(packet);
    expect(client.processPacket(packet)).toEqual(Status.OK);

    expect(lastPacketSent.getChannelId()).toEqual(1);
    expect(lastPacketSent.getServiceId()).toEqual(service.id);
    expect(lastPacketSent.getMethodId()).toEqual(method.id);
    expect(lastPacketSent.getType()).toEqual(PacketType.CLIENT_ERROR);
    expect(lastPacketSent.getStatus()).toEqual(Status.FAILED_PRECONDITION);
  });
});

describe('RPC', () => {
  let lib: Library;
  let client: Client;
  let lastPacketSent: RpcPacket;
  let requests: RpcPacket[] = [];
  let nextPackets: [Uint8Array, Status][] = [];
  let responseLock = false;

  beforeEach(async () => {
    lib = await Library.fromFileDescriptorSet(
        TEST_PROTO_PATH, 'test_protos_tspb');
    const channels = [new Channel(1, handlePacket), new Channel(2, () => {})];
    client = Client.fromProtoSet(channels, lib);
  });

  function enqueueResponse(
      channelId: number,
      method: Method,
      status: Status,
      payload: Uint8Array = new Uint8Array()) {
    const packet = new RpcPacket();
    packet.setType(PacketType.RESPONSE);
    packet.setChannelId(channelId);
    packet.setServiceId(method.service.id);
    packet.setMethodId(method.id);
    packet.setStatus(status);
    packet.setPayload(payload);

    nextPackets.push([packet.serializeBinary(), Status.OK]);
  }

  function enqueueServerStream(
      channelId: number,
      method: Method,
      response: Uint8Array,
      status: Status = Status.OK) {
    const packet = new RpcPacket();
    packet.setType(PacketType.SERVER_STREAM);
    packet.setChannelId(channelId);
    packet.setServiceId(method.service.id);
    packet.setMethodId(method.id);
    packet.setPayload(response);
    packet.setStatus(status);
    nextPackets.push([packet.serializeBinary(), status]);
  }

  function enqueueError(
      channelId: number,
      method: Method,
      status: Status,
      processStatus: Status) {
    const packet = new RpcPacket();
    packet.setType(PacketType.SERVER_ERROR);
    packet.setChannelId(channelId);
    packet.setServiceId(method.service.id);
    packet.setMethodId(method.id);
    packet.setStatus(status);

    nextPackets.push([packet.serializeBinary(), processStatus]);
  }

  function lastRequest(): RpcPacket {
    if (requests.length == 0) {
      throw Error('Tried to fetch request from empty list');
    }
    return requests[requests.length - 1];
  }

  function sentPayload(messageType: typeof Message): any {
    return messageType.deserializeBinary(lastRequest().getPayload_asU8());
  }

  function handlePacket(data: Uint8Array): void {
    requests.push(packets.decode(data));

    if (responseLock == true) {
      return;
    }

    // Avoid infinite recursion when processing a packet causes another packet
    // to send.
    responseLock = true;
    for (const [packet, status] of nextPackets) {
      expect(client.processPacket(packet)).toEqual(status);
    }
    nextPackets = [];
    responseLock = false;
  }

  describe('Unary', () => {
    let unaryStub: UnaryMethodStub;
    let request: any;
    let requestType: MessageCreator;
    let responseType: MessageCreator;

    beforeEach(async () => {
      unaryStub =
          client.channel()?.methodStub(
              'pw.rpc.test1.TheTestService.SomeUnary')! as UnaryMethodStub;
      requestType = unaryStub.method.requestType;
      responseType = unaryStub.method.responseType;
      request = new requestType();
    });


    it('nonblocking call', () => {
      for (let i = 0; i < 3; i++) {
        const response: any = new responseType();
        response.setPayload('hello world');
        const payload = response.serializeBinary();
        enqueueResponse(1, unaryStub.method, Status.ABORTED, payload);

        request.setMagicNumber(5);

        const onNext = jasmine.createSpy();
        const onCompleted = jasmine.createSpy();
        const onError = jasmine.createSpy();
        const call = unaryStub.invoke(request, onNext, onCompleted, onError);

        expect(sentPayload(unaryStub.method.requestType).getMagicNumber())
            .toEqual(5);
        expect(onNext).toHaveBeenCalledOnceWith(response);
        expect(onError).not.toHaveBeenCalled();
        expect(onCompleted).toHaveBeenCalledOnceWith(Status.ABORTED);
      }
    });

    it('nonblocking call cancel', () => {
      request.setMagicNumber(5);

      let onNext = jasmine.createSpy();
      const call = unaryStub.invoke(request, onNext);

      expect(requests.length).toBeGreaterThan(0);
      requests = [];

      expect(call.cancel()).toBeTrue();
      expect(call.cancel()).toBeFalse();
      expect(onNext).not.toHaveBeenCalled();
    });

    it('nonblocking duplicate calls first is cancelled', () => {
      const firstCall = unaryStub.invoke(request);
      expect(firstCall.completed()).toBeFalse();

      const secondCall = unaryStub.invoke(request);
      expect(firstCall.error).toEqual(Status.CANCELLED);
      expect(secondCall.completed()).toBeFalse();
    });

    it('nonblocking exception in callback', () => {
      const errorCallback = () => {
        throw Error('Something went wrong!');
      };

      enqueueResponse(1, unaryStub.method, Status.OK);
      const call = unaryStub.invoke(request, errorCallback);
      expect(call.callbackException!.name).toEqual('Error');
      expect(call.callbackException!.message).toEqual('Something went wrong!');
    });
  })

  describe('ServerStreaming', () => {
    let serverStreaming: ServerStreamingMethodStub;
    let request: any;
    let requestType: any;
    let responseType: any;

    beforeEach(async () => {
      serverStreaming =
          client.channel()?.methodStub(
              'pw.rpc.test1.TheTestService.SomeServerStreaming')! as
          ServerStreamingMethodStub;
      requestType = serverStreaming.method.requestType;
      responseType = serverStreaming.method.responseType;
      request = new requestType();
    });

    it('non-blocking call', () => {
      const response1 = new responseType();
      response1.setPayload('!!!');
      const response2 = new responseType();
      response2.setPayload('?');

      const request = new requestType()
      request.setMagicNumber(4);

      for (let i = 0; i < 3; i++) {
        enqueueServerStream(
            1, serverStreaming.method, response1.serializeBinary());
        enqueueServerStream(
            1, serverStreaming.method, response2.serializeBinary());
        enqueueResponse(1, serverStreaming.method, Status.ABORTED);

        const onNext = jasmine.createSpy();
        const onCompleted = jasmine.createSpy();
        const onError = jasmine.createSpy();
        serverStreaming.invoke(request, onNext, onCompleted, onError);

        expect(onNext).toHaveBeenCalledWith(response1);
        expect(onNext).toHaveBeenCalledWith(response2);
        expect(onError).not.toHaveBeenCalled();
        expect(onCompleted).toHaveBeenCalledOnceWith(Status.ABORTED);

        expect(sentPayload(serverStreaming.method.requestType).getMagicNumber())
            .toEqual(4);
      }
    });

    it('non-blocking cancel', () => {
      const response = new responseType();
      response.setPayload('!!!');
      enqueueServerStream(
          1, serverStreaming.method, response.serializeBinary());
      const request = new requestType();
      request.setMagicNumber(3);

      const onNext = jasmine.createSpy();
      const onCompleted = jasmine.createSpy();
      const onError = jasmine.createSpy();
      let call = serverStreaming.invoke(request, onNext);
      expect(onNext).toHaveBeenCalledOnceWith(response);

      onNext.calls.reset();

      call.cancel();
      expect(lastRequest().getType()).toEqual(PacketType.CANCEL);

      // Ensure the RPC can be called after being cancelled.
      enqueueServerStream(
          1, serverStreaming.method, response.serializeBinary());
      enqueueResponse(1, serverStreaming.method, Status.OK);
      call = serverStreaming.invoke(request, onNext, onCompleted, onError);
      expect(onNext).toHaveBeenCalledWith(response);
      expect(onError).not.toHaveBeenCalled();
      expect(onCompleted).toHaveBeenCalledOnceWith(Status.OK);
    });
  });

  describe('ClientStreaming', () => {
    let clientStreaming: ClientStreamingMethodStub;
    let requestType: any;
    let responseType: any;

    beforeEach(async () => {
      clientStreaming =
          client.channel()?.methodStub(
              'pw.rpc.test1.TheTestService.SomeClientStreaming')! as
          ClientStreamingMethodStub;
      requestType = clientStreaming.method.requestType;
      responseType = clientStreaming.method.responseType;
    });

    it('non-blocking call', () => {
      const response = new responseType();
      response.setPayload('-_-');

      for (let i = 0; i < 3; i++) {
        const onNext = jasmine.createSpy();
        const stream = clientStreaming.invoke(onNext);
        expect(stream.completed()).toBeFalse();

        let request = new requestType();
        request.setMagicNumber(31);
        stream.send(request);
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_STREAM);
        expect(sentPayload(requestType).getMagicNumber()).toEqual(31);
        expect(stream.completed()).toBeFalse();

        // Enqueue the server response to be sent after the next message.
        enqueueResponse(
            1, clientStreaming.method, Status.OK, response.serializeBinary());

        request.setMagicNumber(32);
        stream.send(request);
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_STREAM);
        expect(sentPayload(requestType).getMagicNumber()).toEqual(32);

        expect(onNext).toHaveBeenCalledOnceWith(response);
        expect(stream.completed()).toBeTrue();
        expect(stream.status).toEqual(Status.OK);
        expect(stream.error).toBeUndefined();
      }
    });

    it('non-blocking call ended by client', () => {
      const response = new responseType();
      response.setPayload('-_-');

      for (let i = 0; i < 3; i++) {
        const onNext = jasmine.createSpy();
        const stream = clientStreaming.invoke(onNext);
        expect(stream.completed()).toBeFalse();

        let request = new requestType();
        request.setMagicNumber(31);
        stream.send(request);
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_STREAM);
        expect(sentPayload(requestType).getMagicNumber()).toEqual(31);
        expect(stream.completed()).toBeFalse();

        // Enqueue the server response to be sent after the next message.
        enqueueResponse(
            1, clientStreaming.method, Status.OK, response.serializeBinary());

        stream.finishAndWait();
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_STREAM_END);

        expect(onNext).toHaveBeenCalledOnceWith(response);
        expect(stream.completed()).toBeTrue();
        expect(stream.status).toEqual(Status.OK);
        expect(stream.error).toBeUndefined();
      }
    });

    it('non-blocking call cancelled', () => {
      for (let i = 0; i < 3; i++) {
        const stream = clientStreaming.invoke();
        let request = new requestType();
        request.setMagicNumber(31);
        stream.send(request)

        expect(stream.cancel()).toBeTrue();
        expect(lastRequest().getType()).toEqual(PacketType.CANCEL);
        expect(stream.cancel()).toBeFalse();
        expect(stream.completed()).toBeTrue();
        expect(stream.error).toEqual(Status.CANCELLED);
      }
    });

    it('non-blocking call server error', async () => {
      for (let i = 0; i < 3; i++) {
        const stream = clientStreaming.invoke();
        enqueueError(
            1, clientStreaming.method, Status.INVALID_ARGUMENT, Status.OK);

        const request = new requestType();
        request.setMagicNumber(31);
        stream.send(request);

        await stream.finishAndWait()
            .then(() => {
              fail('Promise should not be resolved');
            })
            .catch((reason) => {
              expect(reason.status).toEqual(Status.INVALID_ARGUMENT);
            });
      }
    });

    it('non-blocking call server error after stream end', async () => {
      for (let i = 0; i < 3; i++) {
        const stream = clientStreaming.invoke();
        // Error will be sent in response to the CLIENT_STREAM_END packet.
        enqueueError(
            1, clientStreaming.method, Status.INVALID_ARGUMENT, Status.OK);

        await stream.finishAndWait()
            .then(() => {
              fail('Promise should not be resolved');
            })
            .catch((reason) => {
              expect(reason.status).toEqual(Status.INVALID_ARGUMENT);
            });
      }
    });

    it('non-blocking call send after cancelled', () => {
      const stream = clientStreaming.invoke();
      expect(stream.cancel()).toBeTrue();

      const request = new requestType();
      request.setMagicNumber(31);
      expect(() => stream.send(request))
          .toThrowMatching(error => error.status === Status.CANCELLED);
    });

    it('non-blocking finish after completed', async () => {
      const enqueuedResponse = new responseType();
      enqueuedResponse.setPayload('?!');
      enqueueResponse(
          1,
          clientStreaming.method,
          Status.UNAVAILABLE,
          enqueuedResponse.serializeBinary());

      const stream = clientStreaming.invoke();
      const result = await stream.finishAndWait();
      expect(result[1]).toEqual([enqueuedResponse]);

      expect(await stream.finishAndWait()).toEqual(result);
      expect(await stream.finishAndWait()).toEqual(result);
    });

    it('non-blocking finish after error', async () => {
      enqueueError(1, clientStreaming.method, Status.UNAVAILABLE, Status.OK);
      const stream = clientStreaming.invoke();

      for (let i = 0; i < 3; i++) {
        await stream.finishAndWait()
            .then(() => {
              fail('Promise should not be resolved');
            })
            .catch((reason) => {
              expect(reason.status).toEqual(Status.UNAVAILABLE);
              expect(stream.error).toEqual(Status.UNAVAILABLE);
              expect(stream.response).toBeUndefined();
            });
      }
    });

    it('non-blocking duplicate calls first is cancelled', () => {
      const firstCall = clientStreaming.invoke();
      expect(firstCall.completed()).toBeFalse();

      const secondCall = clientStreaming.invoke();
      expect(firstCall.error).toEqual(Status.CANCELLED);
      expect(secondCall.completed()).toBeFalse();
    });
  });
});
