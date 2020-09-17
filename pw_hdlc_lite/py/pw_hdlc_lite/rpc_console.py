# Copyright 2020 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Example console for creating a client.

Console can be initiated by running:

  python -m pw_hdlc_lite.client_console --device /dev/ttyUSB0

An example echo RPC command:
print(rpc_client.channel(1).rpcs.pw.rpc.EchoService.Echo(msg="hello!"))
"""
from pathlib import Path
import argparse
import threading
import logging
import time
import code
import serial

from pw_hdlc_lite import decoder
from pw_hdlc_lite.encoder import encode_information_frame
from pw_protobuf_compiler import python_protos
from pw_rpc import callback_client, client, descriptors

_LOG = logging.getLogger(__name__)


def parse_arguments():
    """Parses and returns the command line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-d',
                        '--device',
                        required=True,
                        help='used to specify device port')
    parser.add_argument('-b',
                        '--baudrate',
                        type=int,
                        required=True,
                        help='used to specify baudrate')
    return parser.parse_args()


def configure_serial(device_port, baudrate):
    """Configures and returns a serial."""
    ser = serial.Serial(device_port)
    ser.baudrate = baudrate
    return ser


def construct_rpc_client(ser):
    """Constructs and returns an RPC client using serial ser."""
    def delayed_write(data):
        """Adds a delay between consective bytes written over serial"""
        for byte in data:
            time.sleep(0.001)
            ser.write(bytes([byte]))

    # Creating a channel object
    hdlc_channel_output = lambda data: ser.write(
        encode_information_frame(data, delayed_write))
    channel = descriptors.Channel(1, hdlc_channel_output)

    # Creating a list of modules that provide the .proto service methods
    modules = python_protos.compile_and_import([
        Path(__file__, '..', '..', '..', '..', 'pw_rpc', 'pw_rpc_protos',
             'echo.proto')
    ])

    # Using the modules and channel to create and return an RPC Client
    return client.Client.from_modules(callback_client.Impl(), [channel],
                                      modules)


def read_and_process_data(rpc_client, ser):
    """Reads in the data, decodes the bytes and then processes the rpc."""
    decode = decoder.FrameDecoder()

    while True:
        byte = ser.read()
        for frame in decode.process(byte):
            if frame.ok():
                if not rpc_client.process_packet(frame):
                    _LOG.error('Packet not handled by rpc client: %s', frame)
            else:
                _LOG.error('Failed to parse frame: %s', frame.status.value)


def main():
    """Main function."""
    args = parse_arguments()
    ser = configure_serial(args.device, args.baudrate)

    rpc_client = construct_rpc_client(ser)

    # Starting the reading and processing on a thread
    threading.Thread(target=read_and_process_data,
                     daemon=True,
                     args=(rpc_client, ser)).start()

    # Opening an interactive console that allows sending RPCs.
    code.interact(banner="Interactive console to run RPCs", local=locals())


if __name__ == "__main__":
    main()
