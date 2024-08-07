.. _showcase-sense-tutorial-webapp:

======================================================
11. Communicate with your Pico over the Web Serial API
======================================================
.. warning::

   (2024 August 7) This tutorial doesn't quite work yet.
   Check back tomorrow!

.. _Web Serial API: https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API

:ref:`module-pw_web` makes it possible to create custom web apps that
communicate with embedded devices over the `Web Serial API`_. Try
monitoring and controlling your Pico over the web now.

.. _NVM: https://github.com/nvm-sh/nvm?tab=readme-ov-file#installing-and-updating
.. _those browsers don't support: https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API#browser_compatibility

#. Install `NVM`_. You'll need to close and re-open your terminal
   to get ``nvm`` on your command line path.

   .. note::

      You can skip the NVM steps if you've already got NPM installed
      on your machine. Also, you can install NPM however you want;
      NVM is just one fast option.

#. Use NVM to install the long-term support (LTS) release of Node.js:

   .. code-block:: console

      nvm install --lts

#. Set your working directory to ``web_app``:

   .. code-block:: console

      cd <path>/<to>/sense/web_app

#. Install the web app's dependencies:

   .. code-block:: console

      npm install

#. Run the web app:

   .. code-block:: console

      npm run dev

   In the output, note the line that looks like
   ``> Local: http://127.0.0.1:8000/`` (your actual URL may be different).
   This is the URL you'll need in the next step.

#. Open the web app in Google Chrome or Microsoft Edge. **This application does
   not work in other browsers like Apple Safari or Mozilla Firefox** because
   `those browsers don't support`_ the Web Serial API.

   .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/web_app.png

#. Click **Connect** and select the **Pico** option (or the
   **Debug Probe (CMSIS-DAP)** option if you're using a Debug Probe).

   .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/web_app_connect.png

#. Click **Blink 5 Times**. You should see the LED on your Pico blink 5 times.

   .. note::

      The Pico is not running a web server. The web app spins up its own local
      server and then communicates with the Pico by sending RPCs over the
      Web Serial API.

#. Click **Chart Temperature**. You should see a chart of the Pico's onboard
   temperature getting updated every second.

   .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/web_app_temperature.png

   .. admonition:: Troubleshooting

      **Nothing happens after clicking the Chart Temperature button**. This is a
      known issue that we're actively debugging.

#. Close the browser tab running the web app.

.. _showcase-sense-tutorial-webapp-summary:

-------
Summary
-------
Projects built on top of Pigweed often build themselves custom web apps
to make development, support, and manufacturing processes faster. Other
teams create web apps that let their end customers manage their own
devices.

Next, head over to :ref:`showcase-sense-tutorial-factory` to get
familiar with how Pigweed can make it easier to test newly
manufactured devices.
