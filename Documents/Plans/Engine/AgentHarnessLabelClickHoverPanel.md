<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T01:08:56.018Z","dependsOn":[]} -->
# Fix: agent-harness `click` — a label click on a hover-opened HUD panel fails with `no widget matches label`

## Context
Observed symptom: during a `/next-plan` run that recorded a replay through the
agent harness, the client command `click {"label":"[+F]##Fighter"}` failed with
`no widget matches label; candidates: ...` (thrown from `BeginScriptAndDefer` in
`Engine/Source/Agent/AgentCommandsClientGeneric.cpp` when the click script
finishes with `AgentScriptStatus::kNotFound`). The target button lives in the
HUD `FleetPanel` window (`HudScreen::RenderFleetPanel` in
`Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp`), which slides in
only while the ImGui mouse position is inside the left hover strip
(`engine::ComputeMouseOpennessTarget` in `HudScreen::Render`). The run had
opened the panel with two coordinate `mouse move` commands near the left edge
before the click.

What the observing run saw: the label click cleared the pinned mouse position,
and the hover-opened `FleetPanel` slid out while the click script was still
stabilizing its target, so the label stopped resolving. Code facts consistent
with that: `AgentInput::BeginScript` (`Engine/Source/Agent/AgentInput.cpp`)
clears `mbImGuiMousePositionPinned` when any script starts, and the click
script (`AgentScriptKind::kClick` in `AgentInput::AdvanceFrame`) re-issues a
mouse position only from `AgentInput::StabilizeTarget`, which finishes with
`kNotFound` as soon as `AgentUiRegistry::ResolveLabel` misses. The exact frame
sequence is unproven.

Cost: that recording attempt was abandoned. Every later fleet-panel click in the
run went through a hand-written workaround: a coordinate `mouse move` that pins
the mouse inside the hover strip, a wait, `describe_ui` to read the button's
rectangle, then a coordinate `mouse click` at its center.

Reproduction setup the observing run recorded, in order: server and client
launched through `/agent-harness` from an autosave holding one fleet; client
`mouse move` to (120,150), then `mouse move` to (121,150) so the fleet panel
slides in; then `click {"label":"[+F]##Fighter"}`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4422f9e3-bfee-4a1a-abe2-268f45560966
- Worktree/branch UUID: b719c635-512a-4a63-bc40-8e90d6b34190
- Session branch: claude/b719c635-512a-4a63-bc40-8e90d6b34190
- Worktree: .claude\worktrees\BrokenEngine\b719c635-512a-4a63-bc40-8e90d6b34190
- Landing ref: claude/b719c635-512a-4a63-bc40-8e90d6b34190
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/Engine/AgentHarnessLabelClickHoverPanel.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends a fix that makes a label `click` on a hover-opened panel
behave like the documented workaround: the target stays open while the click
script stabilizes and clicks it. Whether that is done by keeping the earlier
pin until the click script issues its own position, or by another change inside
the click script, is for the root cause to decide. The game's hover-to-open
panel behavior is correct for players and is not the fix site. If the root
cause shows the label click cannot reach a hover-opened panel without a game
UI change, the author recommends documenting the workaround in
`### Client command behavior` instead, which this Plan's scope allows.

## Critical files
- `Engine/Source/Agent/AgentInput.cpp` — `AgentInput::BeginScript`,
  `AgentInput::StabilizeTarget`, and the `AgentScriptKind::kClick` case of
  `AgentInput::AdvanceFrame`
- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp` — `CommandClick`
- `.agents/skills/agent-harness/references/command-reference.md` —
  `### Client command behavior`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the functions and section named under
  `## Critical files`, including that section's description of the mouse pin

## Out of scope
- The landed change the session produced
- `HudScreen` panel hover and slide behavior, and every other game UI file
- `hover`, `set_slider`, `key`, `mouse`, and other agent commands, except where
  the pin handling they share with `click` changes as a side effect the fix
  needs; that side effect is then documented in `### Client command behavior`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: client agent-input scripting in one
subsystem); escalate if the fix reaches build/bootstrap coordination. Agent
input is client-only and outside the simulation CRC; the fix must not change
input delivery for a non-agent client. Never embed transcript paths or home
paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  with the reproduction setup in `## Context`, `click {"label":"[+F]##Fighter"}`
  returns `found: true` and the server log shows the resulting
  spawn-into-fleet request, verified live through `/agent-harness`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
