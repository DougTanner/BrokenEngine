# Harness documentation relies on shell variables that do not survive an agent tool call

## Problem

`Projects/BrokenEngineSandbox/Documents/AgentHarness.md` documents the launch and verification
workflow as several separate fenced PowerShell blocks that read variables assigned in earlier
blocks. Agent tool calls do not carry shell state between calls — the PowerShell tool's own
contract keeps the working directory but discards variables and functions — so an agent that runs
each block as its own call binds `$null` for every cross-block variable, and the invocation either
fails parameter binding or silently binds a wrong value.

Concrete sites in that file:

- `:42` — `& $AgentHarness --owner $Owner --port 27101 -`; both variables come from the skill's
  provision/claim step, in an earlier call.
- `:53` — the prose instruction to run the readiness helper "preserving the `$ServerPid` and
  `$ClientPid` variables above", which were assigned in the `:24-33` launch block.
- `:56` — `Wait-IslandSceneReady.ps1` invoked with `-AgentHarness $AgentHarness -Owner $Owner
  -ArtifactPath (Join-Path $TempDir 'island-scene-readiness.json')`; `$TempDir` is assigned at
  `:13`, in the launch block.
- `:62` — `$IslandReadinessArtifact = Join-Path $TempDir 'island-scene-readiness.json'`, same
  `$TempDir` dependency.

Root `AGENTS.md:101` now fixes the canonical bundled-script invocation form and requires one script
invocation per shell call. That rule makes the reliance unworkable rather than merely fragile: an
agent cannot both obey it and carry a variable from a previous block. By the same directive's own
words — "a script that cannot be run as documented is a bug" — this is a defect in the
documentation.

This condition is pre-existing. It was confirmed while resolving a scope-review finding in the
session that produced this record; that session's change split one block in two and thereby made
`$IslandReadinessArtifact` newly cross a boundary, fixed the newly created boundary by inlining,
and deliberately left the pre-existing variables untouched as out of scope.

## Why this is not yet a Plan

The remedy shape is an open decision, and one candidate contradicts an existing directive in the
skill that owns the lifecycle contract.

`.agents/skills/agent-harness/SKILL.md:85` instructs callers to "not rename or replace the retained
`$ServerPid`/`$ClientPid` lifecycle variables", and `:122` consumes those same two variables in the
release invocation, which is necessarily a later, separate call. So the process-identifier half of
this problem is owned by the skill, not by the project document, and cannot be resolved inside
`AgentHarness.md` alone without leaving the two documents contradicting each other.

## Options

**A. Literal / placeholder substitution.** Replace each cross-call variable in the documented
blocks with a quoted angle-bracket placeholder the agent fills in with the concrete value it
already knows, exactly as `.agents/skills/agent-harness/SKILL.md:88` was changed to do for
`-ArtifactPath '<absolute artifact path>'`. Precedent exists and the change stays textual, but it
reaches into SKILL.md's PID-retention wording at `:85` and `:122`, so that contract has to be
restated rather than merely referenced.

**B. Temp driver script.** Have the documented workflow write a driver script under `$ROOT\Temp`
and run it with one `pwsh -NoProfile -File` call, which `.agents/skills/agent-harness/SKILL.md:108`
already prescribes for any multi-line PowerShell driver. Variables then live inside one process and
never cross a call boundary. This preserves the existing `$ServerPid`/`$ClientPid` retention
language, but changes the shape of the documented workflow much more broadly, and has to say where
that driver's own results are read back from.

**C. Split the two halves.** Apply A to the plain value carriers (`$AgentHarness`, `$Owner`,
`$TempDir`), which the agent can always restate literally, and keep the PID retention as-is by
making the launch and release calls one documented unit under B. Smallest total churn, but leaves
two conventions in one document.

## Decision needed

Pick one shape, and decide whether the resulting change is confined to
`Projects/BrokenEngineSandbox/Documents/AgentHarness.md` or must also amend
`.agents/skills/agent-harness/SKILL.md`. Once that is settled, this becomes a documentation Plan
under `Documents/Plans/Agents/`, with `/validate-skill` required for any changed `SKILL.md`.

## Critical files

- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — `:13`, `:21-22`, `:24-33`, `:42`,
  `:53`, `:56`, `:62`
- `.agents/skills/agent-harness/SKILL.md` — `:85`, `:88`, `:108`, `:122`
- `AGENTS.md:101` — the canonical bundled-script invocation form and the one-invocation-per-call
  rule

## Notes

No runtime, determinism, serialization, wire, or build surface is involved; the exposure is agent
workflow documentation only. Expected Tier 1 once decided, since the resulting change is
documentation with no public signature or invariant exposure.
