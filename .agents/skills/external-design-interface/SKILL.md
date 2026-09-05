---
name: external-design-interface
description: >-
  Generate three radically different C++ interface designs through a read-only
  workflow, compare them, and synthesize the user's choice into a reviewed
  implementation plan. Use when
  the user explicitly asks to design an API, explore interface options, or
  design a system multiple ways, or when interface design for a new Collection,
  manager, or subsystem API is implicitly detected (ask before running); adding
  a member to an existing Collection routes to /add-collection-member instead.
allowed-tools: [Read, Grep, Glob, Agent, AskUserQuestion]
---

# Design Interface

## Purpose

This workflow is Option 1: read-only proposals. The manager owns the comparison
packet, user interaction, exact selection, and the selected design carried into
the implementation plan; workers return evidence and declarations only.

## When to use

Use this skill when interface design is requested or only implicitly
detected — a new Collection, manager, or subsystem API. Do not suggest or use
it for bug fixes, single-function additions, implementation of an already
approved interface, or adding a member to an existing Collection; that last case
routes to `/add-collection-member`. This skill replaces `/plan-alternatives` for
a new Collection, manager, or subsystem API.

## Inputs

Use the supplied system description, or ask for one when absent.

Main runs this orchestration. Supply every child the authoritative shared task
brief, the design brief and fixed scope/compatibility constraints. A delegated
worker never dispatches another worker; if this skill is entered where
delegation is forbidden, return a main-context dispatch requirement instead of
approximating the independent designs inline.

## Handoff

Each researcher and planner returns the complete shared handoff from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md),
with `Residuals` last. Add these domain fields before `Status`:

```text
Researcher extension:
Pattern evidence: <similar systems, caller patterns, conventions, manager/workbuffer use, and collection shapes with file:line or symbol>

Planner extension:
Design axis: Minimal surface | Data locality | Caller ergonomics
Interface declarations: <header-style declarations>
Representative usage: <dominant call-site example>
Hidden complexity: <internal machinery and ownership>
Runtime contracts: <client/server guards, allocation, threading, frame phase, and deterministic-state effect>
Collections and members: <each proposed Collection or SOA member, or none>
Trade-offs: <interface depth, SOA fit, dispatch safety, and costs>

Shared handoff:
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <none>
Changed files: none
Decisive checks: <repository evidence or design constraint checks>
Build required: none
Evidence: <existing or Temp/ path plus selector, or none>
Executor: <own model id> <own effort>, each unknown when unreadable
Residuals: <missing evidence, unresolved constraint, or none>
```

The researcher uses only the researcher extension; each planner uses only the
planner extension. Main consumes those child handoffs, then owns the comparison
packet and user-facing decision below; that presentation is outside the child
envelopes.

### Implicit detection

When the need is only implicitly detected, first ask "Would you like me to run
/external-design-interface to explore different API shapes?" and continue only
if accepted.

### Compare and Decide

Present all three designs with their declarations, usage, and hidden
complexity. Compare interface simplicity and depth, SOA friendliness,
client/server gating, main-loop allocation, dispatch safety, frame-phase
clarity, and deterministic-state placement. Give an opinionated recommendation
grounded in the brief; do not rank by implementation effort.

Ask the user which design or explicit hybrid to carry forward. Do not silently
choose a public interface when multiple meaningfully different shapes remain.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The brief, research, design, and
  synthesis steps the workflow runs.
- [`ScratchInterfacePrototypes.md`](../../../Documents/Features/Agents/ScratchInterfacePrototypes.md)
  — Option 2, a separate future Feature, not a mode of this skill.
- [`IsolatedInterfacePrototypeWorktrees.md`](../../../Documents/Features/Agents/IsolatedInterfacePrototypeWorktrees.md)
  — Option 3, a separate future Feature, not a mode of this skill.
