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
import { spy, match } from 'sinon';
import { LogSource } from '../src/log-source';
import { BrowserLogSource } from '../src/custom/browser-log-source';
import { Severity } from '../src/shared/interfaces';

describe('log-source', () => {
  let logSourceA, logSourceB;
  const logEntry = {
    severity: 'INFO',
    timestamp: new Date(Date.now()),
    fields: [{ key: 'message', value: 'Log message' }],
  };

  beforeEach(() => {
    logSourceA = new LogSource('Log Source A');
    logSourceB = new LogSource('Log Source B');
  });

  afterEach(() => {
    logSourceA = null;
    logSourceB = null;
  });

  it('emits events to registered listeners', () => {
    const eventType = 'log-entry';
    let receivedData = null;

    const listener = (event) => {
      receivedData = event.data;
    };

    logSourceA.addEventListener(eventType, listener);
    logSourceA.publishLogEntry(logEntry);

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

    logSourceA.addEventListener(eventType, listener);

    const emittedLogs = [];

    for (let i = 0; i < numLogs; i++) {
      logEntries.push(logEntry);

      await logSourceA.publishLogEntry(logEntry);
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
      await logSourceA.publishLogEntry(incorrectLogEntry);
    } catch (error) {
      expect(error.message).to.equal('Invalid log entry structure');
    }
  });
});

describe('browser-log-source', () => {
  let browserLogSource;
  let originalConsoleMethods;

  beforeEach(() => {
    originalConsoleMethods = {
      log: console.log,
      info: console.info,
      warn: console.warn,
      error: console.error,
      debug: console.debug,
    };
    browserLogSource = new BrowserLogSource();
    browserLogSource.start();
    browserLogSource.publishLogEntry = spy();
  });

  afterEach(() => {
    browserLogSource.stop();

    console.log = originalConsoleMethods.log;
    console.info = originalConsoleMethods.info;
    console.warn = originalConsoleMethods.warn;
    console.error = originalConsoleMethods.error;
    console.debug = originalConsoleMethods.debug;
  });

  it('captures and formats console.log messages with substitutions correctly', () => {
    browserLogSource.publishLogEntry.resetHistory();

    console.log("Hello, %s. You've called me %d times.", 'Alice', 5);
    const expectedMessage = "Hello, Alice. You've called me 5 times.";

    expect(browserLogSource.publishLogEntry.calledOnce).to.be.true;

    const callArgs = browserLogSource.publishLogEntry.getCall(0).args[0];
    expect(callArgs.severity).to.equal(Severity.INFO);

    const messageField = callArgs.fields.find(
      (field) => field.key === 'message',
    );
    expect(messageField).to.exist;
    expect(messageField.value).to.equal(expectedMessage);
  });

  ['log', 'info', 'warn', 'error', 'debug'].forEach((method) => {
    it(`captures and formats console.${method} messages`, () => {
      const expectedSeverity = mapMethodToSeverity(method);
      console[method]('Test message (%s)', method);
      expect(browserLogSource.publishLogEntry).to.have.been.calledWithMatch({
        timestamp: match.instanceOf(Date),
        severity: expectedSeverity,
        fields: [
          { key: 'severity', value: expectedSeverity },
          { key: 'message', value: `Test message (${method})` },
        ],
      });
    });
  });

  function mapMethodToSeverity(method) {
    switch (method) {
      case 'log':
      case 'info':
        return Severity.INFO;
      case 'warn':
        return Severity.WARNING;
      case 'error':
        return Severity.ERROR;
      case 'debug':
        return Severity.DEBUG;
      default:
        return Severity.INFO;
    }
  }

  it('captures and formats multiple arguments correctly', () => {
    console.log('This is a test', 42, { type: 'answer' });

    const expectedMessage = 'This is a test 42 {"type":"answer"}';

    expect(browserLogSource.publishLogEntry.calledOnce).to.be.true;
    const callArgs = browserLogSource.publishLogEntry.getCall(0).args[0];
    expect(callArgs.severity).to.equal(Severity.INFO);

    const messageField = callArgs.fields.find(
      (field) => field.key === 'message',
    );
    expect(messageField).to.exist;
    expect(messageField.value).to.equal(expectedMessage);
  });

  it('restores original console methods after stop is called', () => {
    browserLogSource.stop();
    expect(console.log).to.equal(originalConsoleMethods.log);
    expect(console.info).to.equal(originalConsoleMethods.info);
    expect(console.warn).to.equal(originalConsoleMethods.warn);
    expect(console.error).to.equal(originalConsoleMethods.error);
    expect(console.debug).to.equal(originalConsoleMethods.debug);
  });
});
