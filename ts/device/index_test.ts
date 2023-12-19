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
import { SerialMock } from '../transport/serial_mock';
import { Device } from './';
import { ProtoCollection } from 'pigweedjs/protos/collection';
import { WebSerialTransport } from '../transport/web_serial_transport';
import { Serial } from 'pigweedjs/types/serial';
import { Message } from 'google-protobuf';
import {
  RpcPacket,
  PacketType,
} from 'pigweedjs/protos/pw_rpc/internal/packet_pb';
import {
  Method,
  ServerStreamingMethodStub,
  UnaryMethodStub,
} from 'pigweedjs/pw_rpc';
import { Status } from 'pigweedjs/pw_status';
import { Response } from 'pigweedjs/protos/pw_rpc/ts/test_pb';

import { EchoMessage } from 'pigweedjs/protos/pw_rpc/echo_pb';

describe('WebSerialTransport', () => {
  let device: Device;
  let serialMock: SerialMock;

  function newResponse(payload = '._.'): Message {
    const response = new Response();
    response.setPayload(payload);
    return response;
  }

  function generateResponsePacket(
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
    return packet.serializeBinary();
  }

  function generateStreamingPacket(
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
    return packet.serializeBinary();
  }

  beforeEach(() => {
    serialMock = new SerialMock();
    device = new Device(
      new ProtoCollection(),
      new WebSerialTransport(serialMock as Serial),
    );
  });

  it('has rpcs defined', () => {
    expect(device.rpcs).toBeDefined();
    expect(device.rpcs.pw.rpc.EchoService.Echo).toBeDefined();
  });

  it('has method arguments data', () => {
    expect(device.getMethodArguments('pw.rpc.EchoService.Echo')).toStrictEqual([
      'msg',
    ]);
    expect(device.getMethodArguments('pw.test2.Alpha.Unary')).toStrictEqual([
      'magic_number',
    ]);
  });

  it('unary rpc sends request to serial', async () => {
    const methodStub = device.client
      .channel()!
      .methodStub('pw.rpc.EchoService.Echo')! as UnaryMethodStub;
    const responseMsg = new EchoMessage();
    responseMsg.setMsg('hello');
    await device.connect();
    const nextCallId = methodStub.rpcs.nextCallId;
    setTimeout(() => {
      device.client.processPacket(
        generateResponsePacket(
          1,
          methodStub.method,
          Status.OK,
          nextCallId,
          responseMsg,
        ),
      );
    }, 10);
    const [status, response] =
      await device.rpcs.pw.rpc.EchoService.Echo('hello');
    expect(response.getMsg()).toBe('hello');
    expect(status).toBe(0);
  });

  it('server streaming rpc sends response', async () => {
    await device.connect();
    const response1 = newResponse('!!!');
    const response2 = newResponse('?');
    const serverStreaming = device.client
      .channel()
      ?.methodStub(
        'pw.rpc.test1.TheTestService.SomeServerStreaming',
      ) as ServerStreamingMethodStub;
    const nextCallId = serverStreaming.rpcs.nextCallId;
    const onNext = jest.fn();
    const onCompleted = jest.fn();
    const onError = jest.fn();

    device.rpcs.pw.rpc.test1.TheTestService.SomeServerStreaming(
      4,
      onNext,
      onCompleted,
      onError,
    );
    device.client.processPacket(
      generateStreamingPacket(1, serverStreaming.method, response1, nextCallId),
    );
    device.client.processPacket(
      generateStreamingPacket(1, serverStreaming.method, response2, nextCallId),
    );
    device.client.processPacket(
      generateResponsePacket(
        1,
        serverStreaming.method,
        Status.ABORTED,
        nextCallId,
      ),
    );

    expect(onNext).toBeCalledWith(response1);
    expect(onNext).toBeCalledWith(response2);
    expect(onCompleted).toBeCalledWith(Status.ABORTED);
  });
});
