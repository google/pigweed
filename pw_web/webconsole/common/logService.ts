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

import {Device, pw_rpc} from "pigweedjs";
type Client = pw_rpc.Client;

function createDefaultRPCLogService(client: Client) {
  const logService = client.channel()!
    .methodStub('pw.log.Logs.Listen');

  return logService;
}

export async function listenToDefaultLogService(
  device: Device,
  onFrame: (frame: Uint8Array) => void) {
  const client = device.client;
  // @ts-ignore
  const logService: pw_rpc.ServerStreamingMethodStub = (createDefaultRPCLogService(client))!;
  const request = new logService.method.responseType();
  // @ts-ignore
  const call = logService.invoke(request, (msg) => {
    // @ts-ignore
    msg.getEntriesList().forEach(entry => onFrame(entry.getMessage()));
  });

  return () => {
    call.cancel();
  };
}
