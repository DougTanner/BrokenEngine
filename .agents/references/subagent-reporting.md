# Delegated Reporting

## Delegated execution context

Delegation uses a fresh, isolated context by default: Codex uses
`fork_turns: "none"`; Claude receives a fresh prompt. A positive Codex turn
fork is allowed only when exact authoritative conversation text cannot be
safely summarized, and the prompt states why.

The Change Workflow delegation role table authoritatively assigns delegated
work; a role exception it does not list is stated in the skill it governs. This
reference owns only reporting-local conduct: independent findings-only work uses
a fresh context, any authorized narrow edit path includes its required
self-check, tool restrictions a skill body states are prose boundaries rather
than host enforcement, and a worker reads a Coretide inventory entry's diff in
the form below.

A worker reads the diff of a changed-file inventory entry under
`Projects/Coretide/` (origin: `.agents/scripts/Get-SessionChangeInventory.ps1`
header) with
`git -C Projects/Coretide diff <coretide.baselineSha> [<coretide.headSha>]`,
taking both values from the same inventory result. A review row's `Head` is the
snapshot that review reads: pass it as the inventory `-Head`, use it as the
committed head of every diff, and read file text with `git show <Head>:<path>`
(a Coretide path at that inventory's `coretide.headSha`); a script that reads
files from disk reads the working tree, which matches the snapshot while the
round is open.

## Task brief

Every delegation supplies one self-contained brief on this form:

```text
Role: <role name from the delegation role table>
Skill: <skill to run, or none>
Objective: <one sentence>
Required sections: <handoff fields or report sections the caller will read>
Scope: <files, regions, artifacts; review outputs per below>
Exclusions: <what this worker must not touch>
Fixed decisions: <user/manager decisions, or none>
Governing paths: <binding paths and subdirectory AGENTS.md chain>
Baseline: <full SHA and worktree/branch from Get-AgentWorktreeSessionContext>
Acceptance: <checks and expected observations>
Prohibitions: <task-specific bans, or none>
Return: <the shared handoff plus the named skill's declared extension fields, or the typed report that skill's `## Handoff` requires>
```

`Required sections` names skill-specific sections to return; skip the rest.
Always return the full shared handoff. Cite readable review outputs per
[`change-workflow.md`](change-workflow.md#subagents) `### Subagents`.

The session baseline is the attribution and delegation fork point for the
session's work. It begins as the commit reported by
`Get-AgentWorktreeSessionContext`, and changes only when an authorized branch
synchronization reports its authoritative new baseline. Use the recorded
baseline for every later delegation brief and changed-file inventory. Keep it
in the running workflow context; do not create a second session-config or
baseline store. For `/next-plan`,
[`claim-results.md`](../skills/next-plan/references/claim-results.md#session-baseline-and-the-sync-object)
owns how a claim receipt updates the recorded baseline. Every other
machine-derivable identity value in a brief remains copied from
`Get-AgentWorktreeSessionContext` output, never retyped from memory or
scrollback. From the session worktree root:

```powershell
Import-Module ./.agents/scripts/AgentWorktreeSession.psm1
Get-AgentWorktreeSessionContext
```

Inside a `/next-plan` run, copy these values from the `Get-NextPlanContext`
output already in context: `SessionBranch` is `Branch`, `Session` is
`SessionId`, `Primary` is `PrimaryRoot`, and `TargetBranch` is `PrimaryBranch`;
`Worktree` and `PrimaryTip` keep their names. Use the recorded `Baseline`
governed by `.agents/skills/next-plan/references/claim-results.md`,
`## Session baseline and the sync object`. Do not run
`Get-AgentWorktreeSessionContext` separately.

The leading `./` is required — without it PowerShell treats the path as a
module name and searches `PSModulePath` instead of the worktree.

That output's `SessionId` is the worktree/branch UUID parsed from the session
branch, never the conversation session ID a follow-up Plan's provenance block
records (where that ID comes from:
`.agents/skills/next-plan/references/follow-up-provenance.md`
`## Conversation session ID`).

On a direct `reviewer` dispatch, when `## Inputs` documents a pre-dispatch
`.agents/scripts/Get-SessionChangeInventory.ps1` run, its named producer writes
the gitignored `Temp/` receipt and passes that path — never raw JSON in main
context, ad-hoc calls, or retyped entries; self-deriving workers take `Baseline`
and `Scope` directly per `## Inputs`. Public subagent `SKILL.md` files (paired
with `references/worker.md`, per
[`skill-skeleton.md`](skill-skeleton.md#section-placement)) may be read whole
for brief or handoff work; small main-readable `.agents/` workflow references
may be read whole once per checkpoint span via `Read`/`read_file` under the
allowlist and cap in
`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
(`$script:MainOwnedReferenceDocs`, `Get-MainWorkflowRefKey`,
`$script:WorkflowRefMaxChars`). Main never opens `references/worker.md` or
worker-only references; all other cited files stay section-bounded per
`AGENTS.md` `## Directives` Bounded reading. Each skill's `## Inputs` owns
command form and producer (e.g.,
`.agents/skills/adversarial-review/SKILL.md` `## Inputs`).

Use `none` for empty fields; add only skill-required fields. Cite paths; for a
plan snapshot or execution card, cite its worktree-relative path plus only
needed `##` selectors (e.g., `## Execution card` alone), never repeating planned
mechanism or check/fixture text; keep assigned items (section/label), scope,
post-plan deltas, and required fields inline. Get `AGENTS.md` chains via
`.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1 <path>
[<path> ...]` (positional paths, per `AGENTS.md` `## Directives` Bundled
scripts; omit `Temp/` and reparse points, including hookless Codex runs). Never
paste root docs, skill bodies, full Plans, transcripts, unrelated handoffs, or
raw XML/logs (deleted Plan: approved text or Git-history path).

Workers verify supplied hints against the tree, stay within ownership, and
expand only for a concrete dependency, decision, or failure. They never route
work to another worker.

## Handoffs

The handoff form a worker returns and the field-authoring rules are owned by
[`subagent-handoff.md`](subagent-handoff.md), `## Handoffs`.

What main does with each field:

| Field | What main does with it |
| --- | --- |
| `Status` | Routes the work: PASS advances; NEEDS_ACTION decides each finding, sending accepted fixes to changed artifacts to `/resolve-findings` and accepted plan findings back into the plan; BLOCKED resolves or asks the user. |
| `Findings` | Decided one at a time; rejections follow [change-workflow.md `### Delegation roles`](change-workflow.md#delegation-roles). Disputed or unquoted repository source checks go to a `locator` brief per [change-workflow.md `### Resolving Ambiguity`](change-workflow.md#resolving-ambiguity) instead of main reading the cited source, including at an `Evidence` selector. |
| `Changed files` | Marks a file-changing dispatch, which [change-workflow.md `#### Step 6 — Review and resolve correctness`](change-workflow.md#step-6--review-and-resolve-correctness) routes. |
| `Decisive checks` | Feed the acceptance table. |
| `Build required` | Goes to a `builder`. |
| `Evidence` | Passed on to later workers as path plus selector; main reads it only when a decision needs it or when it must act on moved material, such as dispatching or presenting its rows, and then only at the cited selector, not the file whole. |
| `Residuals` | Go into the message that reports the task, or to `/create-follow-up-plans`. A contradiction main must decide before work continues, and that a code read settles, goes to a `researcher` brief naming the cited files and rules, per [change-workflow.md `### Resolving Ambiguity`](change-workflow.md#resolving-ambiguity), instead of main reading the cited source, including at an `Evidence` selector. |

## Whether a worker is still running, and interruption

On Claude, main waits for a background worker by ending the turn or doing
independent work and letting the host's task notification deliver the handoff;
it never fetches that worker's result with a blocking host call such as
`TaskOutput`, because the notification carries the same handoff text and the
fetch would take it twice, against
[`subagent-handoff.md`](subagent-handoff.md), `## Handoffs`. For a Codex main
session waiting for its own background worker, do useful independent work
first. Then use a native mailbox or worker wait only when the current host
advertises it and its live contract, including any minimum duration, complies
with all higher-priority blocking-wait limits. A wait whose minimum duration
exceeds such a limit is unavailable for that session. When no compliant native
wait is advertised, rely on the host's available completion delivery and do
not invent a wait operation. The timeout, no-progress, and interruption rules
below are the exception route; a wait in them is a bounded host wait call where
the client provides one. A Codex bridge run instead follows the launch and
`-Wait` contract in [`codex-bridge.md`](codex-bridge.md).

Completion can return a wait early. If a healthy worker's wait times out
without failure or no-progress evidence, wait again; the timeout alone does not
justify status investigation, interruption, or replacement.

Judge whether a worker is still running only from host status and explicit
progress or partial handoffs.
A running host status, elapsed time, or wait boundary does not require a
progress ping or repeated manager status command. Forward progress is a newly
reported distinct action, narrowed search, new evidence, or synthesis; a loop
is explicit repetition without narrowing or new evidence. A worker with a
blocking question or a partial result ends its turn with a `BLOCKED` or partial
handoff instead of messaging its manager, and never narrates status;
manager-to-user narration is unaffected.

Documented no-progress means no recorded worker tool call within a no-activity
window the manager states in advance, measured from the worker's last recorded
action; elapsed turn time alone does not establish it. When terminal failure or
documented no-progress/loop evidence exists:

1. inspect host status and available partial evidence;
2. interrupt the worker with the host's stop; and
3. dispatch a fresh worker of the same role whose continuation capsule
   ([`subagent-handoff.md`](subagent-handoff.md) `## Continuation capsule`) is
   built from the partial evidence available, so completed exploration is not
   repeated.
