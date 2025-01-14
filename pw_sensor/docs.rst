.. _module-pw_sensor:

=========
pw_sensor
=========
.. pigweed-module::
   :name: pw_sensor

This is the main documentation file for pw_sensor. It is under construction.

.. toctree::
   :maxdepth: 1

   py/docs

Defining types
==============
Pigweed provides a data-definition layer for sensors. This allows the properties
of a sensor to be declared once and shared across multiple languages or runtimes.
More information is available in :ref:`module-pw_sensor-py`.

Once we define our units, measurements, attributes, and triggers we can import
them and use them in our language-specific sensor code.

Here's an example sensor definition YAML file for a custom sensor made by a
company called "MyOrg" with a part ID "MyPaRt12345". This sensor supports
reading acceleration and its internal die temperature. We can also configure
the sample rate for the acceleration readings.

.. code-block:: yaml

   deps:
      - "third_party/pigweed/pw_sensor/attributes.yaml"
      - "third_party/pigweed/pw_sensor/channels.yaml"
      - "third_party/pigweed/pw_sensor/units.yaml"
   compatible:
      org: "MyOrg"
      part: "MyPaRt12345"
   channels:
      acceleration: []
      die_temperature: []
   attributes:
      -  attribute: "sample_rate"
         channel: "acceleration"
         units: "frequency"
         representation: "unsigned"

Now that we have our sensor spec in a YAML file we can use it in our C++ code:

.. tab-set::

   .. tab-item:: Bazel

      Compiling one or more sensor YAML files into a header is done by a call to
      the ``pw_sensor_library`` rule. It looks like:

      .. code-block::

         load("@pigweed//pw_sensor:sensor.bzl", "pw_sensor_library")

         pw_sensor_library(
            name = "my_sensor_lib",
            out_header = "my_app/generated/sensor_constants.h",
            srcs = [
               "my_sensor0.yaml",
               "my_sensor1.yaml",
            ],
            inputs = [
               "@pigweed//pw_sensor:attributes.yaml",
               "@pigweed//pw_sensor:channels.yaml",
               "@pigweed//pw_sensor:triggers.yaml",
               "@pigweed//pw_sensor:units.yaml",
            ],
            generator_includes = ["@pigweed//"],
            deps = [
               "@pigweed//pw_sensor:pw_sensor_types",
               "@pigweed//pw_containers:flag_map",
               "@pigweed//pw_tokenizer:pw_tokenizer",
            ],
         )

   .. tab-item:: GN

      Compiling one or more sensor YAML files into a header is done by a call to
      the ``pw_sensor_library`` template. It looks like:

      .. code-block::

         import("$dir_pw_sensor/sensor.gni")

         pw_sensor_library("my_sensor_lib") {
           out_header = "my_app/generated/sensor_constants.h"
           sources = [
            "my_sensor0.yaml",
            "my_sensor1.yaml",
           ]
           inputs = [
            "$dir_pw_sensor/attributes.yaml",
            "$dir_pw_sensor/channels.yaml",
            "$dir_pw_sensor/triggers.yaml",
            "$dir_pw_sensor/units.yaml",
           ]
           generator_includes = [ getenv["PW_ROOT"] ]
           public_deps = [
            "$dir_pw_sensor:pw_sensor_types",
            "$dir_pw_containers:flag_map",
            "$dir_pw_tokenizer:pw_tokenizer",
           ]
         }

   .. tab-item:: CMake

      Compiling one or more sensor YAML files into a header is done by a call to
      the ``pw_sensor_library`` function. It looks like:

      .. code-block::

         include($ENV{PW_ROOT}/pw_sensor/sensor.cmake)

         # Generate an interface library called my_sensor_lib which exposes a
         # header file that can be included as
         # "my_app/generated/sensor_constants.h".
         pw_sensor_library(my_sensor_lib
            OUT_HEADER
               my_app/generated/sensor_constants.h
            INPUTS
               $ENV{PW_ROOT}/attributes.yaml
               $ENV{PW_ROOT}/channels.yaml
               $ENV{PW_ROOT}/triggers.yaml
               $ENV{PW_ROOT}/units.yaml
            GENERATOR_INCLUDES
               $ENV{PW_ROOT}
            SOURCES
               my_sensor0.yaml
               my_sensor1.yaml
            PUBLIC_DEPS
               pw_sensor.types
               pw_containers
               pw_tokenizer
         )

The final product is an interface library that can be linked and used in your
application. As an example:

.. code-block::

   #include "my_app/generated/sensor_constants.h"

   int main() {
     PW_LOG_INFO(
       PW_LOG_TOKEN_FMT() " is measured in " PW_LOG_TOKEN_FMT(),
       pw::sensor::channels::kAcceleration::kMeasurementName,
       pw::sensor::GetMeasurementUnitNameFromType(
         pw::sensor::channels::kAcceleration::kUnitType
       )
     );
   }

--------------
Under the hood
--------------

In order to communicate with Pigweed's sensor stack, there are a few type
definitions that are used:

* Unit types - created with ``PW_SENSOR_UNIT_TYPE``. These can be thought of as
  defining things like "meters", "meters per second square",
  "radians per second", etc.
* Measurement types - created with ``PW_SENSOR_MEASUREMENT_TYPE``. These are
  different things you can measure with a given unit. Examples: "height",
  "width", and "length" would all use "meters" as a unit but are different
  measurement types.
* Attribute types - created with ``PW_SENSOR_ATTRIBUTE_TYPE``. These are
  configurable aspects of the sensor. They are things like sample rates, tigger
  thresholds, etc. Attributes are unitless until they are paired with the
  measurement type that they modify. As an example "range" attribute for
  acceleration measurements would be in "m/s^2", while a "range" attribute for
  rotational velocity would be in "rad/s".
* Attribute instances - created with ``PW_SENSOR_ATTRIBUTE_INSTANCE``. These
  lump together an attribute with the measurement it applies to along with a
  unit to use. Example: Attribute("sample rate") + Measurement("acceleration") +
  Unit("frequency").
* Trigger types - created with ``PW_SENSOR_TRIGGER_TYPE``. These are events that
  affect the streaming API. These can be events like "fifo full", "tap",
  "double tap"

Developers don't need to actually touch these, as they're automatically called
from the generated sensor library above. The important thing from the input YAML
file is that our final generated header will include the following types:
``attributes::kSampleRate``, ``channels::kAcceleration``,
``channels::kDieTemperature``, and ``units::kFrequency``. All of these are used
by our sensor.

A later change will also introduce the ``PW_SENSOR_ATTRIBUTE_INSTANCE``.
