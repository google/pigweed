.. _seed-0002:

===================
0002: SEED Template
===================

.. card::
   :fas:`seedling` SEED-0002: :ref:`SEED Template<seed-0002>`

   :octicon:`comment-discussion` Status:
   :bdg-primary:`Open for Comments`
   :octicon:`chevron-right`
   :bdg-secondary-line:`Last Call`
   :octicon:`chevron-right`
   :bdg-secondary-line:`Accepted`
   :octicon:`kebab-horizontal`
   :bdg-secondary-line:`Rejected`

   :octicon:`calendar` Proposal Date: 2022-11-30

   :octicon:`code-review` CL: `pwrev/123090 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/123090>`_

-------
Summary
-------
Write a brief paragraph outlining your proposal. If the SEED supersedes another,
mention it here.

----------
Motivation
----------
Describe the purpose of this proposal. What problem is it trying to solve?
Include any relevant background information.

--------
Proposal
--------
In this section, provide a detailed treatment of your proposal, and the
user-facing impacts if the proposal were to be accepted. Treat it as a
reference for the new feature, describing the changes to a user without
background context on the proposal, with explanations of key concepts and
examples.

For example, if the proposal adds a new library, this section should describe
its public API, and be written in the style of API documentation.

---------------------
Problem investigation
---------------------
This section contains detailed research into the problem at hand. Crucially, the
focus here is on the problem itself --- *not* the proposed solution. The purpose
is to convince stakeholders that the proposal is addressing the right issues:
there needs to be agreement on the *why* before they can think about the *how*.

Generally, this section should be the primary focus of the proposal.

Considerations which should be made in the investigation include:

- How does this problem currently impact stakeholders?

- What kinds of use cases, both specific and general, are being addressed? What
  are their priorities? What use cases are *not* being considered, and why?

- Examination of any "prior art". Are there existing solutions to this problem
  elsewhere? What attempts have previously been made, and how do they compare to
  this proposal? What are the benefits and drawbacks of each?

- Is there a workaround for this problem currently in use? If so, how does it
  work, and what are its drawbacks?

- Are there existing Pigweed decisions or policies which are relevant to this
  issue, and can anything be learned from them?

- If the problem involves data not under Pigweed's control, consider the
  security and privacy implications.

The depth of the problem investigation should increase proportionally to the
scope and impact of the problem.

---------------
Detailed design
---------------
If the proposal includes code changes, the detailed design section should
explain the implementation at a technical level. It should be sufficiently
detailed for a developer to understand how to implement the proposal.

Any of the following should be discussed in the design, if applicable:

- API design.
- Internal data storage / layout.
- Interactions with existing systems.
- Similar existing Pigweed APIs: consider framework-wide consistency.
- Testing strategies.
- Performance.
- Code / memory size requirements.
- If an existing implementation is being changed, will there be backwards
  compatibility?

This section may also choose to link to a prototype implementation if one is
written.

------------
Alternatives
------------
Describe what alternative solutions to the problem were considered, referencing
the problem investigation as necessary.

Additionally, consider the outcome if the proposal were not to be accepted.

--------------
Open questions
--------------
Are there any areas of the problem that are not addressed by this proposal, and
is there a plan to resolve them? Does this proposal have implications beyond its
described scope?

If there are anticipated to be additional SEEDs to address outstanding issues,
reference them here, linking to them if they already exist.
