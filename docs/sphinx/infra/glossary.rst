.. _docs-infra-glossary:

========
Glossary
========
This glossary defines terms that have specific meanings in the context of
Pigweed's infrastructure.

.. _docs-infra-glossary-bot:

---
Bot
---
A machine that can run a build.

.. _docs-infra-glossary-bucket:

------
Bucket
------
A group of builders that share ACLs.

.. _docs-infra-glossary-build:

-----
Build
-----
A job. Belongs to a builder.  (Think "process" in "program vs process".)

.. _docs-infra-glossary-builder:

-------
Builder
-------
A named build configuration. (Think "program" in "program vs process".)

.. _docs-infra-glossary-ci:

--
CI
--
Nominally "continuous integration", but usually used to refer to post-submit
builders and associated infrastructure.

.. _docs-infra-glossary-cq:

--
CQ
--
Nominally "Commit-Queue", a service that has been replaced with "Change
Verifier", a service that manages pre-submit builders.

.. _docs-infra-glossary-properties:

----------
Properties
----------
Arbitrary input/output JSON object to/from a build. Input properties are
typically specified in the builder config and output properties are
specified by the recipe at runtime.

.. _docs-infra-glossary-recipe:

------
Recipe
------
A Python-based script executed by a build.
