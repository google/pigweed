# Work Order: Test Enhancement Audit

This work order provides a structured process for auditing and improving the
test coverage for a specific area of functionality within a Pigweed module. It
focuses on a deep analysis of the existing code and tests to create a targeted
plan for enhancement.

## Scaffolding Documents

- `<module>/test_audit_design.md`: A document to analyze the current test
  coverage and design the new tests.
- `<module>/tasks.md`: A task list to track the implementation of the new tests.

---

### `test_audit_design.md` Skeleton

```markdown
# Test Enhancement Audit: [Module Name] - [Feature Area]

## 1. Background

- **1.1. Motivation:** Why is this audit being performed? (e.g., general
  hardening, recent bug, proactive quality improvement). Link to a bug report if
  applicable (e.g., `Bug: b/123456789`).
- **1.2. Scope:** A brief description of the feature area being audited.

## 2. Scoping & Sizing

- **2.1. Estimated Size:** A rough estimate of the effort required (e.g., S, M,
  L, XL, or person-days).
- **2.2. Key Risks:** What are the biggest risks or unknowns in this audit?

## 3. Current Functionality Deep Dive

_This section is critical for building a shared understanding of the existing
system. It must be completed before analyzing the tests._

- **3.1. System Overview:** A detailed, narrative description of how the
  relevant parts of the system work today. This should be based on a thorough
  reading of the code and not on assumptions.
- **3.2. Key Code Components:**
  - A list of the key classes, functions, and data structures.
  - For each component, a brief description of its role and key parameters.
- **3.3. Files Read:** A list of all files that were read to build this
  understanding.

## 4. Test Coverage Analysis

_With a deep understanding of the code, now analyze the existing tests._

- **4.1. Existing Tests:**
  - A list of the test files and specific test cases that cover this
    functionality.
- **4.2. Coverage Gaps:**
  - A narrative description of the current test coverage, based on a thorough
    review of the code and tests.
  - **Testing Levels:** An analysis of unit, integration, and language-specific
    (e.g., C++ vs. Python) test coverage.
  - What scenarios are well-tested?
  - What are the gaps? (e.g., edge cases, error conditions)

## 5. Proposed New Tests

- **5.1. Test Plan:** A high-level plan for the new tests to be added to fill
  the identified gaps.
- **5.2. Detailed Test Cases:** A list of the new test cases to be written,
  including:
  - A description of the scenario being tested.
  - The expected outcome.

## 6. Blind Alleys

- A section to document any implementation attempts that were abandoned. For
  each blind alley, describe the approach, why it failed, and what was learned.

## 7. Testing Strategy

- How will the new tests be implemented? (e.g., new test file, adding to
  existing tests)
```

---

### `tasks.md` Skeleton

```markdown
# Tasks: Test Enhancement Audit - [Module Name] - [Feature Area]

**Work order:** test_enhancement_audit: [Module Name] - [Feature Area]

- [ ] **Setup:** Scaffolding documents created.
- [ ] **Planning:**
  - [ ] `test_audit_design.md` complete.
    - [ ] Scoping & Sizing complete.
    - [ ] Current Functionality Deep Dive complete.
    - [ ] Test Coverage Analysis complete.
    - [ ] Proposed New Tests complete.
  - [ ] `tasks.md` complete.
- [ ] **Critic:**
  - [ ] Stage 1: Initial Review complete.
  - [ ] Stage 2: Deep Expert Review complete.
- [ ] **Implementation:**
  - [ ] Test Case 1...
  - [ ] Test Case 2...
- [ ] **Verification:**
  - [ ] Code review complete.
  - [ ] All tests passing.
- [ ] **Work order review (Optional):**
  - [ ] Stage 1: Initial Feedback complete.
  - [ ] Stage 2: Deep Analysis complete.
- [ ] **Integration & Cleanup:**
  - [ ] Final documentation review complete.
  - [ ] Scaffolding files deleted.
  - [ ] Final commit ready.
```

---

## Phases

### Setup Phase

1.  Determine the target module (e.g., `pw_string`) and the specific feature
    area to audit.
2.  Create `test_audit_design.md` and `tasks.md` in the module directory using
    the skeletons above.

### Planning Phase

_This phase is iterative. The "Deep Dive" and "Test Coverage Analysis" sections
of the design document should be developed in tandem, as understanding of the
existing system informs the test analysis, and the test analysis may prompt
further investigation into the code._

1.  **Crucially, complete the "Current Functionality Deep Dive" section of
    `test_audit_design.md` before proceeding.** This requires a thorough review
    of the implementation.
2.  With a deep understanding of the code, analyze the existing tests and
    identify gaps.
3.  Iterate on the "Proposed New Tests" section until the user is satisfied with
    the plan.
4.  Create a detailed task list in `tasks.md` for implementing the new tests.
5.  Flush the context to the scaffolding files.

### Critic Phase

1.  **Stage 1: Initial Review:** Adopt the persona of a skeptical, expert
    principal software engineer. Thoroughly review the `test_audit_design.md`
    and all referenced code. Poke holes in the plan. Look for flawed
    assumptions, missing edge cases, and potential negative interactions with
    other parts of the codebase.
2.  **Stage 2: Deep Expert Review:** Based on the findings from the initial
    review, conduct a second, more intensive critique. Challenge every
    assumption.
3.  Document all findings and discuss them with the user before proceeding.
4.  Flush the context to the scaffolding files.

### Implementation Phase

1.  Identify the smallest verifiable test case to add from the list.
2.  As you implement, your understanding of the code and tests will evolve.
    **Update the "Deep Dive" and "Test Coverage Analysis" sections of
    `test_audit_design.md` to reflect new insights.**
3.  Implement the test case, ensuring it passes and that existing tests continue
    to pass.
4.  Mark the task as complete in `tasks.md`.

### Verification Phase

1.  Review the `test_audit_design.md` and the implemented tests to ensure they
    are aligned.
2.  Ensure that all new and existing tests pass.
3.  Confirm with the user that the test coverage is now sufficient for the
    audited area.

### Integration & Cleanup Phase

1.  Ensure any relevant external documentation (e.g., module READMEs) is updated
    to reflect the improved testing.
2.  Delete the scaffolding files (`test_audit_design.md`, `tasks.md`).
3.  Prepare the final commit.
