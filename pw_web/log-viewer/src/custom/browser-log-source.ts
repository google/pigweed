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

import { LogSource } from '../log-source';
import { Severity } from '../shared/interfaces';

export class BrowserLogSource extends LogSource {
  private originalMethods = {
    log: console.log,
    info: console.info,
    warn: console.warn,
    error: console.error,
    debug: console.debug,
  };

  constructor(sourceName: string = 'Browser Console') {
    super(sourceName);
  }

  private formatMessage(args: IArguments | string[] | object[]): string {
    if (args.length > 0) {
      let msg = String(args[0]);
      const subs = Array.prototype.slice.call(args, 1);
      let subIndex = 0;
      msg = msg.replace(/%s|%d|%i|%f/g, (match) => {
        if (subIndex < subs.length) {
          const replacement = subs[subIndex++];
          switch (match) {
            case '%s':
              // Check if replacement is an object and stringify it
              return typeof replacement === 'object'
                ? JSON.stringify(replacement)
                : String(replacement);
            case '%d':
            case '%i':
              return parseInt(replacement, 10);
            case '%f':
              return parseFloat(replacement);
            default:
              return replacement;
          }
        }
        return match;
      });
      // Handle remaining arguments that were not replaced in the string
      if (subs.length > subIndex) {
        const remaining = subs
          .slice(subIndex)
          .map((arg) =>
            typeof arg === 'object'
              ? JSON.stringify(arg, null, 0)
              : String(arg),
          )
          .join(' ');
        msg += ` ${remaining}`;
      }
      return msg;
    }
    return '';
  }

  private publishFormattedLogEntry(
    severity: Severity,
    originalArgs: IArguments | string[] | object[],
  ): void {
    const formattedMessage = this.formatMessage(originalArgs);

    this.publishLogEntry({
      severity: severity,
      timestamp: new Date(),
      fields: [
        { key: 'timestamp', value: new Date().toISOString() },
        { key: 'severity', value: severity },
        { key: 'message', value: formattedMessage },
      ],
    });
  }

  start(): void {
    console.log = (...args: string[] | object[]) => {
      this.publishFormattedLogEntry(Severity.INFO, args);
      this.originalMethods.log(...args);
    };

    console.info = (...args: string[] | object[]) => {
      this.publishFormattedLogEntry(Severity.INFO, args);
      this.originalMethods.info(...args);
    };

    console.warn = (...args: string[] | object[]) => {
      this.publishFormattedLogEntry(Severity.WARNING, args);
      this.originalMethods.warn(...args);
    };

    console.error = (...args: string[] | object[]) => {
      this.publishFormattedLogEntry(Severity.ERROR, args);
      this.originalMethods.error(...args);
    };

    console.debug = (...args: string[] | object[]) => {
      this.publishFormattedLogEntry(Severity.DEBUG, args);
      this.originalMethods.debug(...args);
    };
  }

  stop(): void {
    console.log = this.originalMethods.log;
    console.info = this.originalMethods.info;
    console.warn = this.originalMethods.warn;
    console.error = this.originalMethods.error;
    console.debug = this.originalMethods.debug;
  }
}
