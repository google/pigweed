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

import { ConditionType } from './log-filter.models';

const testData = [
  {
    query: 'error',
    expected: [
      {
        type: ConditionType.StringSearch,
        searchString: 'error',
      },
    ],
  },
  {
    query: 'source:database',
    expected: [
      {
        type: ConditionType.ColumnSearch,
        column: 'source',
        value: 'database',
      },
    ],
  },
  {
    query: '"Request processed successfully!"',
    expected: [
      {
        type: ConditionType.ExactPhraseSearch,
        exactPhrase: 'Request processed successfully!',
      },
    ],
  },
  {
    query: 'source:',
    expected: [
      {
        type: ConditionType.ColumnSearch,
        column: 'source',
      },
    ],
  },
  {
    query: 'source:network message:error',
    expected: [
      {
        type: ConditionType.AndExpression,
        expressions: [
          {
            type: ConditionType.ColumnSearch,
            column: 'source',
            value: 'network',
          },
          {
            type: ConditionType.ColumnSearch,
            column: 'message',
            value: 'error',
          },
        ],
      },
    ],
  },
  {
    query: 'source:database | source:network',
    expected: [
      {
        type: ConditionType.OrExpression,
        expressions: [
          {
            type: ConditionType.ColumnSearch,
            column: 'source',
            value: 'database',
          },
          {
            type: ConditionType.ColumnSearch,
            column: 'source',
            value: 'network',
          },
        ],
      },
    ],
  },
  {
    query: '!source:database',
    expected: [
      {
        type: ConditionType.NotExpression,
        expression: {
          type: ConditionType.ColumnSearch,
          column: 'source',
          value: 'database',
        },
      },
    ],
  },
  {
    query: 'message:error (source:database | source:network)',
    expected: [
      {
        type: ConditionType.AndExpression,
        expressions: [
          {
            type: ConditionType.ColumnSearch,
            column: 'message',
            value: 'error',
          },
          {
            type: ConditionType.OrExpression,
            expressions: [
              {
                type: ConditionType.ColumnSearch,
                column: 'source',
                value: 'database',
              },
              {
                type: ConditionType.ColumnSearch,
                column: 'source',
                value: 'network',
              },
            ],
          },
        ],
      },
    ],
  },
  {
    query: '(source:database | source:network) message:error',
    expected: [
      {
        type: ConditionType.AndExpression,
        expressions: [
          {
            type: ConditionType.OrExpression,
            expressions: [
              {
                type: ConditionType.ColumnSearch,
                column: 'source',
                value: 'database',
              },
              {
                type: ConditionType.ColumnSearch,
                column: 'source',
                value: 'network',
              },
            ],
          },
          {
            type: ConditionType.ColumnSearch,
            column: 'message',
            value: 'error',
          },
        ],
      },
    ],
  },
  {
    query: '(source:application | source:database) !message:request',
    expected: [
      {
        type: ConditionType.AndExpression,
        expressions: [
          {
            type: ConditionType.OrExpression,
            expressions: [
              {
                type: ConditionType.ColumnSearch,
                column: 'source',
                value: 'application',
              },
              {
                type: ConditionType.ColumnSearch,
                column: 'source',
                value: 'database',
              },
            ],
          },
          {
            type: ConditionType.NotExpression,
            expression: {
              type: ConditionType.ColumnSearch,
              column: 'message',
              value: 'request',
            },
          },
        ],
      },
    ],
  },
  {
    query: '',
    expected: [
      {
        type: ConditionType.StringSearch,
        searchString: '',
      },
    ],
  },
  {
    // Note: AND takes priority over OR in evaluation.
    query: 'source:database message:error | source:network message:error',
    expected: [
      {
        type: ConditionType.OrExpression,
        expressions: [
          {
            type: ConditionType.AndExpression,
            expressions: [
              {
                type: ConditionType.ColumnSearch,
                column: 'source',
                value: 'database',
              },
              {
                type: ConditionType.ColumnSearch,
                column: 'message',
                value: 'error',
              },
            ],
          },
          {
            type: ConditionType.AndExpression,
            expressions: [
              {
                type: ConditionType.ColumnSearch,
                column: 'source',
                value: 'network',
              },
              {
                type: ConditionType.ColumnSearch,
                column: 'message',
                value: 'error',
              },
            ],
          },
        ],
      },
    ],
  },
  {
    query: 'source:database | error',
    expected: [
      {
        type: ConditionType.OrExpression,
        expressions: [
          {
            type: ConditionType.ColumnSearch,
            column: 'source',
            value: 'database',
          },
          {
            type: ConditionType.StringSearch,
            searchString: 'error',
          },
        ],
      },
    ],
  },
  {
    query: 'source:application request',
    expected: [
      {
        type: ConditionType.AndExpression,
        expressions: [
          {
            type: ConditionType.ColumnSearch,
            column: 'source',
            value: 'application',
          },
          {
            type: ConditionType.StringSearch,
            searchString: 'request',
          },
        ],
      },
    ],
  },

  {
    query: 'source: application request',
    expected: [
      {
        type: ConditionType.AndExpression,
        expressions: [
          {
            type: ConditionType.ColumnSearch,
            column: 'source',
          },
          {
            type: ConditionType.StringSearch,
            searchString: 'application',
          },
          {
            type: ConditionType.StringSearch,
            searchString: 'request',
          },
        ],
      },
    ],
  },
  {
    query: 'source:network | (source:database lorem)',
    expected: [
      {
        type: ConditionType.OrExpression,
        expressions: [
          {
            type: ConditionType.ColumnSearch,
            column: 'source',
            value: 'network',
          },
          {
            type: ConditionType.AndExpression,
            expressions: [
              {
                type: ConditionType.ColumnSearch,
                column: 'source',
                value: 'database',
              },
              {
                type: ConditionType.StringSearch,
                searchString: 'lorem',
              },
            ],
          },
        ],
      },
    ],
  },
  {
    query: '"unexpected error" "the operation"',
    expected: [
      {
        type: ConditionType.AndExpression,
        expressions: [
          {
            type: ConditionType.ExactPhraseSearch,
            exactPhrase: 'unexpected error',
          },
          {
            type: ConditionType.ExactPhraseSearch,
            exactPhrase: 'the operation',
          },
        ],
      },
    ],
  },
];

export default testData;
