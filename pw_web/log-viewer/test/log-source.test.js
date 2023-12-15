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
import { LogSource } from '../src/log-source';

describe('log-source', () => {
  let logSource;
  const logEntry = {
    severity: 'INFO',
    timestamp: new Date(Date.now()),
    fields: [{ key: 'message', value: 'Log message' }],
  };

  beforeEach(() => {
    logSource = new LogSource();
  });

  afterEach(() => {
    logSource = null;
  });

  it('emits events to registered listeners', () => {
    const eventType = 'log-entry';
    let receivedData = null;

    const listener = (data) => {
      receivedData = data;
    };

    logSource.addEventListener(eventType, listener);
    logSource.emitEvent(eventType, logEntry);

    expect(receivedData).to.equal(logEntry);
  });

  it("logs aren't dropped at high read frequencies", async () => {
    const numLogs = 10;
    const logEntries = [];
    const eventType = 'log-entry';
    const listener = () => {
      // Simulate a slow listener
      return new Promise((resolve) => {
        setTimeout(() => {
          resolve();
        }, 100);
      });
    };

    logSource.addEventListener(eventType, listener);

    const emittedLogs = [];

    for (let i = 0; i < numLogs; i++) {
      logEntries.push(logEntry);

      await logSource.emitEvent(eventType, logEntry);
      emittedLogs.push(logEntry);
    }

    await new Promise((resolve) => setTimeout(resolve, 200));

    expect(emittedLogs).to.deep.equal(logEntries);
  });

  it('throws an error for incorrect log entry structure', async () => {
    const incorrectLogEntry = {
      fields: [{ key: 'message', value: 'Log entry without timestamp' }],
    };

    try {
      await logSource.emitEvent('log-entry', incorrectLogEntry);
    } catch (error) {
      expect(error.message).to.equal('Invalid log entry structure');
    }
  });
});
