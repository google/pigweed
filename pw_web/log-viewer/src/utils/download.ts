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

import { LogEntry } from '../shared/interfaces';
import { titleCaseToKebabCase } from './strings';

/**
 * Download .txt file of log files
 * @param logs logs to be downloaded
 * @param headers headers of logs
 * @param viewTitle optional title parameter
 */
export function downloadTextLogs(
  logs: LogEntry[],
  headers: string[],
  viewTitle: string = 'logs',
) {
  const maxWidths = headers.map((header) => header.length);
  const fileName = titleCaseToKebabCase(viewTitle);

  logs.forEach((log) => {
    log.fields.forEach((field) => {
      const columnIndex = headers.indexOf(field.key);
      maxWidths[columnIndex] = Math.max(
        maxWidths[columnIndex],
        field.value ? field.value.toString().length : 0,
      );
    });
  });

  const headerRow = headers
    .map((header, columnIndex) => header.padEnd(maxWidths[columnIndex]))
    .join('\t');
  const separator = '';
  const logRows = logs.map((log) => {
    const values = new Array(headers.length).fill('');
    log.fields.forEach((field) => {
      const columnIndex = headers.indexOf(field.key);
      values[columnIndex] = field.value ? field.value.toString() : '';
    });
    values.forEach((_value, index) => {
      values[index] = values[index].padEnd(maxWidths[index]);
    });
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
