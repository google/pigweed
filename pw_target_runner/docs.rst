.. _chapter-test-server:

.. default-domain:: cpp

.. highlight:: sh

---------------
pw_test_server
---------------
The test server module implements a gRPC server which runs unit tests.

Overview
--------
The unit test server is responsible for processing requests to run unit tests
and distributing them among a pool of test workers that run in parallel. This
allows unit tests to be run across multiple devices simultaneously, greatly
speeding up their execution time.

Additionally, the server allows many tests to be queued up at once and scheduled
across available devices, making it possible to automatically run unit tests
from a Ninja build after code is updated. This integrates nicely with the
``pw watch`` command to re-run all affected unit tests after modifying code.

The test server is implemented as a library in various programming languages.
This library provides the core gRPC server and a mechanism through which unit
test worker routines can be registered. Code using the library instantiates a
server with some custom workers for the desired target to run scheduled unit
tests.

The pw_test_server module also provides a standalone ``pw_test_server`` program
which runs the test server with configurable workers that launch external
processes to run unit tests. This program should be sufficient to quickly get
unit tests running in a simple setup, such as multiple devices plugged into a
development machine.

Standalone executable
---------------------
This section describes how to use the ``pw_test_server`` program to set up a
simple unit test server with workers.

Configuration
^^^^^^^^^^^^^
The standalone server is configured from a file written in Protobuf text format
containing a ``pw.test_server.ServerConfig`` message as defined in
``//pw_test_server/config.proto``.

At least one ``worker`` message must be specified. Each of the workers refers to
a script or program which is invoked with the path to a unit test executable
file as a positional argument. Other arguments provided to the program must be
options/switches.

For example, the config file below defines two workers, each connecting to an
STM32F429I Discovery board with a specified serial number.

**server_config.txt**

.. code:: text

  runner {
    command: "stm32f429i_disc1_unit_test_runner"
    args: "--openocd-config"
    args: "targets/stm32f429i-disc1/py/stm32f429i_disc1_utils/openocd_stm32f4xx.cfg"
    args: "--serial"
    args: "066DFF575051717867013127"
  }

  runner {
    command: "stm32f429i_disc1_unit_test_runner"
    args: "--openocd-config"
    args: "targets/stm32f429i-disc1/py/stm32f429i_disc1_utils/openocd_stm32f4xx.cfg"
    args: "--serial"
    args: "0667FF494849887767196023"
  }


Running the server
^^^^^^^^^^^^^^^^^^
To start the standalone server, run the ``pw_test_server`` program with the
``-server`` flag and point it to your config file.

.. code:: text

  $ pw_test_server -server -config server_config.txt -port 8080


Sending test requests
^^^^^^^^^^^^^^^^^^^^^
To request the server to run a unit test, the ``pw_test_server`` program is
run in client mode, specifying the path to the unit test executable through a
``-test`` option.

.. code:: text

  $ pw_test_server -host localhost -port 8080 -test /path/to/my/test.elf

This command blocks until the test has run and prints out its output. Multiple
tests can be scheduled in parallel; the server will distribute them among its
available workers.

Library APIs
------------
To use the test server library in your own code, refer to one of its programming
language APIs below.

.. toctree::
  :maxdepth: 1

  go/docs
