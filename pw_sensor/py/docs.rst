.. _module-pw_sensor-py:

==============
Python package
==============
.. pigweed-module::
   :name: pw_sensor

   - **Configure Sensors**: Update the sensor configurations to match your
     requirements. Sensor configurations can be checked at compile time using
     a YAML sensor description.
   - **2 Phase Reading**: Design your own pipeline and priorities. Sensor can be
     read on one thread (or no thread if DMA is available) and the data can be
     decoded on another thread/core/device.

The ``pw_sensor`` Python package provides utilities for generating data and code
for Pigweed sensor drivers.

.. warning::
   This package is under development and the APIs are *VERY* likely to change.

-----------------
Using the package
-----------------
Typical users of ``pw_sensor`` begin by writing a YAML description of their
sensor using the :cs:`pw_sensor/py/pw_sensor/metadata_schema.json` format, e.g.:

.. code-block:: yaml

   deps:
      - "pw_sensor/channels.yaml"
      - "pw_sensor/attributes.yaml"
   compatible:
      org: "Bosch"
      part: "BMA4xx"
   supported-buses:
      - i2c
      - spi
   channels:
      acceleration: []
      die_temperature: []


``pw_sensor`` provides a validator which will resolve any 'default' properties
and make the final YAML easier for code generators to consume. The returned
dictionary uses the :cs:`pw_sensor/py/pw_sensor/resolved_schema.json` format.

Every platform/language may implement their own generator.
Generators consume the validated (schema-compliant) YAML and may produce
many types of outputs, such as a PDF datasheet, a C++ abstract class definition,
or a Zephyr header of definitions describing the sensor.

-------------------
Describing a sensor
-------------------
When describing a sensor from the user's perspective, there are 3 primary points
of interaction:

#. compatible descriptor
#. supported buses
#. channels
#. attributes
#. triggers

.. note::
   Compatible string in Linux's devicetree are used to detail what a hardware
   device is. They include a manufacturer and a model name in the format:
   ``<manufacturer>,<model>``. In order to make this a bit more generic and
   still functional with devicetree, Pigweed's compatible node consists of 2
   separate properties instead of a single string: ``org`` and ``part``. This
   abstracts away the devicetree model such that generators may produce other
   targeted code. To read more about the compatible property, see
   `Understanding the compatible Property`_

Both *channels* and *attributes* covered in :ref:`seed-0120`, while the
*compatible* descriptor allows us to have a unique identifier for each sensor.
Next, we need a way to describe a sensor in a platform and language agnostic
way.

What are supported buses?
=========================
Currently, pw_sensor supports 2 types of sensor buses: i2c and spi. Each sensor
must list at least 1 supported bus. Additional buses may be added as well as
support for custom bus descriptors in downstream projects.

What are channels?
==================
A channel is something that we can measure from a sensor. It's reasonable to ask
"why not call it a measurement"? The answer is that a measurement isn't specific
enough. A single illuminance sensor might provide a lux reading for:
- Total lux (amount of light per square meter)
- Red lux (amount of red light per square meter)
- Green lux (amount of green light per square meter)
- Blue lux (amount of blue light per square meter)
- UV lux (amount of UV light per square meter)
- IR lux (amount of infra-red light per square meter)

All these are a "measurement" of light intensity, but they're different
channels. When defining a channel we need to provide units. In the example
above, the units are lux. Represented by the symbol "lx". It's likely that when
verbose logging is needed or when generating documentation we might want to also
associate a name and a longer description for the channel. This leaves us with
the following structure for a channel:

.. code-block:: yaml

   <channel_id>:
      "name": "string"
      "description": "string"
      "units": <string_units_id>

When we construct the final sensor metadata, we can list the channels supported
by that sensor. In some cases, the same channel may be available more than once.
This happens at times with temperature sensors. In these cases, we can list
multiple instances of a channel. Generally, if no instances are provided, it
will be assumed that there's 1 instance of the channel. Otherwise, we might have
something like:

.. code-block:: yaml

   channels:
      ambient_temperature:
         -  name: "-X"
            description: "temperature measured in the -X direction"
            units: "temperature"
         -  name: "X"
            description: "temperature measured in the +X direction"
            units: "temperature"

What are attributes?
====================
Attributes are used to change the behavior of a sensor. They're defined using
the ``attributes`` key and are structured by associating the defined attribute
type with a channel along with units and a representation (``float``,
``signed``, or ``unsigned``). Here's an example:

.. code-block:: yaml

   attributes:
      -  attribute: "sample_rate"
         channel: "acceleration"
         units: "frequency"
         representation: "float"

When associated with a ``sensor``, ``attributes`` define specific instances of
configurable states for that sensor:

.. code-block:: yaml

   compatible: ...
   channels: ...
   attributes:
      -  {}

What are triggers?
==================
Triggers are events that have an interrupt associated with them. We can define
common triggers which sensors can individually subscribe to. The definition
looks like:

.. code-block:: yaml

   triggers:
      fifo_watermark:
         name: "FIFO watermark"
         description: "Interrupt when the FIFO watermark has been reached (set as an attribute)"

When associated with a ``sensor``, we simply need to match the right key in a
list:

.. code-block:: yaml

   compatible: ...
   channels: ...
   attributes: ...
   triggers:
      -  fifo_watermark

Additional metadata
===================
It's common for applications to require additional metadata that's not
supported or used by Pigweed. These additional values can be added to the
``extras`` key of the sensor:

.. code-block:: yaml

   compatible: ...
   channels: ...
   extras:
     doc-ref: "my-driver-rst-ref"
     memory-req: 512

Values added here can be read by generator scripts.

-----------------------
The ``Validator`` class
-----------------------
The ``Validator`` class is used to take a sensor spec YAML file and expand it
while verifying that all the information is available. It consists of 2 layers:
1. Declarations
2. Definitions

The declaration YAML
====================
The declaration YAML files allow projects to define new sensor channels and
attributes for their drivers. This allows proprietary functionality of sensors
which cannot be made public. Pigweed will provide some baseline set of channels
and attributes.

The following YAML file is used to create a sensor which counts cakes. The
sensor provides the ability to get the total cake count or a separate
large/small cake count (for a total of 3 channels):

.. code-block:: yaml

   # File: my/org/sensors/cakes.yaml
   units:
      cake:
         symbol: "cakes"
   channels:
     cakes:
         description: "The number of cakes seen by the sensor"
         units: "cake"
      cakes_small:
         description: "The number of cakes measuring 6 inches or less"
         units: "cake"
      cakes_large:
         description: "The number of cakes measuring more than 6 inches"
         units: "cake"

The above YAML file will enable a 3 new channels: ``cakes``, ``cakes_small``,
and ``cakes_large``. All 3 channels will use a unit ``cake``. A sensor
implementing this channel would provide a definition file:

.. code-block:: yaml

   # File: my/org/sensors/cake/sensor.yaml
   deps:
      - "my/org/sensors/cakes.yaml"
   compatible:
      org: "myorg"
      part: "cakevision"
   supported-buses:
      - i2c
      - spi
   channels:
      cakes: []
      cakes_small: []
      cakes_large: []

When validated, the above YAML will be converted to fill in the defined values.
This means that ``channels/cakes`` will be automatically filled with:

- ``name: "cakes"``: automatically derived from the name sinde the definition
  did not provide a name.
- ``description: "The number of cakes seen by the sensor"``: attained from the
  definition file.
- ``units``
   - ``name: "cake"``: derived from the definition's ``symbol`` since ``name``
     is not explicitly specified
   - ``symbol: "cake"``: attained from definition file

Output
======
The resulting output uses references. At times described above, things such as
``units`` will be referenced from inside a sensor's channel. When validated, the
corresponding ``units`` entry is guaranteed to be found at the top level
``units`` map. Currently, there will be 5 keys in the returned dictionary:
``sensors``, ``channels``, ``attributes``, ``units``, and ``triggers``.

The ``sensors`` key is a dictionary mapping unique identifiers generated from
the sensor's compatible string to the resolved values. There will always be
exactly 1 of these since each sensor spec is required to only describe a single
sensor (we'll see an example soon for how these are merged to create a project
level sensor description). Each ``sensor`` will contain: ``name`` string,
``description`` description struct, ``compatible`` struct, ``channels``
dictionary, ``attributes`` list, and ``triggers`` list.

The difference between the ``/sensors/channels`` and ``/channels`` dictionaries
is that the former can be thought of as instantiating the latter.

------------------------
Sensor descriptor script
------------------------
A descriptor script is added to Pigweed via the ``pw sensor-desc`` subcommand.
This command allows validating multiple sensor descriptors and passing the
unified descriptor to a generator.

.. list-table:: CLI Flags
   :header-rows: 1

   * - Flag(s)
     - Description
   * - ``--include-path``, ``-I``
     - Directories in which to search for dependency files.
   * - ``--verbose``, ``-v``
     - Increase the verbosity level (can be used multiple times). Default
       verbosity is WARNING, so additional flags increase it to INFO then DEBUG.
   * - ``--generator``, ``-g``
     - Generator ommand to run along with any flags. Data will be passed into
       the generator as YAML through stdin.
   * - ``-o``
     - Write output to file instead of stdout.

What are the include paths used for?
====================================
The sensor descriptor includes a ``deps`` list with file names which define
various attributes used by the sensor. We wouldn't want to check in absolute
paths in these lists, so instead, it's possible to list a relative path to the
root of the project, then add include paths to the tool which will help resolve
the dependencies. This should look familiar to header file resolution in C/C++.

What is a generator?
====================
The sensor descriptor script validates each sensor descriptor file then creates
a superset of all sensors and channels (making sure there aren't conflicts).
Once complete, it will call the generator (if available) and pass the string
YAML representation of the superset into the generator via stdin. Some ideas for
generators:

- Create a header with a list of all channels, assigning each channel a unique
  ID.
- Generate RST file with documentation on each supported sensor.
- Generate stub driver implementation by knowing which channels and attributes
  are supported.

Example run (prints to stdout):

.. code-block:: bash

   $ pw --no-banner sensor-desc -I pw_sensor/ \
     -g "python3 pw_sensor/py/pw_sensor/constants_generator.py --package pw.sensor" \
     pw_sensor/sensor.yaml

.. _Understanding the compatible Property: https://elinux.org/Device_Tree_Usage#Understanding_the_compatible_Property
