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

// eslint-disable-next-line no-undef
const { createLogViewer, LogSource, LogEntry, Severity } = PigweedLogging;

let currentTheme = {};
let defaultLogStyleRule = 'color: #ffffff;';
let columnStyleRules = {};
let defaultColumnStyles = [];
let logLevelStyles = {};

const logLevelToString = {
  10: 'DBG',
  20: 'INF',
  21: 'OUT',
  30: 'WRN',
  40: 'ERR',
  50: 'CRT',
  70: 'FTL',
};

const logLevelToSeverity = {
  10: Severity.DEBUG,
  20: Severity.INFO,
  21: Severity.INFO,
  30: Severity.WARNING,
  40: Severity.ERROR,
  50: Severity.CRITICAL,
  70: Severity.CRITICAL,
};

let nonAdditionalDataFields = [
  '_hosttime',
  'levelname',
  'levelno',
  'args',
  'fields',
  'message',
  'time',
];
let additionalHeaders = [];

// New LogSource to consume pw-console log json messages
class PwConsoleLogSource extends LogSource {
  constructor() {
    super();
  }
  append_log(data) {
    var fields = [
      { key: 'severity', value: logLevelToSeverity[data.levelno] },
      { key: 'time', value: data.time },
    ];
    Object.keys(data.fields).forEach((columnName) => {
      if (
        nonAdditionalDataFields.indexOf(columnName) === -1 &&
        additionalHeaders.indexOf(columnName) === -1
      ) {
        fields.push({ key: columnName, value: data.fields[columnName] });
      }
    });
    fields.push({ key: 'message', value: data.message });
    fields.push({ key: 'py_file', value: data.py_file });
    fields.push({ key: 'py_logger', value: data.py_logger });
    this.emitEvent('logEntry', {
      severity: logLevelToSeverity[data.levelno],
      timestamp: new Date(),
      fields: fields,
    });
  }
}

// Setup the pigweedjs log-viewer
const logSource = new PwConsoleLogSource();
const containerEl = document.querySelector('#log-viewer-container');
let unsubscribe = createLogViewer(logSource, containerEl);

// Format a date in the standard pw_cli style YYYY-mm-dd HH:MM:SS
function formatDate(dt) {
  function pad2(n) {
    return (n < 10 ? '0' : '') + n;
  }

  return (
    dt.getFullYear() +
    pad2(dt.getMonth() + 1) +
    pad2(dt.getDate()) +
    ' ' +
    pad2(dt.getHours()) +
    ':' +
    pad2(dt.getMinutes()) +
    ':' +
    pad2(dt.getSeconds())
  );
}

// Return the value for the given # parameter name.
function getUrlHashParameter(param) {
  var params = getUrlHashParameters();
  return params[param];
}

// Capture all # parameters from the current URL.
function getUrlHashParameters() {
  var sPageURL = window.location.hash;
  if (sPageURL) sPageURL = sPageURL.split('#')[1];
  var pairs = sPageURL.split('&');
  var object = {};
  pairs.forEach(function (pair, i) {
    pair = pair.split('=');
    if (pair[0] !== '') object[pair[0]] = pair[1];
  });
  return object;
}

// Update web page CSS styles based on a pw-console color json log message.
function setCurrentTheme(newTheme) {
  currentTheme = newTheme;
  defaultLogStyleRule = parsePromptToolkitStyle(newTheme.default);
  // Set body background color
  // document.querySelector('body').setAttribute('style', defaultLogStyleRule);

  // Apply default font styles to columns
  let styles = [];
  Object.keys(newTheme).forEach((key) => {
    if (key.startsWith('log-table-column-')) {
      styles.push(newTheme[key]);
    }
    if (key.startsWith('log-level-')) {
      logLevelStyles[parseInt(key.replace('log-level-', ''))] =
        parsePromptToolkitStyle(newTheme[key]);
    }
  });
  defaultColumnStyles = styles;
}

// Convert prompt_toolkit color format strings to CSS.
// 'bg:#BG-HEX #FG-HEX STYLE' where STYLE is either 'bold' or 'underline'
function parsePromptToolkitStyle(rule) {
  const ruleList = rule.split(' ');
  let outputStyle = ruleList.map((fragment) => {
    if (fragment.startsWith('bg:')) {
      return `background-color: ${fragment.replace('bg:', '')}`;
    } else if (fragment === 'bold') {
      return `font-weight: bold`;
    } else if (fragment === 'underline') {
      return `text-decoration: underline`;
    } else if (fragment.startsWith('#')) {
      return `color: ${fragment}`;
    }
  });
  return outputStyle.join(';');
}

// Inject styled spans into the log message column values.
function applyStyling(data, applyColors = false) {
  let colIndex = 0;
  Object.keys(data).forEach((key) => {
    if (columnStyleRules[key] && typeof data[key] === 'string') {
      Object.keys(columnStyleRules[key]).forEach((token) => {
        data[key] = data[key].replaceAll(
          token,
          `<span
              style="${defaultLogStyleRule};${
                applyColors
                  ? defaultColumnStyles[colIndex % defaultColumnStyles.length]
                  : ''
              };${parsePromptToolkitStyle(columnStyleRules[key][token])};">
                ${token}
            </span>`,
        );
      });
    } else if (key === 'fields') {
      data[key] = applyStyling(data.fields, true);
    }
    if (applyColors) {
      data[key] = `<span
      style="${parsePromptToolkitStyle(
        defaultColumnStyles[colIndex % defaultColumnStyles.length],
      )}">
        ${data[key]}
      </span>`;
    }
    colIndex++;
  });
  return data;
}

// Connect to the pw-console websocket and start emitting logs.
(function () {
  const container = document.querySelector('.log-container');
  const height = window.innerHeight - 50;
  let follow = true;

  const port = getUrlHashParameter('ws');
  const hostname = location.hostname || '127.0.0.1';
  var ws = new WebSocket(`ws://${hostname}:${port}/`);
  ws.onmessage = function (event) {
    let dataObj;
    try {
      dataObj = JSON.parse(event.data);
    } catch (e) {
      // empty
    }
    if (!dataObj) return;

    if (dataObj.__pw_console_colors) {
      // If this is a color theme message, update themes.
      const colors = dataObj.__pw_console_colors;
      setCurrentTheme(colors.classes);
      if (colors.column_values) {
        columnStyleRules = { ...colors.column_values };
      }
    } else {
      // Normal log message.
      const currentData = { ...dataObj, time: formatDate(new Date()) };
      logSource.append_log(currentData);
    }
  };
})();
