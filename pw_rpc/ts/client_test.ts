// Copyright 2022 The Pigweed Authors
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

/* eslint-env browser */

import { Status } from 'pigweedjs/pw_status';
import { MessageCreator } from 'pigweedjs/pw_protobuf_compiler';
import { Message } from 'google-protobuf';
import {
  PacketType,
  RpcPacket,
} from 'pigweedjs/protos/pw_rpc/internal/packet_pb';
import { ProtoCollection } from 'pigweedjs/protos/collection';
import { Request, Response } from 'pigweedjs/protos/pw_rpc/ts/test_pb';

import { Client } from './client';
import { Channel, Method } from './descriptors';
import {
  BidirectionalStreamingMethodStub,
  ClientStreamingMethodStub,
  ServerStreamingMethodStub,
  UnaryMethodStub,
} from './method';
import * as packets from './packets';

const LEGACY_OPEN_CALL_ID = 0;
const OPEN_CALL_ID = 2 ** 32 - 1;

describe('Client', () => {
  let protoCollection: ProtoCollection;
  let client: Client;
  let lastPacketSent: RpcPacket;

  beforeEach(() => {
    protoCollection = new ProtoCollection();
    const channels = [new Channel(1, savePacket), new Channel(5)];
    client = Client.fromProtoSet(channels, protoCollection);
  });

  function savePacket(packetBytes: Uint8Array): void {
    lastPacketSent = RpcPacket.deserializeBinary(packetBytes);
  }

  it('channel returns undefined for empty list', () => {
    const channels = Array<Channel>();
    const emptyChannelClient = Client.fromProtoSet(channels, protoCollection);
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
  });

  it('ChannelClient for unknown name returns undefined', () => {
    const channel = client.channel()!;
    expect(channel.methodStub('')).toBeUndefined();
    expect(
      channel.methodStub('pw.rpc.test1.Garbage.SomeUnary'),
    ).toBeUndefined();
    expect(
      channel.methodStub('pw.rpc.test1.TheTestService.Garbage'),
    ).toBeUndefined();
  });

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
    const packet = packets.encodeResponse([123, 456, 789, 456], new Request());
    expect(client.processPacket(packet)).toEqual(Status.NOT_FOUND);
  });

  it('processPacket for unrecognized service', () => {
    const packet = packets.encodeResponse([1, 456, 789, 456], new Request());
    const status = client.processPacket(packet);
    expect(client.processPacket(packet)).toEqual(Status.OK);

    expect(lastPacketSent.getChannelId()).toEqual(1);
    expect(lastPacketSent.getServiceId()).toEqual(456);
    expect(lastPacketSent.getMethodId()).toEqual(789);
    expect(lastPacketSent.getCallId()).toEqual(456);
    expect(lastPacketSent.getType()).toEqual(PacketType.CLIENT_ERROR);
    expect(lastPacketSent.getStatus()).toEqual(Status.NOT_FOUND);
  });

  it('processPacket for unrecognized method', () => {
    const service = client.services.values().next().value;

    const packet = packets.encodeResponse(
      [1, service.id, 789, 456],
      new Request(),
    );
    const status = client.processPacket(packet);
    expect(client.processPacket(packet)).toEqual(Status.OK);

    expect(lastPacketSent.getChannelId()).toEqual(1);
    expect(lastPacketSent.getServiceId()).toEqual(service.id);
    expect(lastPacketSent.getMethodId()).toEqual(789);
    expect(lastPacketSent.getCallId()).toEqual(456);
    expect(lastPacketSent.getType()).toEqual(PacketType.CLIENT_ERROR);
    expect(lastPacketSent.getStatus()).toEqual(Status.NOT_FOUND);
  });

  it('processPacket for non-pending method', () => {
    const service = client.services.values().next().value;
    const method = service.methods.values().next().value;

    const packet = packets.encodeResponse(
      [1, service.id, method.id, 456],
      new Request(),
    );
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
  let protoCollection: ProtoCollection;
  let client: Client;
  let lastPacketSent: RpcPacket | undefined;
  let requests: RpcPacket[] = [];
  let nextPackets: [Uint8Array, Status][] = [];
  let responseLock = false;
  let sendResponsesAfterPackets = 0;
  let outputException: Error | undefined;

  beforeEach(async () => {
    protoCollection = new ProtoCollection();
    const channels = [
      new Channel(1, handlePacket),
      new Channel(2, () => {
        // Do nothing.
      }),
    ];
    client = Client.fromProtoSet(channels, protoCollection);
    lastPacketSent = undefined;
    requests = [];
    nextPackets = [];
    responseLock = false;
    sendResponsesAfterPackets = 0;
    outputException = undefined;
  });

  function newRequest(magicNumber = 123): Message {
    const request = new Request();
    request.setMagicNumber(magicNumber);
    return request;
  }

  function newResponse(payload = '._.'): Message {
    const response = new Response();
    response.setPayload(payload);
    return response;
  }

  function enqueueResponse(
    channelId: number,
    method: Method,
    status: Status,
    callId: number,
    response?: Message,
  ) {
    const packet = new RpcPacket();
    packet.setType(PacketType.RESPONSE);
    packet.setChannelId(channelId);
    packet.setServiceId(method.service.id);
    packet.setMethodId(method.id);
    packet.setCallId(callId);
    packet.setStatus(status);
    if (response === undefined) {
      packet.setPayload(new Uint8Array(0));
    } else {
      packet.setPayload(response.serializeBinary());
    }
    nextPackets.push([packet.serializeBinary(), Status.OK]);
  }

  function enqueueServerStream(
    channelId: number,
    method: Method,
    response: Message,
    callId: number,
    status: Status = Status.OK,
  ) {
    const packet = new RpcPacket();
    packet.setType(PacketType.SERVER_STREAM);
    packet.setChannelId(channelId);
    packet.setServiceId(method.service.id);
    packet.setMethodId(method.id);
    packet.setCallId(callId);
    packet.setPayload(response.serializeBinary());
    packet.setStatus(status);
    nextPackets.push([packet.serializeBinary(), status]);
  }

  function enqueueError(
    channelId: number,
    method: Method,
    status: Status,
    processStatus: Status,
    callId: number,
  ) {
    const packet = new RpcPacket();
    packet.setType(PacketType.SERVER_ERROR);
    packet.setChannelId(channelId);
    packet.setServiceId(method.service.id);
    packet.setMethodId(method.id);
    packet.setCallId(callId);
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
    if (outputException !== undefined) {
      throw outputException;
    }
    requests.push(packets.decode(data));

    if (sendResponsesAfterPackets > 1) {
      sendResponsesAfterPackets -= 1;
      return;
    }

    processEnqueuedPackets();
  }

  function processEnqueuedPackets(): void {
    // Avoid infinite recursion when processing a packet causes another packet
    // to send.
    if (responseLock) return;
    responseLock = true;
    for (const [packet, status] of nextPackets) {
      expect(client.processPacket(packet)).toEqual(status);
    }
    nextPackets = [];
    responseLock = false;
  }

  describe('Unary', () => {
    let unaryStub: UnaryMethodStub;

    beforeEach(async () => {
      unaryStub = client
        .channel()
        ?.methodStub(
          'pw.rpc.test1.TheTestService.SomeUnary',
        ) as UnaryMethodStub;
    });

    const openCallIds = [
      ['OPEN_CALL_ID', OPEN_CALL_ID],
      ['LEGACY_OPEN_CALL_ID', LEGACY_OPEN_CALL_ID],
    ];
    openCallIds.forEach(([idName, callId]) => {
      it(`matches responses with ${idName} to requests with arbitrary IDs`, async () => {
        const promisedResponse = unaryStub.call(newRequest(6));
        enqueueResponse(
          1,
          unaryStub.method,
          Status.ABORTED,
          OPEN_CALL_ID,
          newResponse('is unrequested'),
        );

        processEnqueuedPackets();
        const [status, response] = await promisedResponse;

        expect(sentPayload(Request).getMagicNumber()).toEqual(6);
        expect(status).toEqual(Status.ABORTED);
        expect(response).toEqual(newResponse('is unrequested'));
      });
    });

    it('blocking call', async () => {
      for (let i = 0; i < 3; i++) {
        enqueueResponse(
          1,
          unaryStub.method,
          Status.ABORTED,
          unaryStub.rpcs.nextCallId,
          newResponse('0_o'),
        );
        const [status, response] = await unaryStub.call(newRequest(6));

        expect(sentPayload(Request).getMagicNumber()).toEqual(6);
        expect(status).toEqual(Status.ABORTED);
        expect(response).toEqual(newResponse('0_o'));
      }
    });

    it('nonblocking call', () => {
      for (let i = 0; i < 3; i++) {
        const response = newResponse('hello world');
        enqueueResponse(
          1,
          unaryStub.method,
          Status.ABORTED,
          unaryStub.rpcs.nextCallId,
          response,
        );

        const onNext = jest.fn();
        const onCompleted = jest.fn();
        const onError = jest.fn();
        const call = unaryStub.invoke(
          newRequest(5),
          onNext,
          onCompleted,
          onError,
        );

        expect(sentPayload(Request).getMagicNumber()).toEqual(5);
        expect(onNext).toHaveBeenCalledWith(response);
        expect(onError).not.toHaveBeenCalled();
        expect(onCompleted).toHaveBeenCalledWith(Status.ABORTED);
      }
    });

    it('open', () => {
      outputException = Error('Error should be ignored');

      for (let i = 0; i < 3; i++) {
        const response = newResponse('hello world');
        enqueueResponse(
          1,
          unaryStub.method,
          Status.ABORTED,
          unaryStub.rpcs.nextCallId,
          response,
        );

        const onNext = jest.fn();
        const onCompleted = jest.fn();
        const onError = jest.fn();
        unaryStub.open(newRequest(5), onNext, onCompleted, onError);
        expect(requests).toHaveLength(0);

        processEnqueuedPackets();

        expect(onNext).toHaveBeenCalledWith(response);
        expect(onError).not.toHaveBeenCalled();
        expect(onCompleted).toHaveBeenCalledWith(Status.ABORTED);
      }
    });

    it('nonblocking concurrent call', () => {
      // Start several calls to the same method
      const callsAndCallbacks = [];
      for (let i = 0; i < 3; i++) {
        const onNext = jest.fn();
        const onCompleted = jest.fn();
        const onError = jest.fn();

        const call = unaryStub.invoke(
          newRequest(5),
          onNext,
          onCompleted,
          onError,
        );
        callsAndCallbacks.push([call, onNext, onCompleted, onError]);

        expect(sentPayload(Request).getMagicNumber()).toEqual(5);
      }
      // Respond only to the last call
      const [lastCall, lastCallback] = callsAndCallbacks.pop();
      const lastResponse = newResponse('last payload');

      enqueueResponse(
        1,
        unaryStub.method,
        Status.OK,
        lastCall.callId,
        lastResponse,
      );
      processEnqueuedPackets();

      expect(lastCallback).toHaveBeenCalledWith(lastResponse);
      for (const i in callsAndCallbacks) {
        const [_call, onNext, onCompleted, onError] = callsAndCallbacks[i];
        expect(onNext).toBeCalledTimes(0);
        expect(onCompleted).toBeCalledTimes(0);
        expect(onError).toBeCalledTimes(0);
      }
    });

    it('blocking server error', async () => {
      for (let i = 0; i < 3; i++) {
        enqueueError(
          1,
          unaryStub.method,
          Status.NOT_FOUND,
          Status.OK,
          unaryStub.rpcs.nextCallId,
        );

        try {
          await unaryStub.call(newRequest());
          fail('call expected to fail');
        } catch (e: any) {
          expect(e.status).toBe(Status.NOT_FOUND);
        }
      }
    });

    it('nonblocking call cancel', () => {
      for (let i = 0; i < 3; i++) {
        const onNext = jest.fn();
        const call = unaryStub.invoke(newRequest(), onNext);

        expect(requests.length).toBeGreaterThan(0);
        requests = [];

        expect(call.cancel()).toBe(true);
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_ERROR);
        expect(lastRequest().getStatus()).toEqual(Status.CANCELLED);

        expect(call.cancel()).toBe(false);
        expect(onNext).not.toHaveBeenCalled();
      }
    });

    it('blocking call with timeout', async () => {
      try {
        await unaryStub.call(newRequest(), 10);
        fail('Promise should not be resolve');
      } catch (err: any) {
        expect(err.timeoutMs).toEqual(10);
      }
    });

    it('nonblocking exception in callback', () => {
      const errorCallback = () => {
        throw Error('Something went wrong!');
      };

      enqueueResponse(
        1,
        unaryStub.method,
        Status.OK,
        unaryStub.rpcs.nextCallId,
      );
      const call = unaryStub.invoke(newRequest(), errorCallback);
      expect(call.callbackException!.name).toEqual('Error');
      expect(call.callbackException!.message).toEqual('Something went wrong!');
    });
  });

  describe('ServerStreaming', () => {
    let serverStreaming: ServerStreamingMethodStub;

    beforeEach(async () => {
      serverStreaming = client
        .channel()
        ?.methodStub(
          'pw.rpc.test1.TheTestService.SomeServerStreaming',
        ) as ServerStreamingMethodStub;
    });

    it('non-blocking call', () => {
      const response1 = newResponse('!!!');
      const response2 = newResponse('?');

      for (let i = 0; i < 3; i++) {
        enqueueServerStream(
          1,
          serverStreaming.method,
          response1,
          serverStreaming.rpcs.nextCallId,
        );
        enqueueServerStream(
          1,
          serverStreaming.method,
          response2,
          serverStreaming.rpcs.nextCallId,
        );
        enqueueResponse(
          1,
          serverStreaming.method,
          Status.ABORTED,
          serverStreaming.rpcs.nextCallId,
        );

        const onNext = jest.fn();
        const onCompleted = jest.fn();
        const onError = jest.fn();
        serverStreaming.invoke(newRequest(4), onNext, onCompleted, onError);

        expect(onNext).toHaveBeenCalledWith(response1);
        expect(onNext).toHaveBeenCalledWith(response2);
        expect(onError).not.toHaveBeenCalled();
        expect(onCompleted).toHaveBeenCalledWith(Status.ABORTED);

        expect(
          sentPayload(serverStreaming.method.requestType).getMagicNumber(),
        ).toEqual(4);
      }
    });

    it('open', () => {
      outputException = Error('Error should be ignored');
      const response1 = newResponse('!!!');
      const response2 = newResponse('?');

      for (let i = 0; i < 3; i++) {
        enqueueServerStream(
          1,
          serverStreaming.method,
          response1,
          serverStreaming.rpcs.nextCallId,
        );
        enqueueServerStream(
          1,
          serverStreaming.method,
          response2,
          serverStreaming.rpcs.nextCallId,
        );
        enqueueResponse(
          1,
          serverStreaming.method,
          Status.ABORTED,
          serverStreaming.rpcs.nextCallId,
        );

        const onNext = jest.fn();
        const onCompleted = jest.fn();
        const onError = jest.fn();
        const call = serverStreaming.open(
          newRequest(3),
          onNext,
          onCompleted,
          onError,
        );

        expect(requests).toHaveLength(0);
        processEnqueuedPackets();

        expect(onNext).toHaveBeenCalledWith(response1);
        expect(onNext).toHaveBeenCalledWith(response2);
        expect(onError).not.toHaveBeenCalled();
        expect(onCompleted).toHaveBeenCalledWith(Status.ABORTED);
      }
    });

    it('blocking timeout', async () => {
      try {
        await serverStreaming.call(newRequest(), 10);
        fail('Promise should not be resolve');
      } catch (err: any) {
        expect(err.timeoutMs).toEqual(10);
      }
    });

    it('non-blocking cancel', () => {
      const testResponse = newResponse('!!!');
      enqueueServerStream(
        1,
        serverStreaming.method,
        testResponse,
        serverStreaming.rpcs.nextCallId,
      );

      const onNext = jest.fn();
      const onCompleted = jest.fn();
      const onError = jest.fn();
      let call = serverStreaming.invoke(newRequest(3), onNext);
      expect(onNext).toHaveBeenNthCalledWith(1, testResponse);

      // onNext.calls.reset();

      call.cancel();
      expect(lastRequest().getType()).toEqual(PacketType.CLIENT_ERROR);
      expect(lastRequest().getStatus()).toEqual(Status.CANCELLED);

      // Ensure the RPC can be called after being cancelled.
      enqueueServerStream(
        1,
        serverStreaming.method,
        testResponse,
        serverStreaming.rpcs.nextCallId,
      );
      enqueueResponse(
        1,
        serverStreaming.method,
        Status.OK,
        serverStreaming.rpcs.nextCallId,
      );
      call = serverStreaming.invoke(newRequest(), onNext, onCompleted, onError);
      expect(onNext).toHaveBeenNthCalledWith(2, testResponse);
      expect(onError).not.toHaveBeenCalled();
      expect(onCompleted).toHaveBeenCalledWith(Status.OK);
    });
  });

  describe('ClientStreaming', () => {
    let clientStreaming: ClientStreamingMethodStub;

    beforeEach(async () => {
      clientStreaming = client
        .channel()
        ?.methodStub(
          'pw.rpc.test1.TheTestService.SomeClientStreaming',
        ) as ClientStreamingMethodStub;
    });

    it('non-blocking call', () => {
      const testResponse = newResponse('-.-');

      for (let i = 0; i < 3; i++) {
        const onNext = jest.fn();
        const stream = clientStreaming.invoke(onNext);
        expect(stream.completed).toBe(false);

        stream.send(newRequest(31));
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_STREAM);
        expect(sentPayload(Request).getMagicNumber()).toEqual(31);
        expect(stream.completed).toBe(false);

        // Enqueue the server response to be sent after the next message.
        enqueueResponse(
          1,
          clientStreaming.method,
          Status.OK,
          stream.callId,
          testResponse,
        );

        stream.send(newRequest(32));
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_STREAM);
        expect(sentPayload(Request).getMagicNumber()).toEqual(32);

        expect(onNext).toHaveBeenCalledWith(testResponse);
        expect(stream.completed).toBe(true);
        expect(stream.status).toEqual(Status.OK);
        expect(stream.error).toBeUndefined();
      }
    });

    it('open', () => {
      outputException = Error('Error should be ignored');
      const response = newResponse('!!!');

      for (let i = 0; i < 3; i++) {
        enqueueResponse(
          1,
          clientStreaming.method,
          Status.OK,
          clientStreaming.rpcs.nextCallId,
          response,
        );

        const onNext = jest.fn();
        const onCompleted = jest.fn();
        const onError = jest.fn();
        const call = clientStreaming.open(onNext, onCompleted, onError);
        expect(requests).toHaveLength(0);

        processEnqueuedPackets();

        expect(onNext).toHaveBeenCalledWith(response);
        expect(onError).not.toHaveBeenCalled();
        expect(onCompleted).toHaveBeenCalledWith(Status.OK);
      }
    });

    it('blocking timeout', async () => {
      try {
        await clientStreaming.call([newRequest()], 10);
        fail('Promise should not be resolve');
      } catch (err: any) {
        expect(err.timeoutMs).toEqual(10);
      }
    });

    it('non-blocking call ended by client', () => {
      const testResponse = newResponse('0.o');
      for (let i = 0; i < 3; i++) {
        const onNext = jest.fn();
        const stream = clientStreaming.invoke(onNext);
        expect(stream.completed).toBe(false);

        stream.send(newRequest(31));
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_STREAM);
        expect(sentPayload(Request).getMagicNumber()).toEqual(31);
        expect(stream.completed).toBe(false);

        // Enqueue the server response to be sent after the next message.
        enqueueResponse(
          1,
          clientStreaming.method,
          Status.OK,
          stream.callId,
          testResponse,
        );

        stream.finishAndWait();
        expect(lastRequest().getType()).toEqual(
          PacketType.CLIENT_REQUEST_COMPLETION,
        );

        expect(onNext).toHaveBeenCalledWith(testResponse);
        expect(stream.completed).toBe(true);
        expect(stream.status).toEqual(Status.OK);
        expect(stream.error).toBeUndefined();
      }
    });

    it('non-blocking call cancelled', () => {
      for (let i = 0; i < 3; i++) {
        const stream = clientStreaming.invoke();
        stream.send(newRequest());

        expect(stream.cancel()).toBe(true);
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_ERROR);
        expect(lastRequest().getStatus()).toEqual(Status.CANCELLED);
        expect(stream.cancel()).toBe(false);
        expect(stream.completed).toBe(true);
        expect(stream.error).toEqual(Status.CANCELLED);
      }
    });

    it('non-blocking call server error', async () => {
      for (let i = 0; i < 3; i++) {
        const stream = clientStreaming.invoke();
        enqueueError(
          1,
          clientStreaming.method,
          Status.INVALID_ARGUMENT,
          Status.OK,
          stream.callId,
        );

        stream.send(newRequest());

        await stream
          .finishAndWait()
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
        // Error will be sent in response to the CLIENT_REQUEST_COMPLETION packet.
        enqueueError(
          1,
          clientStreaming.method,
          Status.INVALID_ARGUMENT,
          Status.OK,
          stream.callId,
        );

        await stream
          .finishAndWait()
          .then(() => {
            fail('Promise should not be resolved');
          })
          .catch((reason) => {
            expect(reason.status).toEqual(Status.INVALID_ARGUMENT);
          });
      }
    });

    it('non-blocking call send after cancelled', () => {
      expect.assertions(2);
      const stream = clientStreaming.invoke();
      expect(stream.cancel()).toBe(true);

      try {
        stream.send(newRequest());
      } catch (e) {
        console.log(e);
        expect(e.status).toEqual(Status.CANCELLED);
      }
      // expect(() => stream.send(newRequest())).toThrowError(
      //   error => error.status === Status.CANCELLED
      // );
    });

    it('non-blocking finish after completed', async () => {
      const enqueuedResponse = newResponse('?!');
      enqueueResponse(
        1,
        clientStreaming.method,
        Status.UNAVAILABLE,
        clientStreaming.rpcs.nextCallId,
        enqueuedResponse,
      );

      const stream = clientStreaming.invoke();
      const result = await stream.finishAndWait();
      expect(result[1]).toEqual([enqueuedResponse]);

      expect(await stream.finishAndWait()).toEqual(result);
      expect(await stream.finishAndWait()).toEqual(result);
    });

    it('non-blocking finish after error', async () => {
      enqueueError(
        1,
        clientStreaming.method,
        Status.UNAVAILABLE,
        Status.OK,
        clientStreaming.rpcs.nextCallId,
      );
      const stream = clientStreaming.invoke();

      for (let i = 0; i < 3; i++) {
        await stream
          .finishAndWait()
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
  });

  describe('BidirectionalStreaming', () => {
    let bidiStreaming: BidirectionalStreamingMethodStub;

    beforeEach(async () => {
      bidiStreaming = client
        .channel()
        ?.methodStub(
          'pw.rpc.test1.TheTestService.SomeBidiStreaming',
        ) as BidirectionalStreamingMethodStub;
    });

    it('blocking call', async () => {
      const testRequests = [newRequest(123), newRequest(456)];

      sendResponsesAfterPackets = 3;
      enqueueResponse(
        1,
        bidiStreaming.method,
        Status.NOT_FOUND,
        bidiStreaming.rpcs.nextCallId,
      );

      const results = await bidiStreaming.call(testRequests);
      expect(results[0]).toEqual(Status.NOT_FOUND);
      expect(results[1]).toEqual([]);
    });

    it('blocking server error', async () => {
      const testRequests = [newRequest(123)];
      enqueueError(
        1,
        bidiStreaming.method,
        Status.NOT_FOUND,
        Status.OK,
        bidiStreaming.rpcs.nextCallId,
      );

      await bidiStreaming
        .call(testRequests)
        .then(() => {
          fail('Promise should not be resolved');
        })
        .catch((reason) => {
          expect(reason.status).toEqual(Status.NOT_FOUND);
        });
    });

    it('non-blocking call', () => {
      const rep1 = newResponse('!!!');
      const rep2 = newResponse('?');

      for (let i = 0; i < 3; i++) {
        const testResponses: Array<Message> = [];
        const stream = bidiStreaming.invoke((response) => {
          testResponses.push(response);
        });
        expect(stream.completed).toBe(false);

        stream.send(newRequest(55));
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_STREAM);
        expect(sentPayload(Request).getMagicNumber()).toEqual(55);
        expect(stream.completed).toBe(false);
        expect(testResponses).toEqual([]);

        enqueueServerStream(1, bidiStreaming.method, rep1, stream.callId);
        enqueueServerStream(1, bidiStreaming.method, rep2, stream.callId);

        stream.send(newRequest(66));
        expect(lastRequest().getType()).toEqual(PacketType.CLIENT_STREAM);
        expect(sentPayload(Request).getMagicNumber()).toEqual(66);
        expect(stream.completed).toBe(false);
        expect(testResponses).toEqual([rep1, rep2]);

        enqueueResponse(1, bidiStreaming.method, Status.OK, stream.callId);

        stream.send(newRequest(77));
        expect(stream.completed).toBe(true);
        expect(testResponses).toEqual([rep1, rep2]);
        expect(stream.status).toEqual(Status.OK);
        expect(stream.error).toBeUndefined();
      }
    });

    it('open', () => {
      outputException = Error('Error should be ignored');
      const response1 = newResponse('!!!');
      const response2 = newResponse('?');

      for (let i = 0; i < 3; i++) {
        enqueueServerStream(
          1,
          bidiStreaming.method,
          response1,
          bidiStreaming.rpcs.nextCallId,
        );
        enqueueServerStream(
          1,
          bidiStreaming.method,
          response2,
          bidiStreaming.rpcs.nextCallId,
        );
        enqueueResponse(
          1,
          bidiStreaming.method,
          Status.OK,
          bidiStreaming.rpcs.nextCallId,
        );

        const onNext = jest.fn();
        const onCompleted = jest.fn();
        const onError = jest.fn();
        const call = bidiStreaming.open(onNext, onCompleted, onError);
        expect(requests).toHaveLength(0);

        processEnqueuedPackets();

        expect(onNext).toHaveBeenCalledWith(response1);
        expect(onNext).toHaveBeenCalledWith(response2);
        expect(onError).not.toHaveBeenCalled();
        expect(onCompleted).toHaveBeenCalledWith(Status.OK);
      }
    });

    it('blocking timeout', async () => {
      try {
        await bidiStreaming.call([newRequest()], 10);
        fail('Promise should not be resolve');
      } catch (err: any) {
        expect(err.timeoutMs).toEqual(10);
      }
    });

    it('non-blocking server error', async () => {
      const response = newResponse('!!!');

      for (let i = 0; i < 3; i++) {
        const testResponses: Array<Message> = [];
        const stream = bidiStreaming.invoke((response) => {
          testResponses.push(response);
        });
        expect(stream.completed).toBe(false);

        enqueueServerStream(1, bidiStreaming.method, response, stream.callId);

        stream.send(newRequest(55));
        expect(stream.completed).toBe(false);
        expect(testResponses).toEqual([response]);

        enqueueError(
          1,
          bidiStreaming.method,
          Status.OUT_OF_RANGE,
          Status.OK,
          stream.callId,
        );

        stream.send(newRequest(999));
        expect(stream.completed).toBe(true);
        expect(testResponses).toEqual([response]);
        expect(stream.status).toBeUndefined();
        expect(stream.error).toEqual(Status.OUT_OF_RANGE);

        await stream
          .finishAndWait()
          .then(() => {
            fail('Promise should not be resolved');
          })
          .catch((reason) => {
            expect(reason.status).toEqual(Status.OUT_OF_RANGE);
          });
      }
    });
    it('non-blocking server error after stream end', async () => {
      for (let i = 0; i < 3; i++) {
        const stream = bidiStreaming.invoke();

        // Error is sent in response to CLIENT_REQUEST_COMPLETION packet.
        enqueueError(
          1,
          bidiStreaming.method,
          Status.INVALID_ARGUMENT,
          Status.OK,
          stream.callId,
        );

        await stream
          .finishAndWait()
          .then(() => {
            fail('Promise should not be resolved');
          })
          .catch((reason) => {
            expect(reason.status).toEqual(Status.INVALID_ARGUMENT);
          });
      }
    });

    it('non-blocking send after cancelled', async () => {
      const stream = bidiStreaming.invoke();
      expect(stream.cancel()).toBe(true);

      try {
        stream.send(newRequest());
        fail('send should have failed');
      } catch (e: any) {
        expect(e.status).toBe(Status.CANCELLED);
      }
    });

    it('non-blocking finish after completed', async () => {
      const response = newResponse('!?');
      enqueueServerStream(
        1,
        bidiStreaming.method,
        response,
        bidiStreaming.rpcs.nextCallId,
      );
      enqueueResponse(
        1,
        bidiStreaming.method,
        Status.UNAVAILABLE,
        bidiStreaming.rpcs.nextCallId,
      );

      const stream = bidiStreaming.invoke();
      const result = await stream.finishAndWait();
      expect(result[1]).toEqual([response]);

      expect(await stream.finishAndWait()).toEqual(result);
      expect(await stream.finishAndWait()).toEqual(result);
    });

    it('non-blocking finish after error', async () => {
      const response = newResponse('!?');
      enqueueServerStream(
        1,
        bidiStreaming.method,
        response,
        bidiStreaming.rpcs.nextCallId,
      );
      enqueueError(
        1,
        bidiStreaming.method,
        Status.UNAVAILABLE,
        Status.OK,
        bidiStreaming.rpcs.nextCallId,
      );

      const stream = bidiStreaming.invoke();

      for (let i = 0; i < 3; i++) {
        await stream
          .finishAndWait()
          .then(() => {
            fail('Promise should not be resolved');
          })
          .catch((reason) => {
            expect(reason.status).toEqual(Status.UNAVAILABLE);
            expect(stream.error).toEqual(Status.UNAVAILABLE);
          });
      }
    });
  });
});
