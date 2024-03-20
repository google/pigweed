.. _seed-0120:

==========================
0120: Sensor Configuration
==========================
.. seed::
   :number: 0120
   :name: Sensors Config
   :status: Open for Comments
   :proposal_date: 2023-11-28
   :cl: 183150
   :authors: Yuval Peress
   :facilitator: Taylor Cramer

-------
Summary
-------
This SEED details the configuration aspect of both sensors and the sensor
framework that will reside under the ``pw_sensor`` module. Under this design,
both a ``Sensor`` and a ``Connection`` object will be configurable with the same
API. As such, the ``Configuration`` is a part of both layers for the sensor
stack:

* There exists a ``Configuration`` class which holds the currently requested
  configuration.
* A ``Configuration`` is owned by a ``Sensor`` in a 1:1 relationship. Each
  sensor only supports 1 configuration.
* The sensor framework (a layer above the ``Sensor`` driver) has the concept of
  a ``Connection``. You can open multiple connections to the same sensor and the
  framework will handle the multiplexing. Each ``Connection`` owns a
  ``Configuration`` in a similar 1:1 relationship like the ``Sensor``. The only
  difference is that when a ``Connection``'s configuration changes, the
  framework arbitrates the multiple ``Connection``\s to produce a single final
  ``Configuration`` of the ``Sensor``.

.. mermaid::
   :alt: Configuration relationship
   :align: center

   classDiagram
       class Configuration
       class Sensor
       class Connection
       class SensorFramework
       Sensor "1" *-- "1" Configuration
       Connection "1" *-- "1" Configuration
       SensorFramework "1" *-- "*" Sensor
       SensorFramework "1" *-- "*" Connection

----------
Motivation
----------
Making sensor drivers configurable lends to the reusability of the driver.
Additionally, each ``Connection`` in the sensor framework should be able to
convey the requested configuration of the client. As depicted above, a
``Connection`` will own a single ``Configuration``. Once a change is made, the
framework will process the change and form a union of all the ``Configuration``
objects that are pointed to the same ``Sensor``. This new union will be used as
the single new configuration of the ``Sensor`` and all ``Connection``\s will be
notified of the change.

------------
Design / API
------------

Measurement Types
-----------------
Measurement types include things like *acceleration*, *rotational velocity*,
*magnetic field*, etc. Each type will be described by a ``uint16_t`` hash of the
name and the unit strings each. This makes the measurement type automatically
easy to log in a human readable manner when leveraging tokenized logging.
Additionally, the final measurement type (being the concatination of 2 tokens)
is represented as a ``uint32_t``.

.. code:: c++

   union MeasurementType {
     struct {
       uint16_t name_token;
       uint16_t unit_token;
     }
     uint32_t type;
   };

   #define PW_SENSOR_MEASUREMENT_TYPE(domain, name_str, unit_str)              \
     {                                                                         \
       .name_token =                                                           \
           PW_TOKENIZE_STRING_MASK(domain, 0xffff, name_str),                  \
       .unit_token =                                                           \
           PW_TOKENIZE_STRING_MASK(domain, 0xffff, unit_str),                  \
     }

Pigweed would include some common measurement types:

.. code:: c++

   constexpr MeasurementType kAcceleration =
       PW_SENSOR_MEASUREMENT_TYPE("PW_SENSOR_MEASUREMENT_TYPE", "acceleration", "m/s2");
   constexpr MeasurementType kRotationalVelocity =
       PW_SENSOR_MEASUREMENT_TYPE("PW_SENSOR_MEASUREMENT_TYPE", "rotational velocity", "rad/s");
   constexpr MeasurementType kMagneticField =
       PW_SENSOR_MEASUREMENT_TYPE("PW_SENSOR_MEASUREMENT_TYPE", "magnetic field", "T");
   constexpr MeasurementType kStep =
       PW_SENSOR_MEASUREMENT_TYPE("PW_SENSOR_MEASUREMENT_TYPE", "step count", "step");

Applications can add their own unique units which will not collide as long as
they have a unique domain, name, or unit representation:

.. code:: c++

   /// A measurement of how many pancakes something is worth.
   constexpr MeasurementType kPancakes =
       PW_SENSOR_MEASUREMENT_TYPE("iHOP", "value", "pnks");

Attribute Types
---------------
Attribute types are much simpler that ``MeasurementTypes`` since they derive
their units from the measurement type. Instead, they'll just be
represented via a single token:

.. code:: c++

   using AttributeType = uint32_t;

   #define PW_SENSOR_ATTRIBUTE_TYPE(domain, name_str)                          \
       PW_TOKENIZE_STRING_DOMAIN(domain, name_str)

Similar to the ``MeasurementType``, Pigweed will define a few common attribute
types:

.. code:: c++

   constexpr AttributeType kOffset =
       PW_SENSOR_ATTRIBUTE_TYPE("PW_SENSOR_ATTRIBUTE_TYPE", "offset");
   constexpr AttributeType kFullScale =
       PW_SENSOR_ATTRIBUTE_TYPE("PW_SENSOR_ATTRIBUTE_TYPE", "full scale");
   constexpr AttributeType kSampleRate =
       PW_SENSOR_ATTRIBUTE_TYPE("PW_SENSOR_ATTRIBUTE_TYPE", "sample rate");

Attributes
----------
A single ``Attribute`` representation is the combination of 3 fields:
measurement type, attribute type, and value.

.. code:: c++

   class Attribute : public pw::IntrusiveList<Attribute>::Item {
    public:
     Attribute(MeasurementType measurement_type, AttributeType attribute_type)
         : measurement_type(measurement_type), attribute_type(attribute_type) {}

     bool operator==(const Attribute& rhs) const {
       return measurement_type == rhs.measurement_type &&
              attribute_type == rhs.attribute_type &&
              memcmp(data, rhs.data, sizeof(data)) == 0;
     }

     Attribute& operator=(const Attribute& rhs) {
       PW_DASSERT(measurement_type == rhs.measurement_type);
       PW_DASSERT(attribute_type == rhs.attribute_type);
       memcpy(data, rhs.data, sizeof(data));
       return *this;
     }

     template <typename T>
     void SetValue(typename std::enable_if<std::is_integral_v<T> ||
                                               std::is_floating_point_v<T>,
                                           T>::type value) {
       memcpy(data, value, sizeof(T));
     }

     template <typename T>
     typename std::enable_if<std::is_integral_v<T> ||
                                               std::is_floating_point_v<T>,
                                           T>::type GetValue() {
       return *static_cast<T*>(data);
     }

     MeasurementType measurement_type;
     AttributeType attribute_type;

    private:
     std::byte data[sizeof(long double)];
   };

Configuration
-------------
A configuration is simply a list of attributes. Developers will have 2 options
for accessing and manipulating configurations. The first is to create the
sensor's desired configuration and pass it to ``Sensor::SetConfiguration()``.
The driver will return a ``Future`` using the async API and will attempt to set
the desired configuration. The second option is to first query the sensor's
attribute values, then manipulate them, and finally set the new values using the
same ``Sensor::SetConfiguration()`` function.

.. code:: c++

   using Configuration = pw::alloc::Vector<Attribute>;

   /// @brief A pollable future that returns a configuration
   /// This future is used by the Configurable::GetConfiguration function. On
   /// success, the content of Result will include the current values of the
   /// requester Attribute objects.
   class ConfigurationFuture {
    public:
     pw::async::Poll<pw::Result<Configuration*>> Poll(pw::async::Waker& waker);
   };

   class Configurable {
    public:
     /// @brief Get the current values of a configuration
     /// The @p configuration will dictate both the measurement and attribute
     /// types which are to be queried. The function will return a future and
     /// begin performing any required bus transactions. Once complete, the
     /// future will resolve and contain a pointer to the original Configuration
     /// that was passed into the function, but the values will have been set.
     virtual ConfigurationFuture GetConfiguration(
         Configuration& configuration) = 0;

     /// @brief Set the values in the provided Configuration
     /// The driver will attempt to set each attribute in @p configuration. By
     /// default, if an attribute isn't supported or the exact value can't be
     /// used, the driver will make a best effort by skipping the attribute in
     /// the case that it's not supported or rounding it to the closest
     /// reasonable value. On success, the function should mutate the attributes
     /// to the actual values that were set.
     /// For example:
     ///   Lets assume the driver supports a sample rate of either 12.5Hz or
     ///   25Hz, but the caller used 20Hz. Assuming that @p allow_best_effort
     ///   was set to `true`, the driver is expected to set the sample rate to
     ///   25Hz and update the attribute value from 20Hz to 25Hz.
     virtual ConfigurationFuture SetConfiguration(
         Configuration& configuration, bool allow_best_effort = true) = 0;
   };

Memory management
-----------------
In the ``Configurable`` interface we expose 2 functions which allow getting and
setting the configuration via the Pigweed async API. In both cases, the caller
owns the memory of the configuration. It is the caller that is required to
allocate the space of the attributes which they'd like to query or mutate and it
is the caller's responsibility to make sure that those attributes (via the
``Configuration``) do not go out of scope. The future, will not own the
configuration once the call is made, but will hold a pointer to it. This means
that the address must also be stable. If the future goes out of scope, then the
request is assumed canceled, but the memory for the configuration is not
released since the future does not own the memory.

While it's possible to optimize this path a bit further, sensors are generally
not re-configured often. The majority of sensors force some down time and the
loss of some samples while being re-configured. This makes the storage and
mutation of a ``Configuration`` less critical. It would be possible to leverage
a ``FlatMap`` for the ``Configuration`` in order to improve the lookup time.
The biggest drawback to this approach is the lack of dynamic attribute support.
If we want to allow pluggable sensors where attributes are discovered at
runtime, we would not be able to leverage the ``FlatMap``.

Alternatively, if a ``Configuration``'s keys are known at compile time, we
could support the following cases:

* When a ``Sensor`` knows which attributes it supports at compile time, we
  should be able to allocate an appropriate ``FlatMap``. When the developer
  requests the full configuration, we would copy that ``FlatMap`` out and allow
  the consumer to mutate the copy.
* A consumer which only cares about a subset of statically known attributes, can
  allocate their own ``FlatMap`` backed ``Configuration``. It would pass a
  reference to this object when querying the ``Sensor`` and have the values
  copied out into the owned ``Configuration``.

--------------------
Sensor vs. Framework
--------------------
When complete, both the ``Sensor`` and the ``Connection`` [1]_ objects will
inherit from the ``Configurable`` interface. The main differences are that in
the case of the ``Sensor``, the configuration is assumed to be applied directly
to the driver, while in the case of the ``Connection``, the sensor framework
will need to take into account the configurations of other ``Connection``
objects pointing to the same ``Sensor``.

.. [1] A connection is allocated by the sensor framework to the client and
   allows clients to request configuration changes.
