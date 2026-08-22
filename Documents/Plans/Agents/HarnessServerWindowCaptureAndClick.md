<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T15:36:14.863Z","dependsOn":[]} -->
# Fix: /agent-harness — no way to screenshot or click the server's window

## Context
Harness acceptance verification of the engine-owned server monitoring window had
no harness route to observe or drive that window. `screenshot`, `describe_ui`,
and `click` are client-only commands
(`Projects/BrokenEngineSandbox/Documents/AgentHarness.md:392`, `:398`, `:399`,
under `### Client commands`), and
`.agents/skills/agent-harness/references/command-reference.md` states that
sending a side-specific command to the other executable returns
`unknown command`; the shared command set it lists (`ping`, `quit`, `get_logs`,
`set_log_level`, `crash_report_fixture`) contains nothing that captures or
clicks a window. The server's monitoring window is a plain GDI window, not an
ImGui surface, so no client UI command applies to it either.

The verifying worker therefore improvised outside the harness:

- Capture: ad-hoc `PrintWindow(hwnd, hdc, PW_RENDERFULLCONTENT)` scripts written
  under this worktree's ignored `Temp/` directory, re-authored per check instead
  of invoked from the skill.
- Clicks: `SetCursorPos` plus `mouse_event` after raising the target window
  topmost — synthetic global input that steals the physical cursor and depends
  on window z-order.
- Failed approach worth recording: cross-process `PostMessage` and
  `SendMessageTimeout` of `WM_LBUTTONDOWN`/`WM_LBUTTONUP` returned success but
  the window's handler never ran, so posted-message clicking fails silently with
  no error to detect. Any future automation that reaches for it will appear to
  work while doing nothing.

Citation: this session's harness verification handoff; the capture and click
scripts remain under the ignored `Temp/` directory of the worktree recorded
below and disappear with it.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: a01ac9d1-bdae-49dd-b97b-0b1c9d71c2b0
- Worktree/branch UUID: 7f1a2780-9111-4062-a0f1-ed6c34f89d56
- Session branch: claude/7f1a2780-9111-4062-a0f1-ed6c34f89d56
- Worktree: .claude\worktrees\BrokenEngine\7f1a2780-9111-4062-a0f1-ed6c34f89d56
- Landing ref: claude/7f1a2780-9111-4062-a0f1-ed6c34f89d56
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/HarnessServerWindowCaptureAndClick.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
In a new session, run `/next-plan-review claude/7f1a2780-9111-4062-a0f1-ed6c34f89d56`,
supplying client `claude` and the conversation session ID recorded above. Root-cause
the friction from the proven transcript, then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author's recommendation, for the fixing session to confirm or replace once
the transcript proves the root cause, is to prefer documenting the working
recipe over adding a new command: a short reference section covering the
`PrintWindow` + `PW_RENDERFULLCONTENT` capture, the synthetic-input click with
its cursor and z-order caveats, and the posted-message pitfall is a much smaller
change than a new server-side window command, and the server window is a plain
GDI surface with no ImGui item labels for a `click`-style command to target. A
new command becomes the better option only if the transcript shows the
documented recipe cannot be made reliable.

## Critical files
- `.agents/skills/agent-harness/SKILL.md`
- `.agents/skills/agent-harness/references/command-reference.md`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md`

## In scope
- Root-cause investigation via /next-plan-review, run with client `claude`, the
  review ref named in `## Design`, and the recorded conversation session ID.
- The smallest resulting fix, confined to the files named above: the harness
  skill's verification guidance, the shared command reference's statement of
  what can and cannot be observed on the server side, and the project command
  document's client/server command sections.

## Out of scope
- The landed change the session produced (the engine-owned server monitoring
  window itself) and any change to that window's behavior.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.
- Engine, game, simulation, CRC, replay, wire, or gameplay behavior.
- Unit tests.

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces: a session needing to observe or
  drive the server window finds a documented harness route or a documented
  recipe, without re-authoring an ad-hoc capture script.
- The posted-message failure mode is recorded where an automating session will
  see it before trying it.
- /validate-skill passes for any changed SKILL.md; `plan validate` exits 0.
