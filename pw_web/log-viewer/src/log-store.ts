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

import { LogEntry } from './shared/interfaces';
import { titleCaseToKebabCase } from './utils/strings';

export class LogStore {
  private logs: LogEntry[];

  constructor() {
    this.logs = [];
  }

  addLogEntry(logEntry: LogEntry) {
    this.logs.push(logEntry);
  }

  downloadLogs(event: CustomEvent) {
    const logs = this.getLogs();
    const headers = logs[0]?.fields.map((field) => field.key) || [];
    const maxWidths = headers.map((header) => header.length);
    const viewTitle = event.detail.viewTitle;
    const fileName = viewTitle ? titleCaseToKebabCase(viewTitle) : 'logs';

    logs.forEach((log) => {
      log.fields.forEach((field, columnIndex) => {
        maxWidths[columnIndex] = Math.max(
          maxWidths[columnIndex],
          field.value.toString().length,
        );
      });
    });

    const headerRow = headers
      .map((header, columnIndex) => header.padEnd(maxWidths[columnIndex]))
      .join('\t');
    const separator = '';
    const logRows = logs.map((log) => {
      const values = log.fields.map((field, columnIndex) =>
        field.value.toString().padEnd(maxWidths[columnIndex]),
      );
      return values.join('\t');
    });

    const formattedLogs = [headerRow, separator, ...logRows].join('\n');
    const blob = new Blob([formattedLogs], { type: 'text/plain' });
    const downloadLink = document.createElement('a');
    downloadLink.href = URL.createObjectURL(blob);
    downloadLink.download = `${fileName}.txt`;
    downloadLink.click();

    URL.revokeObjectURL(downloadLink.href);
  }

  getLogs(): LogEntry[] {
    return this.logs;
  }
}
