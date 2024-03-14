.. _module-pw_json:

=======
pw_json
=======
.. pigweed-module::
   :name: pw_json

   Use :cpp:class:`pw::JsonBuilder` to serialize JSON to a buffer. It's simple,
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
.. doxygenfile:: pw_json/builder.h
   :sections: detaileddescription

**Example**

.. literalinclude:: builder_test.cc
   :language: cpp
   :start-after: [pw-json-builder-example-2]
   :end-before: [pw-json-builder-example-2]

API Reference
=============
.. doxygengroup:: pw_json_builder_api
   :content-only:
   :members:
