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

import {pw_hdlc, pw_rpc, WebSerial} from "pigweedjs";
type WebSerialTransport = WebSerial.WebSerialTransport
type Client = pw_rpc.Client;
const encoder = new pw_hdlc.Encoder();
const decoder = new pw_hdlc.Decoder();
const RPC_ADDRESS = 82;

function sendPacket(
  transport: WebSerialTransport,
  packetBytes: Uint8Array
) {
  const hdlcBytes = encoder.uiFrame(RPC_ADDRESS, packetBytes);
  transport.sendChunk(hdlcBytes);
}

async function createDefaultRPCLogClient(transport: WebSerialTransport): Promise<Client> {
  // @ts-ignore
  const ProtoCollection = await import("pigweedjs/protos/collection.umd");
  const protoCollection = new ProtoCollection.ProtoCollection();
  const channels = [
    new pw_rpc.Channel(1, (bytes: Uint8Array) => {
      sendPacket(transport, bytes);
    }),
  ];
  // @ts-ignore
  const client = pw_rpc.Client.fromProtoSet(channels, protoCollection);
  return client;
}

function createDefaultRPCLogService(client: Client) {
  const logService = client.channel()!
    .methodStub('pw.log.Logs.Listen');

  return logService;
}

export async function listenToDefaultLogService(
  transport: WebSerialTransport,
  onFrame: (frame: Uint8Array) => void) {
  const client = await createDefaultRPCLogClient(transport);
  const logService = (createDefaultRPCLogService(client))!;
  const request = new logService.method.responseType();
  // @ts-ignore
  logService.invoke(request, (msg) => {
    // @ts-ignore
    msg.getEntriesList().forEach(entry => onFrame(entry.getMessage()));
  });

  const subscription = transport.chunks.subscribe((item) => {
    const decoded = decoder.process(item);
    let elem = decoded.next();
    while (!elem.done) {
      const frame = elem.value;
      if (frame.address === RPC_ADDRESS) {
        client.processPacket(frame.data);
      }
      elem = decoded.next();
    }
  });

  return () => subscription.unsubscribe();
}
