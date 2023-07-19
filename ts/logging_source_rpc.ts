// Copyright 2023 The Pigweed Authors
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

import {Detokenizer} from "../pw_tokenizer/ts";
import {LogSource} from "../pw_web/log-viewer/src/log-source";
import {Device} from "./device";
import {LogEntry} from "./logging";

export class PigweedRPCLogSource extends LogSource {
  private detokenizer: Detokenizer | undefined;
  private logs: LogEntry[] = [];
  private call: any;
  constructor(device: Device, tokenDB: string | undefined) {
    super();
    if (tokenDB && tokenDB.length > 0) {
      this.detokenizer = new Detokenizer(tokenDB);
    }
    this.call = device.rpcs.pw.log.Logs.Listen((msg: any) => {
      msg.getEntriesList().forEach((entry: any) => this.processFrame(entry.getMessage()));
    });
  }

  destroy() {
    this.call.cancel();
  }

  private processFrame(frame: Uint8Array) {
    let entry: LogEntry;
    if (this.detokenizer) {
      const detokenized = this.detokenizer.detokenizeUint8Array(frame);
      entry = this.parseLogMsg(detokenized);
    }
    else {
      const decoded = new TextDecoder().decode(frame);
      entry = this.parseLogMsg(decoded);
    }
    this.logs = [...this.logs, entry];
    this.emitEvent("logEntry", entry);
  }

  private parseLogMsg(msg: string): LogEntry {
    const pairs = msg.split("■").slice(1).map(pair => pair.split("♦"));

    // Not a valid message, print as-is.
    const timestamp = new Date();
    if (pairs.length === 0) {
      return {
        fields: [
          {key: "timestamp", value: timestamp.toISOString()},
          {key: "message", value: msg},
        ],
        timestamp: timestamp,
      }
    }

    let map: any = {};
    pairs.forEach(pair => map[pair[0]] = pair[1])
    return {
      fields: [
        {key: "timestamp", value: timestamp},
        {key: "message", value: map.msg},
        {key: "module", value: map.module},
        {key: "file", value: map.file},
      ],
      timestamp: timestamp,
    }
  }
}
