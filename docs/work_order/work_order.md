# Work Orders: Robust & Repeatable AI Workflows

## Background

To ensure high-quality, sustainable, and well-thought-out contributions, we use
a "Work Order" process. This process is designed to guide AI-assisted
development through a structured workflow, preventing premature code generation
and encouraging a more thorough design and planning phase.

The work order concept addresses several challenges:

- **Promotes Deeper Thinking:** It pushes the AI model to analyze the task more
  deeply before implementation.
- **Enables Collaboration and Persistence:** Work orders, being stored in source
  control, allow for partially completed efforts to be shared, reviewed, and
  resumed later, even across different sessions.
- **Reduces Errors:** By following a structured process, we can avoid common
  pitfalls like non-working code, unsustainable designs, and incomplete
  features.

## What is a Work Order?

A "work order" is a formal process for accomplishing a common development task
repeatably using AI tooling. It follows a micro-waterfall model where the AI and
the user collaborate on a plan, which is documented in source-controlled
markdown files. Once the plan is agreed upon, the AI proceeds with the
implementation.

Each work order is defined by two files in the `docs/work_order` directory:

- `docs/work_order/<work_order_name>.md`: The main workflow definition. This
  file is only loaded into the context when the work order is active.
- `docs/work_order/<work_order_name>_trigger.md`: A concise file containing a
  short description and the trigger conditions for the AI to suggest the work
  order. This file is always included in the context.

### Example Work Order Types

- **[new_feature](new_feature.md):** Add a new feature, from product
  requirements to implementation.
- **[compile_docs_examples](compile_docs_examples.md):** Convert inline
  documentation code examples into standalone, buildable, and testable files.
- **[test_enhancement_audit](test_enhancement_audit.md):** Audit and improve the
  test coverage for a specific area of functionality.
- _(Others to be proposed)_

## Work Order Execution

The execution of a work order is divided into distinct phases. The AI should
explicitly ask for user confirmation before moving from one phase to the next.

### The "Flush" Operation

A key part of the work order process is the "Flush" operation. A flush is the
process of pushing all relevant context from the conversation—including new
requirements, design decisions, files read, and files modified—into the
scaffolding documents.

A flush must be performed at the end of each phase. This ensures that the
scaffolding files are always up-to-date and accurately reflect the current state
of the work. The AI must ask for user approval before flushing and moving to the
next phase.

### Work Order Phases

1.  **Setup:** Create the scaffolding documents relevant to the work order type.
    The specific work order definition outlines the required files, their
    initial content, and their location.

2.  **Planning:** Iteratively expand the scaffolding documents. This involves
    reading existing code, asking clarifying questions, and refining the plan
    with the user. A key output of this phase is an initial estimate of the
    work's scope and size.

3.  **Critic:** Before implementation, critically review the plan. This phase is
    conducted in two stages to ensure a thorough and deep analysis.
    -   **Stage 1: Initial Review:** Act as an expert principal software
        engineer. Scrutinize the proposed changes, read the surrounding code,
        and look for potential issues. Consider maintainability, fragility, and
        interactions with other parts of the system. The goal is to catch lazy
        planning, such as assuming what code does instead of reading it.
    -   **Stage 2: Deep Expert Review:** Based on the findings from the initial
        review, conduct a second, more intensive critique. Challenge every
        assumption. The perspective is that of a skeptical, external auditor
        poking holes in the approach. The Critic must actively challenge the
        plan's assumptions. Use tools like `read_file` and `glob` to find and
        examine existing patterns and prove the plan is viable *before* writing
        code. The goal is to find flaws in the plan before they become bugs in
        the implementation.

4.  **Implementation:** Work through the defined tasks incrementally. The
    scaffolding documents should be updated as milestones are reached and
    understanding evolves. The Implementation phase must proceed as a series of
    small, verifiable steps. For each task, the required workflow is:
    **1. Implement** the minimal change. **2. Verify** the change by running
    the appropriate build or test command. **3. Document** the successful step
    by checking off the task. Do not proceed to the next task until the current
    one is verified.

5.  **Verification:** Evaluate whether the goal has been accomplished. This
    includes ensuring the implemented code matches the design, all tasks are
    completed, and any implied testing has been performed.

6.  **Work order review (Optional):** After completing the work, consider
    reviewing the work order process itself. This is an opportunity to reflect
    on what went well and what could be improved in the work order template
    (e.g., the `new_feature.md` skeleton). The goal is to refine the process for
    future tasks. This review should also be a two-stage process:
    -   **Stage 1: Initial Feedback:** Discuss with the user if any
        improvements can be made to the work order template for future use.
    -   **Stage 2: Deep Analysis:** Based on the initial feedback, perform a
        deeper analysis of the work order's execution. Identify specific,
        actionable improvements to the process or templates and propose them to
        the user.


7.  **Integration & Cleanup:** The work is not done when the code is committed.
    This final phase ensures the change is properly integrated, documented, and
    communicated. Only after these steps are complete is the work order
    finished.

### Learning from Experience: Capturing Blind Alleys

To avoid getting stuck in failure loops, it is critical to document blind alleys
and failed approaches. When an attempted change (e.g., Change A) fails and you
move to another approach (Change B), the reasons for Change A's failure must be
recorded. This prevents re-trying failed solutions.

Each work order's design document (e.g., `design.md`) should have a section for
"Alternatives Considered" or a similar title. This section must be updated with:

- A description of the failed approach.
- A clear explanation of why it failed.
- Any insights gained from the attempt.

This practice ensures that the project benefits from all work, even failed
attempts, and provides valuable context for future development and for resuming
work.

### Work Order State

The current state of a work order is defined entirely by the content of its
scaffolding files in git. In a Gerrit-based workflow, a single commit is
typically amended over time with updates to these files, representing the
evolving state of the work order.

### Pausing and Resuming Work

A key benefit of the work order system is the ability to pause and resume work
without losing context. To resume a work order, the AI must first re-read the
`design.md` (or equivalent) and `tasks.md` in their entirety to fully reload the
context before proceeding.

### Common Work Order Processes

Every work order must include a task list in a markdown file (e.g., `tasks.md`).
This file must indicate the current phase of the work order.

Here is a typical structure for a tasks file:

```markdown
**Work order:** new_feature: New filtering syntax

- [x] **Setup:** Scaffolding documents created.
- [x] **Planning:** Iterating on design and task list.
- [ ] **Critic:** Not started.
- [ ] **Implementation:** In progress.
- [ ] **Verification:** Not started.
- [ ] **Work order review (Optional):** Not started.
- [ ] **Integration & Cleanup:** Not started.

---

### Detailed Tasks

... (work-order-specific task hierarchy) ...
```

The named work order type (e.g., "new_feature") should correspond to a defined
work order.

### Initiating and Continuing Work Orders

The AI should proactively suggest using a work order when a user's request is
complex and would benefit from a more structured approach. Each work order has a
series of hints (triggers) for when to suggest it.

When initiating a work order seems appropriate, don't just suggest starting the
work order, also give a two-sentence summary of that work order.

### Standardized AI Communication

To make the work order process clear and predictable, the AI assistant will use
standardized phrases at key points in the workflow. This allows users to easily
recognize the state of the process.

- **Suggesting a work order:**

  > This seems like a good opportunity to use a work order. Would you like to
  > start the **Work Order: [Work Order Name]**?

- **Moving to a new phase:**

  > The **[Current Phase Name]** phase is complete. I will now perform a flush
  > to update the scaffolding documents. Once that is done, are you ready to
  > move to the **[Next Phase Name]** phase?

- **Completing a work order:**
  > The **Work Order: [Work Order Name]** is complete. I will now delete the
  > scaffolding files and prepare the final commit.
