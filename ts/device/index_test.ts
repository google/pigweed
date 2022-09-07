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
import {SerialMock} from '../transport/serial_mock';
import {Device} from "./"
import {ProtoCollection} from 'pigweedjs/protos/collection';
import {WebSerialTransport} from '../transport/web_serial_transport';
import {Serial} from 'pigweedjs/types/serial';

describe('WebSerialTransport', () => {
  let device: Device;
  let serialMock: SerialMock;
  beforeEach(() => {
    serialMock = new SerialMock();
    device = new Device(new ProtoCollection(), new WebSerialTransport(serialMock as Serial));
  });

  it('has rpcs defined', () => {
    expect(device.rpcs).toBeDefined();
    expect(device.rpcs.pw.rpc.EchoService.Echo).toBeDefined();
  });

  it('has method arguments data', () => {
    expect(device.getMethodArguments("pw.rpc.EchoService.Echo")).toStrictEqual(["msg"]);
    expect(device.getMethodArguments("pw.test2.Alpha.Unary")).toStrictEqual(['magic_number']);
  });

  it('unary rpc sends request to serial', async () => {
    const helloResponse = new Uint8Array([
      126, 165, 3, 42, 7, 10, 5, 104,
      101, 108, 108, 111, 8, 1, 16, 1,
      29, 82, 208, 251, 20, 37, 233, 14,
      71, 139, 109, 127, 108, 165, 126]);

    await device.connect();
    console.log(device.rpcs.pw.rpc.EchoService.Echo);
    serialMock.dataFromDevice(helloResponse);
    const [status, response] = await device.rpcs.pw.rpc.EchoService.Echo("hello");
    expect(response.getMsg()).toBe("hello");
    expect(status).toBe(0);
  });
});
