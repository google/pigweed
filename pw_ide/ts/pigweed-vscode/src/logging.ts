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

import * as vscode from 'vscode';

type LogEntry = {
  level: 'info' | 'warn' | 'error';
  message: string;
  timestamp: Date;
};
export interface Logger {
  info: (msg: string) => void;
  warn: (msg: string) => void;
  error: (msg: string) => void;
}

export const output = vscode.window.createOutputChannel('Pigweed', {
  log: true,
});

const logger = {
  info: (msg: string) => output.info(msg),
  warn: (msg: string) => output.warn(msg),
  error: (msg: string) => output.error(msg),
};

export interface LogDumpPassthrough extends Logger {
  logs: LogEntry[];
}

function loggerWithPassthrough(logger: Logger): LogDumpPassthrough {
  const logs: LogEntry[] = [];
  return {
    info: (msg: string) => {
      logger.info(msg);
      logs.push({ level: 'info', message: msg, timestamp: new Date() });
    },
    warn: (msg: string) => {
      logger.warn(msg);
      logs.push({ level: 'warn', message: msg, timestamp: new Date() });
    },
    error: (msg: string) => {
      logger.error(msg);
      logs.push({ level: 'error', message: msg, timestamp: new Date() });
    },
    logs,
  };
}

export default loggerWithPassthrough(logger);
