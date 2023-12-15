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

import { expect } from '@open-wc/testing';
import '../src/components/log-viewer';
import { MockLogSource } from '../src/custom/mock-log-source';
import { createLogViewer } from '../src/createLogViewer';

// Initialize the log viewer component with a mock log source
function setUpLogViewer() {
  const mockLogSource = new MockLogSource();
  const destroyLogViewer = createLogViewer(mockLogSource, document.body);
  const logViewer = document.querySelector('log-viewer');
  return { mockLogSource, destroyLogViewer, logViewer };
}

// Handle benign ResizeObserver error caused by custom log viewer initialization
// See: https://developer.mozilla.org/en-US/docs/Web/API/ResizeObserver#observation_errors
function handleResizeObserverError() {
  const e = window.onerror;
  window.onerror = function (err) {
    if (
      err === 'ResizeObserver loop completed with undelivered notifications.'
    ) {
      console.warn(
        'Ignored: ResizeObserver loop completed with undelivered notifications.',
      );
      return false;
    } else {
      return e(...arguments);
    }
  };
}

/**
 * Checks if the table header cells in the rendered log viewer match the given
 * expected column names.
 */
function checkTableHeaderCells(table, expectedColumnNames) {
  const tableHeaderRow = table.querySelector('thead tr');
  const tableHeaderCells = tableHeaderRow.querySelectorAll('th');

  expect(tableHeaderCells).to.have.lengthOf(expectedColumnNames.length);

  for (let i = 0; i < tableHeaderCells.length; i++) {
    const columnName = tableHeaderCells[i].textContent.trim();
    expect(columnName).to.equal(expectedColumnNames[i]);
  }
}

/**
 * Checks if the table body cells in the log viewer match the values of the given log entry objects.
 */
function checkTableBodyCells(table, logEntries) {
  const tableHeaderRow = table.querySelector('thead tr');
  const tableHeaderCells = tableHeaderRow.querySelectorAll('th');
  const tableBody = table.querySelector('tbody');
  const tableRows = tableBody.querySelectorAll('tr');
  const fieldKeys = Array.from(tableHeaderCells).map((cell) =>
    cell.textContent.trim(),
  );

  // Iterate through each row and cell in the table body
  tableRows.forEach((row, rowIndex) => {
    const cells = row.querySelectorAll('td');
    const logEntry = logEntries[rowIndex];

    cells.forEach((cell, cellIndex) => {
      const fieldKey = fieldKeys[cellIndex];
      const cellContent = cell.textContent.trim();

      if (logEntry.fields.some((field) => field.key === fieldKey)) {
        const fieldValue = logEntry.fields.find(
          (field) => field.key === fieldKey,
        ).value;
        expect(cellContent).to.equal(String(fieldValue));
      } else {
        // Cell should be empty for missing fields
        expect(cellContent).to.equal('');
      }
    });
  });
}

async function appendLogsAndWait(logViewer, logEntries) {
  const currentLogs = logViewer.logs || [];
  logViewer.logs = [...currentLogs, ...logEntries];

  await logViewer.updateComplete;
  await new Promise((resolve) => setTimeout(resolve, 100));
}

describe('log-viewer', () => {
  let mockLogSource;
  let destroyLogViewer;
  let logViewer;

  beforeEach(() => {
    window.localStorage.clear();
    ({ mockLogSource, destroyLogViewer, logViewer } = setUpLogViewer());
    handleResizeObserverError();
  });

  afterEach(() => {
    mockLogSource.stop();
    destroyLogViewer();
  });

  it('should generate table columns properly with correctly-structured logs', async () => {
    const logEntry1 = {
      timestamp: new Date(),
      fields: [
        { key: 'source', value: 'application' },
        { key: 'timestamp', value: '2023-11-13T23:05:16.520Z' },
        { key: 'message', value: 'Log entry 1' },
      ],
    };

    const logEntry2 = {
      timestamp: new Date(),
      fields: [
        { key: 'source', value: 'server' },
        { key: 'timestamp', value: '2023-11-13T23:10:00.000Z' },
        { key: 'message', value: 'Log entry 2' },
        { key: 'user', value: 'Alice' },
      ],
    };

    await appendLogsAndWait(logViewer, [logEntry1, logEntry2]);

    const { table } = getLogViewerElements(logViewer);
    const expectedColumnNames = ['source', 'timestamp', 'message', 'user'];

    checkTableHeaderCells(table, expectedColumnNames);
  });

  it('displays the correct number of logs', async () => {
    const numLogs = 5;
    const logEntries = [];

    for (let i = 0; i < numLogs; i++) {
      const logEntry = mockLogSource.readLogEntryFromHost();
      logEntries.push(logEntry);
    }

    await appendLogsAndWait(logViewer, logEntries);

    const { table } = getLogViewerElements(logViewer);
    const tableRows = table.querySelectorAll('tbody tr');

    expect(tableRows.length).to.equal(numLogs);
  });

  it('should display columns properly given varying log entry fields', async () => {
    // Create log entries with differing fields
    const logEntry1 = {
      timestamp: new Date(),
      fields: [
        { key: 'source', value: 'application' },
        { key: 'timestamp', value: '2023-11-13T23:05:16.520Z' },
        { key: 'message', value: 'Log entry 1' },
      ],
    };

    const logEntry2 = {
      timestamp: new Date(),
      fields: [
        { key: 'source', value: 'server' },
        { key: 'timestamp', value: '2023-11-13T23:10:00.000Z' },
        { key: 'message', value: 'Log entry 2' },
        { key: 'user', value: 'Alice' },
      ],
    };

    const logEntry3 = {
      timestamp: new Date(),
      fields: [
        { key: 'source', value: 'database' },
        { key: 'timestamp', value: '2023-11-13T23:15:00.000Z' },
        { key: 'description', value: 'Log entry 3' },
      ],
    };

    await appendLogsAndWait(logViewer, [logEntry1, logEntry2, logEntry3]);

    const { table } = getLogViewerElements(logViewer);
    const expectedColumnNames = [
      'source',
      'timestamp',
      'message',
      'user',
      'description',
    ];

    checkTableHeaderCells(table, expectedColumnNames);

    checkTableBodyCells(table, logViewer.logs);
  });
});

function getLogViewerElements(logViewer) {
  const logView = logViewer.shadowRoot.querySelector('log-view');
  const logList = logView.shadowRoot.querySelector('log-list');
  const table = logList.shadowRoot.querySelector('table');

  return { logView, logList, table };
}
