.. _module-pw_json:

=======
pw_json
=======
.. pigweed-module::
   :name: pw_json

   Use :cc:`pw::JsonBuilder` to serialize JSON to a buffer. It's simple,
   safe, and efficient.

   .. literalinclude:: builder_test.cc
      :language: cpp
      :start-after: [pw-json-builder-example-1]
      :end-before: [pw-json-builder-example-1]

   The above code produces JSON equivalent to the following:

   .. code-block:: json

      {
        "tagline": "Easy, efficient JSON serialization!",
        "simple": true,
        "safe": 100,
        "dynamic allocation": false,
        "features": ["values", "arrays", "objects", "nesting!"]
      }

-----------
JsonBuilder
-----------
:cc:`pw::JsonBuilder` is used to create arbitrary JSON. It contains a
JSON value, which may be an object or array. Arrays and objects may contain
other values, objects, or arrays.

**Example**

.. literalinclude:: builder_test.cc
   :language: cpp
   :start-after: [pw-json-builder-example-2]
   :end-before: [pw-json-builder-example-2]

API Reference
=============
Moved: :cc:`pw_json`
