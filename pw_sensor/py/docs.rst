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
and make the final YAML easier for code generators to consume.

Every platform/language may implement their own generator.
Generators consume the validated (schema-compliant) YAML and may produce
many types of outputs, such as a PDF datasheet, a C++ abstract class definition,
or a Zephyr header of definitions describing the sensor.

Describing a sensor
-------------------
When describing a sensor from the user's perspective, there are 2 primary points
of interaction:
1. channels
2. attributes

Both are covered in :ref:`seed-0120`. We need a way to describe a sensor in a
platform and language agnostic way.

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

.. _metadata_schema.json: https://cs.opensource.google/pigweed/pigweed/+/main:pw_sensor/py/pw_sensor/metadata_schema.json
