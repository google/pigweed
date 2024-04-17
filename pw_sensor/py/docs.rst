.. _module-pw_sensor-py:

------------------------
pw_sensor Python package
------------------------
The ``pw_sensor`` Python package provides utilities for generating data and code
for Pigweed sensor drivers.

.. warning::
   This package is under development and the APIs are *VERY* likely to change.

Using the package
-----------------
Typical users of ``pw_sensor`` begin by writing a YAML description of their
sensor using the `metadata_schema.json`_ format, e.g.:

.. code:: yaml

   deps:
      - "pw_sensor/channels.yaml"
      - "pw_sensor/attributes.yaml"
   compatible:
      org: "Bosch"
      part: "BMA4xx"
   channels:
      acceleration: {}
      die_temperature: {}


``pw_sensor`` provides a validator which will resolve any 'inherited' properties
and make the final YAML easier for code generators to consume. The returned
dictionary uses the `resolved_schema.json`_ format.

Every platform/language may implement their own generator.
Generators consume the validated (schema-compliant) YAML and may produce
many types of outputs, such as a PDF datasheet, a C++ abstract class definition,
or a Zephyr header of definitions describing the sensor.

Describing a sensor
-------------------
When describing a sensor from the user's perspective, there are 3 primary points
of interaction:
#. compatible descriptor
#. channels
#. attributes

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

.. code:: yaml

   <channel_id>:
      "name": "string"
      "description": "string"
      "units":
         "name": "string"
         "symbol": "string"

The current design allows us to define red, green, blue, UV, and IR as
"sub-channels". While we could define them on their own, having a sub-channel
allows us to make the units immutable. This means that ``illuminance`` will
always have the same units as ``illuminance_red``, ``illuminance_green``,
``illuminance_blue``, etc. These are described with a ``sub-channels`` key that
allows only ``name`` and ``description`` overrides:

.. code:: yaml

   <channel_id>:
      ...
      subchannels:
         red:
            name: "custom name"
            description: "custom description"

When we construct the final sensor metadata, we can list the channels supported
by that sensor. In some cases, the same channel may be available more than once.
This happens at times with temperature sensors. In these cases, we can use the
``indicies`` key in the channel specifier of the metadata file. Generally, if
the ``indicies`` is ommitted, it will be assumed that there's 1 instance of the
channel. Otherwise, we might have something like:

.. code:: yaml

   channels:
      ambient_temperature:
         indicies:
            - name: "-X"
              description: "temperature measured in the -X direction"
            - name: "X"
               description: "temperature measured in the +X direction"

What are attributes?
====================
Attributes are used to change the behavior of a sensor. They're defined using
the ``attributes`` key and are structured similarly to ``channels`` since they
can usually be measured in some way. Here's an example:

.. code:: yaml

   attributes:
      sample_rate:
         name: "sample rate"
         description: "frequency at which samples are collected"
         units:
            name: "frequency"
            symbol: "Hz"

When associated with a ``sensor``, ``attributes`` again behave like ``channels``
but without the ``indicies``:

.. code:: yaml

   compatible: ...
   channels: ...
   attributes:
      sample_rate: {}

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

.. code:: yaml

   # File: my/org/sensors/channels.yaml
   channels:
     cakes:
       description: "The number of cakes seen by the sensor"
       units:
         symbol: "cake"
       sub-channels:
         small:
            description: "The number of cakes measuring 6 inches or less"
         large:
            description: "The number of cakes measuring more than 6 inches"

The above YAML file will enable a 3 new channels: ``cakes``, ``cakes_small``,
and ``cakes_large``. All 3 channels will use a unit ``cake``. A sensor
implementing this channel would provide a definition file:

.. code:: yaml

   # File: my/org/sensors/cake/sensor.yaml
   deps:
      - "my/org/sensors/channels.yaml"
   compatible:
      org: "myorg"
      part: "cakevision"
   channels:
      cakes: {}
      cakes_small: {}
      cakes_large: {}

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
The resulting output is verbose and is intended to allow callers of the
validation function to avoid having to cross reference values. Currently, there
will be 2 keys in the returned dictionary: ``sensors`` and ``channels``.

The ``sensors`` key is a dictionary mapping unique identifiers generated from
the sensor's compatible string to the resolved values. There will always be
exactly 1 of these since each sensor spec is required to only describe a single
sensor (we'll see an example soon for how these are merged to create a project
level sensor description). Each ``sensor`` will contain: ``name``,
``description``, ``compatible`` struct, and a ``channels`` dictionary.

The difference between the ``/sensors/channels`` and ``/channels`` dictionaries
is the inclusion of ``indicies`` in the former. The ``indicies`` can be thought
of as instantiations of the ``/channels``. All other channel properties will be
exactly the same.

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

.. code:: bash

   $ pw --no-banner sensor-desc -I pw_sensor/ \
     -g "python3 pw_sensor/py/pw_sensor/constants_generator.py --package pw.sensor" \
     pw_sensor/sensor.yaml

.. _Understanding the compatible Property: https://elinux.org/Device_Tree_Usage#Understanding_the_compatible_Property
.. _metadata_schema.json: https://cs.opensource.google/pigweed/pigweed/+/main:pw_sensor/py/pw_sensor/metadata_schema.json
.. _resolved_schema.json: https://cs.opensource.google/pigweed/pigweed/+/main:pw_sensor/py/pw_sensor/resolved_schema.json
