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
import {
  Button,
  TextField,
  makeStyles,
  Typography,
  createTheme,
  ThemeProvider,
} from '@material-ui/core';
import {ToggleButtonGroup, ToggleButton} from '@material-ui/lab';
import {WebSerialTransport} from '../transport/web_serial_transport';
import {Decoder, Frame, Encoder} from '@pigweed/pw_hdlc';
import {SerialLog} from './serial_log';
import {Log} from './log';
import * as React from 'react';
import {useState, useRef} from 'react';
import {Channel, Client, UnaryMethodStub} from '@pigweed/pw_rpc';
import {Status} from '@pigweed/pw_status';
import {ProtoCollection} from 'web_proto_collection/generated/ts_proto_collection';

const RPC_ADDRESS = 82;

const darkTheme = createTheme({
  palette: {
    type: 'dark',
    primary: {
      main: '#42a5f5',
    },
    secondary: {
      main: 'rgb(232, 21, 165)',
    },
  },
});

const useStyles = makeStyles(() => ({
  root: {
    display: 'flex',
    'flex-direction': 'column',
    'align-items': 'flex-start',
    color: 'rgba(255, 255, 255, 0.8);',
    overflow: 'hidden',
    width: '1000px',
  },
  connect: {
    'margin-top': '16px',
    'margin-bottom': '20px',
  },
  rpc: {
    margin: '10px',
  },
}));

export function App() {
  const [connected, setConnected] = useState<boolean>(false);
  const [frames, setFrames] = useState<Frame[]>([]);
  const [logLines, setLogLines] = useState<string[]>([]);
  const [echoText, setEchoText] = useState<string>('Hello World');
  const [logViewer, setLogViewer] = useState<string>('log');

  const classes = useStyles();

  const transportRef = useRef(new WebSerialTransport());
  const decoderRef = useRef(new Decoder());
  const encoderRef = useRef(new Encoder());
  const protoCollectionRef = useRef(new ProtoCollection());
  const channelsRef = useRef([
    new Channel(1, (bytes: Uint8Array) => {
      sendPacket(transportRef.current!, bytes);
    }),
  ]);
  const clientRef = useRef<Client>(
    Client.fromProtoSet(channelsRef.current!, protoCollectionRef.current!)
  );
  const echoService = clientRef
    .current!.channel()!
    .methodStub('pw.rpc.EchoService.Echo') as UnaryMethodStub;

  function onConnected() {
    setConnected(true);
    transportRef.current!.chunks.subscribe((item: Uint8Array) => {
      const decoded = decoderRef.current!.process(item);
      for (const frame of decoded) {
        setFrames(old => [...old, frame]);
        if (frame.address === RPC_ADDRESS) {
          const status = clientRef.current!.processPacket(frame.data);
        }
        if (frame.address === 1) {
          const decodedLine = new TextDecoder().decode(frame.data);
          const date = new Date();
          const logLine = `[${date.toLocaleTimeString()}] ${decodedLine}`;
          setLogLines(old => [...old, logLine]);
        }
      }
    });
  }

  function sendPacket(
    transport: WebSerialTransport,
    packetBytes: Uint8Array
  ): void {
    const hdlcBytes = encoderRef.current.uiFrame(RPC_ADDRESS, packetBytes);
    transport.sendChunk(hdlcBytes);
  }

  function echo(text: string) {
    const request = new echoService.method.responseType();
    request.setMsg(text);
    echoService
      .call(request)
      .then(([status, response]) => {
        console.log(response.toObject());
      })
      .catch(() => {});
  }

  return (
    <div className={classes.root}>
      <ThemeProvider theme={darkTheme}>
        <Typography variant="h3">Pigweb Demo</Typography>
        <Button
          className={classes.connect}
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
        <ToggleButtonGroup
          value={logViewer}
          onChange={(event, selected) => {
            setLogViewer(selected);
          }}
          exclusive
        >
          <ToggleButton value="log">Log Viewer</ToggleButton>
          <ToggleButton value="serial">Serial Debug</ToggleButton>
        </ToggleButtonGroup>
        {logViewer === 'log' ? (
          <Log lines={logLines} />
        ) : (
          <SerialLog frames={frames} />
        )}
        <span className={classes.rpc}>
          <TextField
            id="echo-text"
            label="Echo Text"
            disabled={!connected}
            value={echoText}
            onChange={event => {
              setEchoText(event.target.value);
            }}
          ></TextField>
          <Button
            disabled={!connected}
            variant="contained"
            color="primary"
            onClick={() => {
              echo(echoText);
            }}
          >
            Send Echo RPC
          </Button>
        </span>
      </ThemeProvider>
    </div>
  );
}
