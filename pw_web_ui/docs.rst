.. _module-pw_web_ui:

---------
pw_web_ui
---------

Pigweed provides an NPM package with modules to build web UIs for Pigweed
devices.

Also included is a basic React app that demonstrates using the npm package.


Available Modules
=============================
Following Pigweed modules are included in the NPM package:

- `pw_hdlc <https://pigweed.dev/pw_hdlc/#typescript>`_
- `pw_rpc <https://pigweed.dev/pw_rpc/ts/>`_
- `pw_tokenizer <https://pigweed.dev/pw_tokenizer/#typescript>`_
- `pw_transfer <https://pigweed.dev/pw_transfer/#typescript>`_

To help with connecting to WebSerial and listening for serial data, a helper
class is also included under ``WebSerial.WebSerialTransport``. Here is an example
usage:

.. code:: javascript

   import { WebSerial, pw_hdlc } from 'pigweedjs';

   const transport = new WebSerial.WebSerialTransport();
   const decoder = new pw_hdlc.Decoder();

   // Present device selection prompt to user
   await transport.connect();

   // Listen and decode HDLC frames
   transport.chunks.subscribe((item) => {
     const decoded = decoder.process(item);
     for (const frame of decoded) {
       if (frame.address === 1) {
         const decodedLine = new TextDecoder().decode(frame.data);
         console.log(decodedLine);
       }
     }
   });

Installation
=============

You can install ``pigweedjs`` in your web application by:

.. code:: bash

   $ npm install --save pigweedjs


Getting Started
================

After installing, you can import modules from ``pigweedjs`` in this way:

.. code:: javascript

   import { pw_rpc, pw_tokenizer } from 'pigweedjs';

Click on each module above to see its usage.
