.. _seed-0119:

=============
0119: Sensors
=============
.. seed::
   :number: 0119
   :name: Sensors
   :status: Accepted
   :proposal_date: 2023-10-18
   :cl: 175479
   :authors: Yuval Peress
   :facilitator: Taylor Cramer

-------
Summary
-------
This SEED proposes that Pigweed provide a full end-to-end sensor solution,
including a shared set of abstractions and communication primitives. This
will allow Pigweed users to build reusable drivers and portable applications.

----------
Motivation
----------
Sensors are part of nearly every embedded device. However, sensor drivers and
application logic are often bespoke. Current clients of Pigweed must write
sensor drivers using various bus APIs. This results in drivers and applications
that are deeply integrated and not reusable. When drivers are custom-built for
particular applications, they frequently omit significant functionality needed
by other potential users.

This solution does not scale.

Introducing a standard sensor driver interface will allow Pigweed users to
build drivers that can be reused across applications, and applications that
are compatible with multiple different drivers.

A second layer of a sensor framework will then be introduced. This secondary
layer will provide functionality required for more advanced applications and
will include support for virtual sensors and multi-client connections per
sensor. It will also be compatible with Android's `CHRE`_.

---------
Prior Art
---------
Sensor APIs are available today on many platforms:

- `Android`_
- `Apple iOS and macOS`_
- `Linux`_
- `Windows`_
- `Zephyr`_

Where possible, we strive to make it possible for Pigweed's Sensor API to
delegate to these platform-level APIs without significant loss of
functionality.

However, where performance, code size, API comprehensibility, or reusability
are of particular concern, priority is given to ensuring that Pigweed-native
sensor drivers and APIs meet the needs of Pigweed applications.

-------------
System Design
-------------
At the high level, the ``pw_sensor`` module will introduce the following
concepts:

- ``Sensor``: a class which will enable a single user to **configure**,
  **read**, or **stream** data. The implementation can vary. Some
  implementations may call down to an already implemented sensor driver of the
  underlying RTOS, others may be software sensors which themselves will open
  ``Connection``\s to other sensors, or in other cases, the ``Sensor``
  implementation will actually perform remote calls to a sensor on another SoC
  or board.
- ``Connection``: a class which will enable us to abstract away the single
  client concept of the ``Sensor``. A ``Connection`` is owned by some logic
  and uses the same configuration as the ``Sensor``. The only difference is
  that the Pigweed provided sensor framework will then mux all the
  configurations targeting the same sensor into a single configuration. This
  single configuration will be used for the actual ``Sensor`` object (see
  `pw sensor configuration mux`_ as an example). When data becomes
  available, the sensor framework will demux the data and provide it to the
  various clients based on the live ``Connection``\s.

.. image:: 0119-pw-sensor/high-level-view.svg

Actually reading the data is a 2 step process. The first is getting the data
from the ``Sensor`` object. The second step is decoding it into something
that's usable. Data from the first step is generally still in register format
and includes some headers from the driver which will allow the ``Decoder`` to
convert the values to the appropriate representation (most likely ``float``).

.. image:: 0119-pw-sensor/data-pipeline.svg

-----------------
The Sensor Driver
-----------------
The sensor driver provides the 3 functionalities listed above (configuring,
reading, and streaming).

Each sensor may assume it has exactly 1 caller. It is up to the application to
leverage the right locking and arbitration mechanisms if a sensor is to be
shared between parts of the code. Doing this keeps the driver implementor's job
much simpler and allows them to focus on performance, testing, and simplicity.
In order to provide consumers of sensor data with more advanced features, a
sensor framework will also be provided and discussed in the following section
:ref:`the sensor framework`.

Asynchronous APIs
-----------------
Bus transactions are asynchronous by nature and often can be set up to use an
interrupt to signal completion. By making the assumption that all reading,
writing, and configuring of the sensor are asynchronous, it's possible to
provide an API which is callable even from an interrupt context. The final
result of any operation of the API will use the ``pw_async`` logic. Doing so
will allow the clients to avoid concerns about the context in which callbacks
are called and having to schedule work on the right thread. Instead, they're
able to rely on the async dispatcher.

Configuring
-----------
Sensors generally provide some variable configurations. In some cases, these
configurations are global (i.e. they apply to the device). An example of such
global configuration might be a FIFO watermark (via a batching duration). In
other cases, the configuration might apply to specific sub-sensors /
measurements. An example of a specific configuration attribute can be the sample
rate which on an Inertial Measurement Unit (IMU) might be an acceleration and
rotational velocity. We can describe each configuration with the following:

- Measurement type: such as acceleration or rotational velocity
- Measurement index: the index of the measurement. This is almost always 0, but
  some sensors do provide multiple samples of the same measurement type (range
  finders). In which case it's possible that we would need to configure
  separate instances of the sensor.
- Attribute: such as the sample rate, scale, offset, or batch duration
- Value: the value associated with the configuration (might be a ``bool``,
  ``float``, ``uint64_t``, or something else entirely).

Here's an example:

+---------------+----------------+--------+
| Measurement   | Attribute      | Value  |
+-------+-------+----------------+--------+
| Type  | Index |                |        |
+=======+=======+================+========+
| Accel | 0     | Sample Rate    | 1000Hz |
+-------+-------+----------------+--------+
| All   | 0     | Batch duration | 200ms  |
+-------+-------+----------------+--------+

Reading
-------
Reading a sensor involves initiating some I/O which will fetch an unknown amount
of data. As such, the operation will require some ``Allocator`` to be used along
with a possible *Measurement Type* filter to limit the amount of data being
retrieved and stored. When complete, the result will be provided in a
``pw::ConstByteSpan`` which was allocated from the ``Allocator``. This byte span
can be cached or possibly sent over a wire for decoding.

Streaming
---------
Streaming data from a sensor is effectively the same as reading the sensor with
minor considerations. Instead of filtering "what" data we want, we're able to
specify "when" we want the data. This happens in the form of one or more
interrupts. There will be some additional control over the data returned from
the stream; it will come in the form of an operation. 3 operations will be
supported for streams:

- ``Include``: which tells the driver to include any/all associated data with
  the trigger. As an example, a batching trigger will include all the data from
  the FIFO so it can be decoded later.
- ``Drop``: which tells the driver to get rid of the associated data and just
  report that the event happened. This might be done on a FIFO full event to
  reset the state and start over.
- ``Noop``: which tells the driver to just report the event and do nothing with
  the associated data (maybe the developer wants to read it separately).

.. note::
   We do not allow specifying a measurement filter like we do in the reading API
   because it would drastically increase the cost of the driver developer.
   Imagine a trigger for the stream on an IMU using the batch duration where we
   want to only get the acceleration values from the FIFO. This scenario doesn't
   make much sense to support since the caller should simply turn off the
   gyroscope in the FIFO via the configuration. Having the gyroscope
   measurements in the FIFO usually means they will simply be discarded when
   read. This puts a very heavy burden on the driver author to place a filter in
   the reader logic as well as in the decoder.

Decoder
-------
The decoder provides functionality to peek into the raw data returned from the
``Sensor``. It should implement functionality such as:

- Checking if a measurement type is present in the buffer. If so, how many
  :ref:`pw sensor define frame` and indices?
- Checking how much memory will be required to decode the frame header (which
  includes information like the base timestamp, frame count, etc) and each frame
  of data.
- Decoding frames of data. There will be a hard mapping of a measurement type to
  data representation. Example: a measurement type of *Acceleration* will always
  decode to a ``struct acceleration_data``.

.. _the sensor framework:

--------------------
The Sensor Framework
--------------------
The sensor framework is an abstraction above the ``Sensor`` class which provides
a superset of features but on a ``Connection`` object. The framework will be a
singleton object and will provide consumers the following:

- List all sensors represented as read-only ``SensorInfo`` objects.
- Ability to open/close connections. When a connection is open, a ``Connection``
  object is returned. The connection can be closed by either calling
  ``Connection::Close()`` or simply calling the ``Connection``\s deconstructor.

Once the sensor framework is linked into the application, ``Sensor`` objects
should not be manipulated directly. Instead, the only direct client of the
``Sensor``\s is the framework. Users can request a list of all the sensors
(``SensorInfo`` objects). Once the client finds the sensor they want to listen
to, they can request a ``Connection`` to be opened to that sensor. A
``Connection`` provides very similar functionality to that of the ``Sensor`` but
is owned by the framework. As an example, a configuration change made on the
``Connection`` will trigger the framework to mux together all the configurations
of all the connections that point to the same ``Sensor``. Once complete, a
single configuration will be selected and set on the ``Sensor``. Similarly, when
the ``Sensor`` produces data, the data will be demuxed and sent to all the open
``Connection``\s.

Virtual Sensors
---------------
This framework provides an interesting way to build portable virtual (soft)
sensors. If the library containing the virtual sensors depends on the framework,
it's possible for the virtual sensors to own connections, configure the sources,
and perform all the necessary signal processing without compromising other
unknown clients of the same sensor (since the framework handles all the
configuration arbitration).

As an example, a hinge angle sensor could accept 2 ``Connection`` objects to
accelerometers in its constructor. When the hinge angle sensor is configured
(such as sample rate) it would pass the configuration down to the connections
and request the same sample rate from the 2 accelerometers.

--------
Glossary
--------

.. _pw sensor define frame:

Frame
   A single time slice. Usually this is used to reference a single sample of
   multiple sensor measurement types such as an IMU measuring both acceleration
   and rotational velocity at the same time.

--------
Examples
--------

.. _pw sensor configuration mux:

Pigweed will provide some default mechanism for muxing together
``Configuration`` objects. Like many other modules and backends in Pigweed, this
mechanism will be overridable by the application. Below is an example of what it
might look like:

- Assume a client requests samples at 1kHz
- Assume a second client requests samples at 1.1kHz
- The resulting sample rate is 1.1kHz, but it's much more likely that the sensor
  doesn't support 1.1kHz and will instead be giving both clients 2kHz of
  samples. It will then be up to both clients to decimate the data correctly.

.. note::
   Decimating 2kHz down to 1.1kHz isn't as simple as just throwing away 9
   samples for every 20. What the client is likely to do is use a weighted
   average in order to simulate the 1.1kHz. It's likely that Pigweed should at
   some point provide a decimation library with a few common strategies which
   would help developers with the task.

.. _`Android`: https://developer.android.com/develop/sensors-and-location/sensors/sensors_overview
.. _`Apple iOS and macOS`: https://developer.apple.com/documentation/sensorkit
.. _CHRE: https://source.android.com/docs/core/interaction/contexthub
.. _Linux: https://www.kernel.org/doc/html/v4.14/driver-api/iio/intro.html
.. _Windows: https://learn.microsoft.com/en-us/windows/win32/sensorsapi/the-sensor-object
.. _Zephyr: https://docs.zephyrproject.org/apidoc/latest/group__sensor__interface.html
