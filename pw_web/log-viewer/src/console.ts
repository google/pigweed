// Copyright 2024 The Pigweed Authors
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

import { WebSocketRPCReplKernel, WebSocketRPCClient } from './repl-kernel';
import { createLogViewer } from './createLogViewer';
import { LogSource } from './log-source';
import { Severity } from './shared/interfaces';
import { Repl } from './components/repl/repl';

const logLevelToSeverity = {
  10: Severity.DEBUG,
  20: Severity.INFO,
  21: Severity.INFO,
  30: Severity.WARNING,
  40: Severity.ERROR,
  50: Severity.CRITICAL,
  70: Severity.CRITICAL,
};

const nonAdditionalDataFields = [
  '_hosttime',
  'levelname',
  'levelno',
  'args',
  'fields',
  'message',
  'time',
];

// Format a date in the standard pw_cli style YYYY-mm-dd HH:MM:SS
function formatDate(dt: Date) {
  function pad2(n: number) {
    return (n < 10 ? '0' : '') + n;
  }

  return (
    dt.getFullYear() +
    pad2(dt.getMonth() + 1) +
    pad2(dt.getDate()) +
    ' ' +
    pad2(dt.getHours()) +
    ':' +
    pad2(dt.getMinutes()) +
    ':' +
    pad2(dt.getSeconds())
  );
}

// New LogSource to consume pw-console log json messages
class PwConsoleLogSource extends LogSource {
  constructor() {
    super('PWConsole');
  }
  // @ts-ignore
  appendLog(data) {
    const fields = [
      // @ts-ignore
      { key: 'severity', value: logLevelToSeverity[data.levelno] },
      { key: 'time', value: data.time },
    ];
    Object.keys(data.fields).forEach((columnName) => {
      if (nonAdditionalDataFields.indexOf(columnName) === -1) {
        fields.push({ key: columnName, value: data.fields[columnName] });
      }
    });
    fields.push({ key: 'message', value: data.message });
    fields.push({ key: 'py_file', value: data.py_file });
    fields.push({ key: 'py_logger', value: data.py_logger });
    this.publishLogEntry({
      // @ts-ignore
      severity: logLevelToSeverity[data.levelno],
      timestamp: new Date(),
      fields: fields,
    });
  }
}

export async function renderPWConsole(
  containerEl: HTMLElement,
  wsUrl: string = '/ws',
) {
  const logSource = new PwConsoleLogSource();
  const unsubscribe = createLogViewer(
    logSource,
    containerEl.querySelector('#logs-container')!,
  );
  const ws = new WebSocket(wsUrl);
  const kernel = new WebSocketRPCClient(ws);
  kernel.openStream('log_source_subscribe', { name: 'Fake Device' }, (data) => {
    if (data.log_line)
      logSource.appendLog({ ...data.log_line, time: formatDate(new Date()) });
  });
  const rpc = new WebSocketRPCReplKernel(kernel);
  const repl = new Repl(rpc);
  document.querySelector('#repl-container')?.appendChild(repl);
  (window as any).rpc = rpc;
  return () => {
    unsubscribe();
  };
}
