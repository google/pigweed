// Copyright 2021 The Pigweed Authors
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

/* eslint-env browser */

import {makeStyles, Paper, Box} from '@material-ui/core';
import * as React from 'react';
import {default as AnsiUp} from 'ansi_up';
import * as Parser from 'html-react-parser';
import {Frame} from 'pigweedjs/pw_hdlc';
import {Detokenizer} from 'pigweedjs/pw_tokenizer';

type Props = {
  lines: string[];
  frames: Frame[];
};

const useStyles = makeStyles(() => ({
  root: {
    padding: '8px',
    'background-color': '#131416',
    height: '500px',
    'overflow-y': 'auto',
    width: '100%',
    color: 'white',
  },
}));

export function Log(props: Props) {
  const classes = useStyles();
  const [detokenizer, setDetokenizer] = React.useState<Detokenizer | null>(
    null
  );
  const ansiUp = new AnsiUp();

  function row(text: string, index: number) {
    const textHtml = ansiUp.ansi_to_html(text);
    return (
      <Box key={index} sx={{fontFamily: 'Monospace'}}>
        {Parser.default(textHtml)}
      </Box>
    );
  }

  let decodedTokens: string[] = [];
  if (detokenizer !== null) {
    decodedTokens = props.frames.map(frame =>
      (detokenizer as Detokenizer).detokenizeBase64(frame)
    );
  }

  return (
    <>
      <p>
        Upload a tokenizer DB:{' '}
        <input
          id="inp"
          type="file"
          onChange={async e => {
            const tokenCsv = await readFile(e.target.files![0]);
            setDetokenizer(new Detokenizer(String(tokenCsv)));
          }}
        />
      </p>
      <div className={classes.root}>
        {decodedTokens.length > 0
          ? decodedTokens.map(row)
          : props.lines.map(row)}
      </div>
    </>
  );
}

function readFile(file: Blob): Promise<string> {
  return new Promise((resolve, reject) => {
    if (!file) return resolve('');
    const reader = new FileReader();
    reader.onload = function (e) {
      resolve(String(e.target!.result));
    };
    reader.readAsText(file);
  });
}
