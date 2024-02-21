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

import { LogEntry } from '../../shared/interfaces';
import { FilterCondition, ConditionType } from './log-filter.models';

export class LogFilter {
  /**
   * Generates a structured representation of filter conditions which can be
   * used to filter log entries.
   *
   * @param {string} searchQuery - The search query string provided.
   * @returns {function[]} An array of filter functions, each representing a
   *   set of conditions grouped by logical operators, for filtering log
   *   entries.
   */
  static parseSearchQuery(searchQuery: string): FilterCondition[] {
    const filters: FilterCondition[] = [];
    const orGroups = searchQuery.split(/\s*\|\s*/);

    for (let i = 0; i < orGroups.length; i++) {
      let orGroup = orGroups[i];

      if (orGroup.includes('(') && !orGroup.includes(')')) {
        let j = i + 1;
        while (j < orGroups.length && !orGroups[j].includes(')')) {
          orGroup += ` | ${orGroups[j]}`;
          j++;
        }

        if (j < orGroups.length) {
          orGroup += ` | ${orGroups[j]}`;
          i = j;
        }
      }

      const andConditions = orGroup.match(
        /\([^()]*\)|"[^"]+"|[^\s:]+:[^\s]+|[^\s]+/g,
      );

      const andFilters: FilterCondition[] = [];

      if (andConditions) {
        for (const condition of andConditions) {
          if (condition.startsWith('(') && condition.endsWith(')')) {
            const nestedConditions = condition.slice(1, -1).trim();
            andFilters.push(...this.parseSearchQuery(nestedConditions));
          } else if (condition.startsWith('"') && condition.endsWith('"')) {
            const exactPhrase = condition.slice(1, -1).trim();
            andFilters.push({
              type: ConditionType.ExactPhraseSearch,
              exactPhrase,
            });
          } else if (condition.startsWith('!')) {
            const column = condition.slice(1, condition.indexOf(':'));
            const value = condition.slice(condition.indexOf(':') + 1);
            andFilters.push({
              type: ConditionType.NotExpression,
              expression: {
                type: ConditionType.ColumnSearch,
                column,
                value,
              },
            });
          } else if (condition.endsWith(':')) {
            const column = condition.slice(0, condition.indexOf(':'));
            andFilters.push({
              type: ConditionType.ColumnSearch,
              column,
            });
          } else if (condition.includes(':')) {
            const column = condition.slice(0, condition.indexOf(':'));
            const value = condition.slice(condition.indexOf(':') + 1);
            andFilters.push({
              type: ConditionType.ColumnSearch,
              column,
              value,
            });
          } else {
            andFilters.push({
              type: ConditionType.StringSearch,
              searchString: condition,
            });
          }
        }
      }

      if (andFilters.length > 0) {
        if (andFilters.length === 1) {
          filters.push(andFilters[0]);
        } else {
          filters.push({
            type: ConditionType.AndExpression,
            expressions: andFilters,
          });
        }
      }
    }

    if (filters.length === 0) {
      filters.push({
        type: ConditionType.StringSearch,
        searchString: '',
      });
    }

    if (filters.length > 1) {
      return [
        {
          type: ConditionType.OrExpression,
          expressions: filters,
        },
      ];
    }

    return filters;
  }

  /**
   * Takes a condition node, which represents a specific filter condition, and
   * recursively generates a filter function that can be applied to log
   * entries.
   *
   * @param {FilterCondition} condition - A filter condition to convert to a
   *   function.
   * @returns {function} A function for filtering log entries based on the
   *   input condition and its logical operators.
   */
  static createFilterFunction(
    condition: FilterCondition,
  ): (logEntry: LogEntry) => boolean {
    switch (condition.type) {
      case ConditionType.StringSearch:
        return (logEntry) =>
          this.checkStringInColumns(logEntry, condition.searchString);
      case ConditionType.ExactPhraseSearch:
        return (logEntry) =>
          this.checkExactPhraseInColumns(logEntry, condition.exactPhrase);
      case ConditionType.ColumnSearch:
        return (logEntry) =>
          this.checkColumn(logEntry, condition.column, condition.value);
      case ConditionType.NotExpression: {
        const innerFilter = this.createFilterFunction(condition.expression);
        return (logEntry) => !innerFilter(logEntry);
      }
      case ConditionType.AndExpression: {
        const andFilters = condition.expressions.map((expr) =>
          this.createFilterFunction(expr),
        );
        return (logEntry) => andFilters.every((filter) => filter(logEntry));
      }
      case ConditionType.OrExpression: {
        const orFilters = condition.expressions.map((expr) =>
          this.createFilterFunction(expr),
        );
        return (logEntry) => orFilters.some((filter) => filter(logEntry));
      }
      default:
        // Return a filter that matches all entries
        return () => true;
    }
  }

  /**
   * Checks if the column exists in a log entry and then performs a value
   * search on the column's value.
   *
   * @param {LogEntry} logEntry - The log entry to be searched.
   * @param {string} column - The name of the column (log entry field) to be
   *   checked for filtering.
   * @param {string} value - An optional string that represents the value used
   *   for filtering.
   * @returns {boolean} True if the specified column exists in the log entry,
   *   or if a value is provided, returns true if the value matches a
   *   substring of the column's value (case-insensitive).
   */
  private static checkColumn(
    logEntry: LogEntry,
    column: string,
    value?: string,
  ): boolean {
    const field = logEntry.fields.find((field) => field.key === column);
    if (!field) return false;

    if (value === undefined) {
      return true;
    }

    const searchRegex = new RegExp(value, 'i');
    return searchRegex.test(field.value.toString());
  }

  /**
   * Checks if the provided search string exists in any of the log entry
   * columns (excluding `severity`).
   *
   * @param {LogEntry} logEntry - The log entry to be searched.
   * @param {string} searchString - The search string to be matched against
   *   the log entry fields.
   * @returns {boolean} True if the search string is found in any of the log
   *   entry fields, otherwise false.
   */
  private static checkStringInColumns(
    logEntry: LogEntry,
    searchString: string,
  ): boolean {
    const escapedSearchString = this.escapeRegEx(searchString);
    const columnsToSearch = logEntry.fields.filter(
      (field) => field.key !== 'severity',
    );
    return columnsToSearch.some((field) =>
      new RegExp(escapedSearchString, 'i').test(field.value.toString()),
    );
  }

  /**
   * Checks if the exact phrase exists in any of the log entry columns
   * (excluding `severity`).
   *
   * @param {LogEntry} logEntry - The log entry to be searched.
   * @param {string} exactPhrase - The exact phrase to search for within the
   *   log entry columns.
   * @returns {boolean} True if the exact phrase is found in any column,
   *   otherwise false.
   */
  private static checkExactPhraseInColumns(
    logEntry: LogEntry,
    exactPhrase: string,
  ): boolean {
    const escapedExactPhrase = this.escapeRegEx(exactPhrase);
    const searchRegex = new RegExp(escapedExactPhrase, 'i');
    const columnsToSearch = logEntry.fields.filter(
      (field) => field.key !== 'severity',
    );
    return columnsToSearch.some((field) =>
      searchRegex.test(field.value.toString()),
    );
  }

  private static escapeRegEx(text: string) {
    return text.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
  }
}
