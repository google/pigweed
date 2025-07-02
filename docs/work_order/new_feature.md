# Work Order: New Feature

This work order guides the implementation of a new feature from conception to
completion, ensuring a robust and well-documented result.

## Scaffolding Documents

This work order uses three scaffolding documents to track the process:

- `<module>/requirements.md`: A product requirements document (PRD) that defines
  the feature, its purpose, and its user experience.
- `<module>/design.md`: A technical design document that outlines the
  implementation plan.
- `<module>/tasks.md`: A task list to track progress.

---

### `requirements.md` Skeleton

```markdown
# Feature: [Feature Name]

## 1. Overview

- **1.1. Problem:** What user problem does this feature solve?
- **1.2. Proposal:** A high-level summary of the proposed feature.
- **1.3. Scope:** What is included and what is explicitly excluded?

## 2. User Experience (UX)

- **2.1. User Stories:** Who are the target users and what are their goals?
- **2.2. Interaction Model:** How will users interact with this feature?
- **2.3. UI/API Design:** A description of the user interface or public API.

## 3. Non-Functional Requirements

- **3.1. Performance:** What are the performance targets? How will they be
  measured?
- **3.2. Security:** Have security implications been considered?
- **3.3. Observability:** What metrics, logs, or traces are needed to monitor
  this feature?
- **3.4. Maintainability:** Are there any new dependencies or patterns that
  could affect maintainability?

## 4. Success Metrics

- How will we know if this feature is successful?
```

---

### `design.md` Skeleton

```markdown
# Design: [Feature Name]

## 1. Background

- A brief summary of the feature and its purpose, linking to the
  `requirements.md` file.

## 2. Scoping & Sizing

- **2.1. Estimated Size:** A rough estimate of the effort required (e.g., S, M,
  L, XL, or person-days).
- **2.2. Key Risks:** What are the biggest risks or unknowns in this project?

## 3. Current Functionality Deep Dive

_This section is critical for building a shared understanding of the existing
system. It must be completed before proposing any changes._

- **3.1. System Overview:** A detailed, narrative description of how the
  relevant parts of the system work today. This should be based on a thorough
  reading of the code and not on assumptions.
- **3.2. Key Code Components:**
  - A list of the key classes, functions, and data structures.
  - For each component, a brief description of its role and key parameters.
- **3.3. Current Testing:** A brief overview of the existing tests for this
  functionality. This is not a detailed analysis but a starting point to
  motivate a deeper read of the test files.
- **3.4. Files Read:** A list of all files that were read to build this
  understanding.

## 4. Proposed Changes

- **4.1. High-Level Design:** A description of the overall architecture of the
  proposed changes.
- **4.2. Detailed Design:** A breakdown of the changes to each file, referencing
  the components identified in the deep dive.
- **4.3. Files Added/Modified:** An active list of files that will be added or
  modified as part of this work. This serves as a delta from the "Files Read"
  list and is useful for resuming work.
- **4.4. Alternatives Considered & Blind Alleys:** Other approaches that were
  considered and why they were not chosen. This section is also used to document
  any implementation attempts that were abandoned ("blind alleys"). For each
  blind alley, describe the approach, why it failed, and what was learned.

## 5. Deferred for Later

- A list of ideas or features that were explicitly deferred as part of this
  work, but should be kept in mind for the future.

## 6. Non-Functional Impact Analysis

- How will the proposed changes affect the non-functional requirements
  (performance, security, etc.)?

## 7. Testing Strategy

- How will this feature be tested? (e.g., unit tests, integration tests, manual
  verification)
```

---

### `tasks.md` Skeleton

```markdown
# Tasks: [Feature Name]

**Work order:** new_feature: [Feature Name]

- [ ] **Setup:** Scaffolding documents created.
- [ ] **Planning:**
  - [ ] `requirements.md` complete.
  - [ ] `design.md` complete.
    - [ ] Scoping & Sizing complete.
    - [ ] Current Functionality Deep Dive complete.
    - [ ] Proposed Changes complete.
    - [ ] Testing Strategy complete.
  - [ ] `tasks.md` complete.
- [ ] **Critic:**
  - [ ] Stage 1: Initial Review complete.
  - [ ] Stage 2: Deep Expert Review complete.
- [ ] **Implementation:**
  - [ ] Task 1...
  - [ ] Task 2...
- [ ] **Verification:**
  - [ ] Code review complete.
  - [ ] All tests passing.
- [ ] **Work order review (Optional):**
  - [ ] Stage 1: Initial Feedback complete.
  - [ ] Stage 2: Deep Analysis complete.
- [ ] **Integration & Cleanup:**
  - [ ] Final documentation review complete.
  - [ ] Change communication plan executed.
  - [ ] Scaffolding files deleted.
  - [ ] Final commit ready.
```

---

## Phases

### Setup Phase

1.  Determine the target module (e.g., `pw_string`). Ask the user if unsure.
2.  Create `requirements.md`, `design.md`, and `tasks.md` in the module
    directory using the skeletons above, based on the user's initial prompt.

### Planning Phase

_This phase is iterative. The "Deep Dive" and "Proposed Changes" sections of the
design document should be developed in tandem, as understanding of the existing
system informs the proposal, and the proposal guides further investigation._

1.  Iterate on the requirements (`requirements.md`) until the user is satisfied.
2.  **Crucially, complete the "Current Functionality Deep Dive" section of
    `design.md` before proceeding.** This step is mandatory and requires
    thorough code analysis.
3.  Iterate on the technical design (`design.md`) until the user is satisfied.
4.  Create a detailed task list in `tasks.md` based on the requirements and
    design.

### Critic Phase

1.  Adopt the persona of a skeptical, expert principal software engineer.
2.  Thoroughly review the `design.md` and all referenced code.
3.  Poke holes in the plan. Look for flawed assumptions, missing edge cases,
    maintainability issues, and potential negative interactions with other parts
    of the codebase.
4.  Challenge the design's robustness. Consider how it will hold up to changes
    in the system, environment, or dependencies.
5.  Document all findings and discuss them with the user before proceeding.

### Implementation Phase

1.  Identify the smallest verifiable task from the list.
2.  As you implement, your understanding of the code will evolve. **Update the
    "Current Functionality Deep Dive" and "Proposed Changes" sections of
    `design.md` to reflect new insights.**
3.  Implement the task, ensuring the code compiles and any relevant tests pass.
4.  Mark the task as complete in `tasks.md`.

### Verification Phase

1.  Review the `requirements.md`, `design.md`, and the implemented code to
    ensure they are aligned.
2.  Ensure that relevant automated tests have been added.
3.  If automated tests are not feasible, ask the user to confirm that they have
    manually verified the key functionality.

### Integration & Cleanup Phase

1.  Ensure any relevant external documentation (e.g., module READMEs, API docs)
    is updated.
2.  Communicate the change to the team as needed (e.g., in a team chat or
    release notes).
3.  Delete the scaffolding files (`requirements.md`, `design.md`, `tasks.md`).
4.  Prepare the final commit.
