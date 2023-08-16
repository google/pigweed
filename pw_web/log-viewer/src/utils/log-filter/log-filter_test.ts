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

import { LogFilter } from './log-filter';
import { Severity, LogEntry } from '../../shared/interfaces';
import { describe, expect, test } from '@jest/globals';
import testData from './test-data';

describe('LogFilter', () => {
  describe('parseSearchQuery()', () => {
    describe('parses search queries correctly', () => {
      testData.forEach(({ query, expected }) => {
        test(`parses "${query}" correctly`, () => {
          const filters = LogFilter.parseSearchQuery(query);
          expect(filters).toEqual(expected);
        });
      });
    });
  });

  describe('createFilterFunction()', () => {
    describe('filters log entries correctly', () => {
      const logEntry1: LogEntry = {
        timestamp: new Date(),
        severity: Severity.INFO,
        fields: [
          { key: 'source', value: 'application' },
          {
            key: 'message',
            value: 'Request processed successfully!',
          },
        ],
      };

      const logEntry2: LogEntry = {
        timestamp: new Date(),
        severity: Severity.WARNING,
        fields: [
          { key: 'source', value: 'database' },
          {
            key: 'message',
            value: 'Database connection lost. Attempting to reconnect.',
          },
        ],
      };

      const logEntry3: LogEntry = {
        timestamp: new Date(),
        severity: Severity.ERROR,
        fields: [
          { key: 'source', value: 'network' },
          {
            key: 'message',
            value:
              'An unexpected error occurred while performing the operation.',
          },
        ],
      };

      const logEntries = [logEntry1, logEntry2, logEntry3];

      test('should filter by simple string search', () => {
        const searchQuery = 'error';
        const filters = LogFilter.parseSearchQuery(searchQuery).map((node) =>
          LogFilter.createFilterFunction(node),
        );
        expect(filters.length).toBe(1);
        expect(logEntries.filter(filters[0])).toEqual([logEntry3]);
      });

      test('should filter by column-specific search', () => {
        const searchQuery = 'source:database';
        const filters = LogFilter.parseSearchQuery(searchQuery).map((node) =>
          LogFilter.createFilterFunction(node),
        );
        expect(filters.length).toBe(1);
        expect(logEntries.filter(filters[0])).toEqual([logEntry2]);
      });

      test('should filter by exact phrase', () => {
        const searchQuery = '"Request processed successfully!"';
        const filters = LogFilter.parseSearchQuery(searchQuery).map((node) =>
          LogFilter.createFilterFunction(node),
        );
        expect(filters.length).toBe(1);
        expect(logEntries.filter(filters[0])).toEqual([logEntry1]);
      });

      test('should filter by column presence', () => {
        const searchQuery = 'source:';
        const filters = LogFilter.parseSearchQuery(searchQuery).map((node) =>
          LogFilter.createFilterFunction(node),
        );
        expect(filters.length).toBe(1);
        expect(logEntries.filter(filters[0])).toEqual([
          logEntry1,
          logEntry2,
          logEntry3,
        ]);
      });

      test('should handle AND expressions', () => {
        const searchQuery = 'source:network message:error';
        const filters = LogFilter.parseSearchQuery(searchQuery).map((node) =>
          LogFilter.createFilterFunction(node),
        );
        expect(filters.length).toBe(1);
        expect(logEntries.filter(filters[0])).toEqual([logEntry3]);
      });

      test('should handle OR expressions', () => {
        const searchQuery = 'source:database | source:network';
        const filters = LogFilter.parseSearchQuery(searchQuery).map((node) =>
          LogFilter.createFilterFunction(node),
        );
        expect(filters.length).toBe(1);
        expect(logEntries.filter(filters[0])).toEqual([logEntry2, logEntry3]);
      });

      test('should handle NOT expressions', () => {
        const searchQuery = '!source:database';
        const filters = LogFilter.parseSearchQuery(searchQuery).map((node) =>
          LogFilter.createFilterFunction(node),
        );
        expect(filters.length).toBe(1);
        expect(logEntries.filter(filters[0])).toEqual([logEntry1, logEntry3]);
      });

      test('should handle a combination of AND and OR expressions', () => {
        const searchQuery = '(source:database | source:network) message:error';
        const filters = LogFilter.parseSearchQuery(searchQuery).map((node) =>
          LogFilter.createFilterFunction(node),
        );
        expect(filters.length).toBe(1);
        expect(logEntries.filter(filters[0])).toEqual([logEntry3]);
      });

      test('should handle a combination of AND, OR, and NOT expressions', () => {
        const searchQuery =
          '(source:application | source:database) !message:request';
        const filters = LogFilter.parseSearchQuery(searchQuery).map((node) =>
          LogFilter.createFilterFunction(node),
        );
        expect(filters.length).toBe(1);
        expect(logEntries.filter(filters[0])).toEqual([logEntry2]);
      });

      test('should handle an empty query', () => {
        const searchQuery = '';
        const filters = LogFilter.parseSearchQuery(searchQuery).map((node) =>
          LogFilter.createFilterFunction(node),
        );
        expect(filters.length).toBe(1);
        expect(logEntries.filter(filters[0])).toEqual([
          logEntry1,
          logEntry2,
          logEntry3,
        ]);
      });
    });
  });
});
