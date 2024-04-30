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

import { Field, LogEntry } from './shared/interfaces';
import { downloadTextLogs } from './utils/download';

export class LogStore {
  private logs: LogEntry[] = [];
  private columnOrder: string[] = [];

  addLogEntry(logEntry: LogEntry) {
    this.logs.push(logEntry);
  }

  downloadLogs() {
    const logs = this.getLogs();
    const headers = this.getHeaders(logs[0]?.fields);
    const fileName = 'logs';
    downloadTextLogs(logs, headers, fileName);
  }

  getLogs(): LogEntry[] {
    return this.logs;
  }

  setColumnOrder(colOrder: string[]) {
    this.columnOrder = [...new Set(colOrder)];
  }

  /**
   * Helper to order header columns on download
   * @param headerCol Header column from logs
   * @return Ordered header columns
   */
  private getHeaders(headerCol: Field[]): string[] {
    if (!headerCol) {
      return [];
    }

    const order = this.columnOrder;
    if (order.indexOf('severity') != 0) {
      const index = order.indexOf('severity');
      if (index != -1) {
        order.splice(index, 1);
      }
      order.unshift('severity');
    }

    if (order.indexOf('message') != order.length) {
      const index = order.indexOf('message');
      if (index != -1) {
        order.splice(index, 1);
      }
      order.push('message');
    }

    headerCol.forEach((field) => {
      if (!order.includes(field.key)) {
        order.splice(order.length - 1, 0, field.key);
      }
    });

    return order;
  }
}
