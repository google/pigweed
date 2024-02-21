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

import {
  Field,
  LogEntry,
  LogSourceEvent,
  SourceData,
} from './shared/interfaces';

export abstract class LogSource {
  private eventListeners: {
    eventType: string;
    listener: (event: LogSourceEvent) => void;
  }[];

  protected sourceId: string;

  protected sourceName: string;

  constructor(sourceName: string) {
    this.eventListeners = [];
    this.sourceId = crypto.randomUUID();
    this.sourceName = sourceName;
  }

  abstract start(): void;

  abstract stop(): void;

  addEventListener(
    eventType: string,
    listener: (event: LogSourceEvent) => void,
  ): void {
    this.eventListeners.push({ eventType, listener });
  }

  removeEventListener(
    eventType: string,
    listener: (event: LogSourceEvent) => void,
  ): void {
    this.eventListeners = this.eventListeners.filter(
      (eventListener) =>
        eventListener.eventType !== eventType ||
        eventListener.listener !== listener,
    );
  }

  emitEvent(event: LogSourceEvent): void {
    this.eventListeners.forEach((eventListener) => {
      if (eventListener.eventType === event.type) {
        eventListener.listener(event);
      }
    });
  }

  publishLogEntry(logEntry: LogEntry): void {
    // Validate the log entry
    const validationResult = this.validateLogEntry(logEntry);
    if (validationResult !== null) {
      console.error('Validation error:', validationResult);
      return;
    }

    const sourceData: SourceData = { id: this.sourceId, name: this.sourceName };
    logEntry.sourceData = sourceData;

    // Add the name of the log source as a field in the log entry
    const logSourceField: Field = { key: 'log_source', value: this.sourceName };
    logEntry.fields.splice(1, 0, logSourceField);

    this.emitEvent({ type: 'log-entry', data: logEntry });
  }

  validateLogEntry(logEntry: LogEntry): string | null {
    try {
      if (!logEntry.timestamp) {
        return 'Log entry has no valid timestamp';
      }
      if (!Array.isArray(logEntry.fields)) {
        return 'Log entry fields must be an array';
      }
      if (logEntry.fields.length === 0) {
        return 'Log entry fields must not be empty';
      }

      for (const field of logEntry.fields) {
        if (!field.key || typeof field.key !== 'string') {
          return 'Invalid field key';
        }
        if (
          field.value === undefined ||
          (typeof field.value !== 'string' &&
            typeof field.value !== 'boolean' &&
            typeof field.value !== 'number' &&
            typeof field.value !== 'object')
        ) {
          return 'Invalid field value';
        }
      }

      if (
        logEntry.severity !== undefined &&
        typeof logEntry.severity !== 'string'
      ) {
        return 'Invalid severity value';
      }

      return null;
    } catch (error) {
      if (error instanceof Error) {
        console.error('Validation error:', error.message);
      }
      return 'An unexpected error occurred during validation';
    }
  }
}
