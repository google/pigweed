.. _docs-first-time-setup-linux:

=================================
Get started with Pigweed on Linux
=================================
.. _docs-first-time-setup-linux-express-setup:

-------------
Express setup
-------------
Run the following commands, and you should be ready to start developing with
Pigweed:

.. inclusive-language: disable

.. code-block:: sh

   sudo apt install git build-essential
   curl -LJo /etc/udev/rules.d/60-openocd.rules https://raw.githubusercontent.com/openocd-org/openocd/master/contrib/60-openocd.rules

.. inclusive-language: enable

------------------------------
Manual setup with explanations
------------------------------
This section expands the contents of the express setup into more detailed
explanations of Pigweed's Linux prerequisites. If you have already gone through
the :ref:`docs-first-time-setup-linux-express-setup`, you don't need to go
through these steps.

Install prerequisites
=====================
Most Linux installations should work out-of-the-box and not require any manual
installation of prerequisites beyond basics like ``git`` and
``build-essential`` (or the equivalent for your distro). If you already do
software development, you likely already have these installed.

To ensure you have the necessary prerequisites, you can run the following
command:

.. code-block:: sh

   sudo apt install git build-essential

Configure system settings
=========================
.. inclusive-language: disable

To flash devices using `OpenOCD <https://openocd.org/>`_, you will need to
extend your system udev rules by adding a new configuration file in
``/etc/udev/rules.d/`` that lists the hardware debuggers you'll be using. The
OpenOCD repository has a good
`example udev rules file <https://github.com/openocd-org/openocd/blob/master/contrib/60-openocd.rules>`_
that includes many popular hardware debuggers.

.. inclusive-language: enable

-------------
Support notes
-------------
Linux is Pigweed's recommended platform for embedded software development. When
developing on Linux, you can enjoy all of Pigweed's benefits like tokenized
logging, automated on-device unit testing, RPC, and rich build system and IDE
integrations.
