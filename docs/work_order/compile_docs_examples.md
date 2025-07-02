# Work Order: Compile Docs Examples

This work order converts inline documentation code examples into standalone,
buildable, and testable files. It provides a high-level project management
framework for the process that is detailed in
`docs/contributing/docs/examples.rst`.

## Scaffolding

- `<module>/design.md`: To plan the conversion, listing the source `.rst` files,
  the examples to convert, and the new files and build targets to be created.
- `<module>/tasks.md`: To track the status of each example conversion.

---

### `design.md` Skeleton

```markdown
# Compile Docs Examples: [Module Name]

## 1. Background

- **1.1. Goal:** Convert inline code examples in the documentation for the
  `[Module Name]` module into buildable and testable examples.
- **1.2. Source Documentation:**
  - `docs.rst`
  - (List other relevant `.rst` files here)

## 2. Plan

- **Source File:** `docs.rst`
  - **Example:** Example of X feature
  - **Compilable:** Yes
  - **Action:** Convert to new file `x_feature_test.cc`
- **Source File:** `docs.rst`
  - **Example:** Pseudo-code for Y concept
  - **Compilable:** No
  - **Action:** Leave as-is in a `code-block`

```

---

### `tasks.md` Skeleton

```markdown
# Tasks: Compile Docs Examples - [Module Name]

**Work order:** compile_docs_examples: [Module Name]

- [ ] **Setup:** Scaffolding documents created.
- [ ] **Planning:**
  - [ ] `design.md` complete.
  - [ ] `tasks.md` complete.
- [ ] **Critic:**
  - [ ] Stage 1: Initial Review complete.
  - [ ] Stage 2: Deep Expert Review complete.
- [ ] **Implementation:**
  - [ ] `x_feature_test.cc` created and compiling.
  - [ ] `docs.rst` updated with `literalinclude` for X feature.
- [ ] **Verification:**
  - [ ] `//docs` build successful.
  - [ ] HTML output for `[Module Name]` verified.
- [ ] **Work order review (Optional):**
  - [ ] Stage 1: Initial Feedback complete.
  - [ ] Stage 2: Deep Analysis complete.
- [ ] **Integration & Cleanup:**
  - [ ] Scaffolding files deleted.
  - [ ] Final commit ready.
```

---

## Context Files

During this work order, the following file must be loaded into the context, as
it is the primary source of truth for the implementation steps:

- `docs/contributing/docs/examples.rst`

## Phases

- **Setup:**
  1.  Determine the target module (e.g., `pw_string`).
  2.  Create the `<module>/design.md` and `<module>/tasks.md` files.
  3.  Read the module's documentation (`.rst` files) to find inline code
      examples (`.. code-block:: cpp`).
  4.  Populate `design.md` with a list of the documentation files and the
      specific examples to be converted.
  5.  Populate `tasks.md` with a checklist for each identified example.

- **Planning:**
  1.  Review the proposed conversion plan in `design.md` with the user.
  2.  Consult the "Workflow" section of `docs/contributing/docs/examples.rst`
      to decide which examples should be made buildable. Confirm with the user.
  3.  Get user approval on the plan before starting implementation.

- **Critic:**
  1.  Review the plan in `design.md`.
  2.  Ensure the plan is consistent with the "Workflow" section in
      `docs/contributing/docs/examples.rst`.
  3.  Verify that proposed file locations, naming, and `DOCSTAGS` are consistent
      with Pigweed conventions.

- **Implementation:**
  1.  Following the iterative process described in the "Workflow"
      section of `docs/contributing/docs/examples.rst`, convert the examples.
  2.  Update `tasks.md` as each example is successfully converted and verified.

- **Verification:**
  1.  Follow the verification steps outlined in the "Workflow",
      including building the docs and visually inspecting the HTML output.
  2.  Ensure all tasks in `tasks.md` are complete.

- **Work order review (Optional):**
  1.  After all examples are converted, reflect on the process.
  2.  Discuss with the user if any improvements can be made to this work order
      template for future use.

- **Completion:**
  1.  After all examples are converted and verified, delete the `design.md` and
      `tasks.md` scaffolding files.
  2.  Prepare the final commit.
