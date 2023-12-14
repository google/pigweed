.. _seed-0001:

======================
0001: The SEED Process
======================
.. seed::
   :number: 1
   :name: The SEED Process
   :status: Accepted
   :proposal_date: 2022-10-31
   :cl: 116577
   :facilitator: Unassigned

-------
Summary
-------
SEEDs are the process through which substantial changes to Pigweed are proposed,
reviewed, and decided by community stakeholders to collaboratively drive the
project in a favorable direction.

This document outlines the SEED process at a high level. Details about how SEEDs
should be written and structured are described in :ref:`seed-0002`.

----------
Motivation
----------
As Pigweed and its community grow, it becomes important to ensure that the
Pigweed team, Pigweed users, and other community stakeholders align on the
future of the project. To date, development of Pigweed has primarily been
driven by the core team, and community feedback has been mostly informal and
undocumented.

SEEDs are a formalized process for authoring, reviewing, and ratifying proposed
changes to Pigweed.

The SEED process has several goals.

- Increase the visibility of proposed changes to Pigweed early on and allow
  interested parties to engage in their design.

- Maintain a public record of design decisions rendered during Pigweed's
  development and the rationales behind them.

- Ensure consistent API design across Pigweed modules through a formal team-wide
  review process.

Active SEEDs are discussed by the community through Gerrit code review comments
as well as a SEEDs chatroom in Pigweed's Discord server. Decisions are
ultimately rendered by a review committee of Pigweed team members.

------------------------
When is a SEED required?
------------------------
SEEDs should be written by any developer wishing to make a "substantial" change
to Pigweed. Whether or not a change is considered "substantial" varies depending
on what parts of Pigweed it touches and may change over time as the project
evolves. Some general guidelines are established below.

Examples of changes considered "substantial" include, but are not limited to:

- Adding a new top-level module.

- Modifying a widely-used public API.

- A breaking update to typical Pigweed user workflows (bootstrap, build setup,
  ``pw`` commands, etc.).

- Changing any data interchange or storage protocol or format (e.g. transport
  protocols, flash layout), unless the change is small and backwards compatible
  (e.g. adding a field to an exchanged Protobuf message).

- Changes to Pigweed's code policy, style guides, or other project-level
  guidelines.

- Whenever a Pigweed team member asks you to write a SEED.

Conversely, the following changes would likely not require a SEED:

- Fixing typos.

- Refactoring internal module code without public API changes.

- Adding minor parallel operations to existing APIs (e.g.
  ``Read(ConstByteSpan)`` vs ``Read(const byte*, size_t)``).

If you're unsure whether a change you wish to make requires a SEED, it's worth
asking the Pigweed team in our Discord server prior to writing any code.

-------
Process
-------
Suppose you'd like to propose a new Pigweed RPC Over Smoke Signals protocol.

#. If you haven't already, clone the Pigweed repository and set it up locally,
   following the :ref:`docs-get-started-upstream` guide.

#. Claim a number for your SEED. This should be the next sequential number
   listed within the ``pw_seed_index`` in ``seed/BUILD.gn``. (We will use 5309
   for our example.)

   .. _SEED index: https://cs.opensource.google/pigweed/pigweed/+/main:seed/0000-index.rst

#. Create a new RST file for your SEED. Name the file with the number of your
   SEED followed by the lowercase title of the proposal, with words separated by
   hyphens.

   .. code-block:: sh

      touch seed/5309-pw_rpc-over-smoke-signals.rst

#. Commit your RST document and push it up to Gerrit, marking it as a
   Work-In-Progress change, following the :ref:`docs-contributing` guide.

   Your change will be assigned a number, which can be found in the Gerrit UI
   and in its URL. For example, if your change is located at
   ``http://pigweed-review.googlesource.com/c/pigweed/pigweed/+/987654``, its CL
   number is ``987654``.

#. Add a SEED entry to the GN build pointing to your CL by modifying
   ``seed/BUILD.gn``.

   .. code-block::

      # Define a target for your SEED.
      pw_seed("5309") {
        changelist = 987654
        title = "pw_rpc Over Smoke Signals"
        status = "Draft"
        author = "Your Name"
      }

      # Insert your dependency to the doc group at the bottom of the file.
      pw_seed_index("seeds") {
        seeds = [
          ":0001",
          ...
          ":5308",
          ":5309",
        ]
      }

#. Push a separate change to Gerrit with your SEED's GN build additions and add
   GWSQ as a reviewer. Set ``Pigweed-Auto-Submit`` to +1.

   .. image:: 0001-the-seed-process/seed-index-gerrit.png

#. Fill out your proposal document, using the :ref:`SEED template<seed-0002>` as
   a guide.

   If your SEED requires additional resources such as images, place them within
   a subdirectory named identically to your document without the ``.rst``
   extension. These should be listed as ``inputs`` in your SEED's GN doc group
   target.

   .. code-block::

      seed/
        ...
        5309-pw_rpc-over-smoke-signals.rst
        5309-pw_rpc-over-smoke-signals/
          state-diagram.svg

#. When you feel you have enough substantive content in your proposal to be
   reviewed, push it up to Gerrit and switch the change from WIP to Active.
   This will begin the open comments period.

   Congrats! You are now a SEED author.

#. The Pigweed team will now assign your SEED a SEED facilitator. The
   facilitator will leave a comment on your SEED asking you to add their name
   to the ``facilitator:`` entry in the header of your SEED.

   The SEED facilitator is a member of the Pigweed team who will help move your
   through the process. The SEED facilitator will be added as a reviewer on
   your SEED and will be your primary point of contact on the Pigweed team.

#. Create a thread for your SEED in the ``#seed`` channel of Pigweed's
   `Discord server <https://discord.gg/M9NSeTA>`_.

#. Engage with reviewers to iterate on your proposal through its comment period.

#. During the comment period, the facilitator may comment that your proposal has
   received "Approval of Intent" and request in the SEED comments for interested
   reviewers to identify themselves.

   The SEED status should be changed to ``Intent Approved``.

   At this point, initial implementation of the feature may begin landing in
   Pigweed upstream. Any CLs prior to the SEED landing should CC both the
   facilitator and other commenters who've indictated their interest in
   reviewing.

   All code landed during this period should be marked as experimental and
   protected by visibility limitations.

#. When a tentative decision has been reached, the facilitator will comment on
   your proposal with a summary of the discussion and reasoning, moving it into
   its Last Call phase (as described in the
   :ref:`Lifecycle <seed-0001-lifecycle>` section).

#. Following the conclusion of the Last Call period (one week from the start of
   Last Call), the facilitator will sign off on the CL with a +2 vote, allowing
   it to be submitted. Once a +2 has been given, the SEED author should update
   the SEED index and submit the CL.

   Before submitting, update your SEED's GN target to point to the local RST
   file and to reflect its final status.

   .. code-block::

      pw_seed("5309") {
        sources = [ "5309-pw_rpc-over-smoke-signals.rst" ]
        title = "pw_rpc Over Smoke Signals"
        status = "Accepted"
        author = "Your Name"
      }

---------------------------------------
The relationship between SEEDs and code
---------------------------------------
Some common questions raised by participants in the SEED process revolve around
how SEED proposals relate to implemented code. This section addresses several of
those questions.

When should implementation of a SEED proposal begin?
====================================================
.. admonition:: TL;DR

  The SEED's author can start writing code as soon as the intent of the proposal
  is approved.

Generally speaking, there are two stages of approval for the majority of SEED
proposals. The first is approval of the *intent* of the SEED --- that is,
stakeholders agree that it represents a problem that Pigweed should address,
and the general outline of the solution is reasonable.

Following this comes the approval of the specific details of the proposed
solution. Depending on the nature of the SEED, this could range
from higher-level component hierarchies and interactions down to concrete API
design and precise implementation details.

Once the intent of a SEED is approved, authors are free to begin implementing
code for their proposal if they wish. This can serve as an additional reference
for reviewers to aid their understanding of the proposal, and allow both the
proposal and implementation to co-evolve throughout the review process.

Code written alongside an active SEED can be reviewed and even merged into
Pigweed, hidden behind experimental feature flags.

At what point is the code related to a SEED considered usable?
==============================================================
.. admonition:: TL;DR

  Code written for a SEED is considered experimental and unstable until the SEED
  is fully approved.

It is possible for code to be written, reviewed, and committed to Pigweed while
its SEED is still in the review process. As these changes end up in Pigweed's
main, it naturally raises the question of whether or not it is usable by other
modules, or even external projects.

Any code which is approved and submitted while its SEED remains active will be
treated as experimental and hidden behind a feature flag. These flags will be
configurable by other modules and downstream projects, allowing dependencies on
experimental code. All experimental features are unstable and subject to
potentially large changes at any time, so depending on them in non-experimental
contexts is strongly discouraged.

There may be rare circumstances where particularly time-sensitive code is
required by projects with whom Pigweed works in close collaboration before a
full SEED approval cycle can be completed. In these instances, the project may
begin to depend on experimental code prematurely, and Pigweed will assist them
with keeping up-to-date as it evolves. This type of usage is limited to only
exceptional circumstances. In almost all cases, experimental code should be used
at a project's own risk.

Will approved SEEDs be updated in response to code changes?
===========================================================
.. admonition:: TL;DR

  Approved SEEDs will not be updated as code evolves. Use module documentation
  as a current reference.

SEED documents are intended to capture decisions made at a point in time with
their justification. They are not living documents which reflect the current
state of the codebase. Generally speaking, SEEDs will not be updated following
their acceptance and will likely diverge from the actual code as time passes.
Some SEEDs may even become entirely obsolete if the team revisited the issue and
decided to move in a different direction, becoming purely a historical record of
design decisions.

There are exceptions when a SEED may be modified after it has been approved;
typically, these will occur shortly after the approval if its implementer finds
that an important detail was incorrect or missing.

If a SEED/s content is obsolete or outdated, it should ideally be marked as
such by adding a notice or warning to the top of the SEED. However, these
indications are marked on a best-effort basis, so SEEDs should not be be used as
the primary source of documentation for a Pigweed feature.

Users should instead rely on module documentation for up-to-date
information about the state of a Pigweed module or feature. SEEDs can be used as
an additional resource to learn *why* something was designed the way that it is,
but is never necessary to understand functionality or usage.

--------------
SEED documents
--------------
SEEDs are written as ReST documents integrated with the rest of Pigweed's
documentation. They live directly within the core Pigweed repository, under a
top-level ``seed/`` subdirectory.

The structure of SEED documents themselves, their format, required sections, and
other considerations are outlined in :ref:`seed-0002`.

The first 100 SEEDs (0000-0100) are *Meta-SEEDs*. These are reserved for
internal Pigweed usage and generally detail SEED-related processes. Unlike
regular SEEDs, Meta-SEEDs are living documents which may be revised over time.

.. _seed-0001-lifecycle:

-----------------------
The lifecycle of a SEED
-----------------------
A SEED proposal undergoes several phases between first being published and a
final decision.

:bdg-primary-line:`Draft` **The SEED is a work-in-progress and not yet ready
for comments.**

- The SEED exists in Gerrit as a Work-In-Progress (WIP) change.
- Has an assigned SEED number and exists in the index.
- Not yet ready to receive feedback.

:bdg-primary:`Open for Comments` **The SEED is soliciting feedback.**

- The SEED has sufficient substance to be reviewed, as determined by its
  author.
- A thread for the SEED is created in Discord to promote the proposal and open
  discussion.
- Interested parties comment on the SEED to evaluate the proposal, raise
  questions and concerns, and express support or opposition.
- Back and forth discussion between the author and reviewers, resulting in
  modifications to the document.
- The SEED remains open for as long as necessary. Internally, Pigweed's review
  committee will regularly meet to consider active SEEDs and determine when to
  advance to them the next stage.
- Open SEEDs are assigned facilitators in the core Pigweed team, who are
  primarily responsible for engaging with the author to move the SEED through
  its review process.

:bdg-warning:`Last Call` **A tentative decision has been reached, but
commenters may raise final objections.**

- A tentative decision on the SEED has been made. The decision is issued at the
  best judgement of the SEED's facilitator when they feel there has been
  sufficient discussion on the tradeoffs of the proposal to do so.
- Transition is triggered manually by its facilitator, with a comment on the
  likely outcome of the SEED (acceptance / rejection).
- On entering Last Call, the visibility of the SEED is widely boosted through
  Pigweed's communication channels (Discord, mailing list, Pigweed Live, etc.)
  to solicit any strong objections from stakeholders.
- Typically, Last Call lasts for a set period of 7 calendar days, after which
  the final decision is formalized.
- If any substantial new arguments are raised during Last Call, the review
  committee may decide to re-open the discussion, returning the SEED to a
  commenting phase.

:bdg-success:`Accepted` **The proposal is ratified and ready for
implementation.**

- The SEED is submitted into the Pigweed repository.
- A tracking bug is created for the implementation, if applicable.
- The SEED may no longer be modified (except minor changes such as typos).
  Follow-up discussions on the same topic require a new SEED.

:bdg-danger:`Rejected` **The proposal has been turned down.**

- The SEED is submitted into the Pigweed repository to provide a permanent
  record of the considerations made for future reference.
- The SEED may no longer be modified.

:bdg-secondary:`Deprecated` **The proposal was originally accepted and
implemented but later removed.**

- The proposal was once implemented but later undone.
- The SEED's changelog contains justification for the deprecation.

:bdg-info:`Superseded` **The proposal was originally accepted and implemented
but significant portions were later overruled by a different SEED.**

- A newer SEED proposal revisits the same topic and proposal and redesigns
  significant parts of the original.
- The SEED is marked as superseded with a reference to the newer proposal.

---------
Rationale
---------

Document format
===============
Three different documentation formats are considered for SEEDs:

- **ReST:** Used for Pigweed's existing documentation, making it a natural
  option.
- **Google Docs:** The traditional way of writing SEED-like investigation and
  design documents.
- **Markdown:** Ubiquitous across open-source projects, with extensive tooling
  available.

Summary
-------
Based on the evaluated criteria, ReST documents provide the best overall SEED
experience. The primary issues with ReST exist around contributor tooling, which
may be mitigated with additional investment from the Pigweed team.

The table below details the main criteria evaluated for each format, with more
detailed explanations following.

.. list-table::
   :widths: 55 15 15 15
   :header-rows: 1

   * - Criterion
     - ReST
     - Markdown
     - Google Docs
   * - Straightforward integration with existing docs
     - ✅
     - ❌
     - ❌
   * - Indexable on `pigweed.dev <https://pigweed.dev>`_
     - ✅
     - ✅
     - ❌
   * - Auditable through source control
     - ✅
     - ✅
     - ❌
   * - Archive of review comments and changes
     - ✅
     - ✅
     - ❌
   * - Accessible to contributors
     - ❌
     - ✅
     - ✅
   * - Extensive styling and formatting options
     - ✅
     - ❌
     - ✅
   * - Easy sharing between Google and external contributors
     - ✅
     - ✅
     - ❌

Integration
-----------
.. admonition:: Goal

   SEED documents should seamlessly integrate with the rest of Pigweed's docs.

As all of Pigweed's documentation is written using ReST, it becomes a natural
choice for SEEDs. The use of other formats requires additional scaffolding and
may not provide as seamless of an experience.

Indexability
------------
.. admonition:: Goal

   Design decisions in SEEDs should be readily available for Pigweed users.

`pigweed.dev <https://pigweed.dev>`_ has a search function allowing users to
search the site for Pigweed-related keywords. As SEEDs contain design discussion
and rationales, having them appear in these searches offers useful information
to users.

The search function is provided by Pigweed's Sphinx build, so only documents
which exist as part of that (ReST / Markdown) are indexed.

Auditability
------------
.. admonition:: Goal

   Changes to SEED documents should be reviewed and recorded.

ReST and Markdown documents exist directly within Pigweed's source repository
after being submitted, requiring any further changes to go through a code
review process.

Conversely, Google Docs may be edited by anyone with access, making them prone
to unintentional modification.

Archive of discussions
----------------------
.. admonition:: Goal

   Discussions during the review of a SEED should be well-archived for
   future reference.

ReST and Markdown documentation are submitted through Gerrit and follow the
standard code review process. Review comments on the changes are saved in
Gerrit and are easily revisited. Incremental updates to the SEED during the
review process are saved as patch sets.

Comments in Google Docs are more difficult to find once they are resolved, and
document changes do not exist as clearly-defined snapshots, making the history
of a SEED harder to follow.

Accessibility
-------------
.. admonition:: Goal

   SEEDs should be easy for contributors to write.

Both Markdown and Google Docs are easy to write, familiar to many, and have
extensive tooling available. SEED documents can be written outside of the
Pigweed ecosystem using authors' preferred tools.

ReST, on the other hand, is an unfamiliar and occasionally strange format, and
its usage for SEEDs is heavily tied to Pigweed's documentation build. Authors
are required to set up and constantly re-run this build, slowing iteration.

Format and styling
------------------
.. admonition:: Goal

   SEED authors should have options for formatting various kinds of information
   and data in their proposals.

Markdown intentionally only offers limited control over document formatting,
whereas ReST has a wide selection of directives and Google Docs functions as a
traditional WYSIWYG editor, making them far more flexible.

Sharing between Google and non-Google
-------------------------------------
.. admonition:: Goal

   Both Google and non-Google contributors should easily be able to write and
   review SEEDs.

Due to security and legal concerns, managing ownership of Google Docs between
internal and external contributors is nontrivial.

Text documentation formats like Markdown and ReST live within the Pigweed
repository, and as such follow the standard code contribution process.
