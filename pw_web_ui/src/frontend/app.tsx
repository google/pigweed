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
import {Button, makeStyles, Typography} from '@material-ui/core';
import {WebSerialTransport} from '../transport/web_serial_transport';
import {Decoder, Frame} from '@pigweed/pw_hdlc';
import {SerialDebug} from './log';
import * as React from 'react';
import {useState, useRef} from 'react';

const useStyles = makeStyles(() => ({
  root: {
    display: 'flex',
    'flex-direction': 'column',
    overflow: 'hidden',
    width: '1000px',
  },
}));

export function App() {
  const [connected, setConnected] = useState<boolean>(false);
  const [frames, setFrames] = useState<Frame[]>([]);

  const transportRef = useRef(new WebSerialTransport());
  const decoderRef = useRef(new Decoder());
  const classes = useStyles();

  function onConnected() {
    setConnected(true);
    transportRef.current!.chunks.subscribe((item: Uint8Array) => {
      const decoded = decoderRef.current!.process(item);
      for (const frame of decoded) {
        setFrames(old => [...old, frame]);
      }
    });
  }

  return (
    <div className={classes.root}>
      <Typography variant="h1">Pigweb Demo</Typography>
      <Button
        disabled={connected}
        variant="contained"
        color="primary"
        onClick={() => {
          transportRef.current
            .connect()
            .then(onConnected)
            .catch(error => {
              setConnected(false);
              console.log(error);
            });
        }}
      >
        {connected ? 'Connected' : 'Connect'}
      </Button>
      <SerialDebug frames={frames} />
    </div>
  );
}
