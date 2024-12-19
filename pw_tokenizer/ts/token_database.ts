// Copyright 2022 The Pigweed Authors
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

/** Parses CSV Database for easier lookups */

import Papa from 'papaparse';

interface TokenData {
  token: number;
  removalDate: Date | null;
  domain: string;
  template: string;
}

function parseTokenNumber(num: string, lineNumber?: number): number {
  if (!/^[a-fA-F0-9]+$/.test(num)) {
    // Malformed number
    console.error(
      new Error(
        `TokenDatabase number ${num} ` + lineNumber
          ? `at line ${lineNumber} `
          : '' + `is not a valid hex number`,
      ),
    );
  }

  try {
    return parseInt(num, 16);
  } catch {
    console.error(
      new Error(
        `TokenDatabase number ${num} ` + lineNumber
          ? `at line ${lineNumber} `
          : '' + `could not be parsed`,
      ),
    );
  }
}

function parseRemovalDate(
  dateString: string,
  lineNumber?: number,
): Date | null {
  const dateContent = dateString.trim();
  if (dateContent === '') return null;

  try {
    return new Date(dateContent);
  } catch {
    console.error(
      new Error(
        `TokenDatabase removal date ${dateString} ` + lineNumber
          ? `at line ${lineNumber} `
          : '' + `could not be parsed`,
      ),
    );
  }
}

export function parseCsvEntry(
  data: string[],
  lineNumber?: number,
): TokenData | undefined {
  if (data.length < 3) {
    console.error(
      new Error(
        `TokenDatabase entry ${data} ` + lineNumber
          ? `at line ${lineNumber} `
          : '' + `could not be parsed`,
      ),
    );

    return undefined;
  }

  // Column 0: Token
  const token = parseTokenNumber(data.shift(), lineNumber);

  // Column 1: Removal date
  const removalDate = parseRemovalDate(data.shift(), lineNumber);

  // Modern 4-column databases will have the domain in this position.
  const domain = data.length > 1 ? data.shift() : '';

  // Last column: Template strings
  const template = data.shift();

  return {
    token,
    removalDate,
    domain,
    template,
  };
}

export class TokenDatabase {
  private tokens: Map<number, TokenData> = new Map();

  constructor(readonly csv: string) {
    this.parseTokensToTokensMap(csv);
  }

  has(token: number): boolean {
    return this.tokens.has(token);
  }

  get(token: number): string | undefined {
    return this.tokens.get(token)?.template;
  }

  private parseTokensToTokensMap(csv: string) {
    const parsedCsv = Papa.parse(csv, { header: false });

    if (parsedCsv.errors.length > 0) {
      console.error(
        new Error(
          `TokenDatabase could not be parsed: ${parsedCsv.errors.join(', ')}`,
        ),
      );
    }

    const csvData = parsedCsv.data as string[][];

    for (const [lineNumber, line] of csvData.entries()) {
      const entry = parseCsvEntry(line, lineNumber);
      entry && this.tokens.set(entry.token, entry);
    }
  }
}
