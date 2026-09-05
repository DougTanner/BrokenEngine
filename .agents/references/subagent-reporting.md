# Delegated Reporting

Delegation uses a fresh, isolated context by default: Codex uses
`fork_turns: "none"`; Claude receives a fresh prompt. A positive Codex turn
fork is allowed only when exact authoritative conversation text cannot be
safely summarized, and the prompt states why.

Root `AGENTS.md`'s rule that subagents never spawn subagents is enforced by
`CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH=1` and by `disallowedTools: Agent` in each
role definition.

A delegated review or audit runs solely inside one fresh delegated `reviewer`
that returns findings only — except `/comment-review`, the checklist review the
root `AGENTS.md` role table assigns to `mechanic` — and the tool restrictions a
skill body states are prose boundaries rather than host enforcement.

## Task brief

Every delegation supplies one self-contained brief on this form:

```text
Role: <role name from the root AGENTS.md table>
Skill: <skill to run, or none>
Objective: <one sentence>
Required sections: <handoff fields or report sections the caller will read>
Scope: <files, regions, or artifacts owned>
Exclusions: <what this worker must not touch>
Fixed decisions: <user decisions already made, or none>
Governing paths: <repository paths that bind this work>
Baseline: <full SHA and worktree/branch from Get-AgentWorktreeSessionContext>
Acceptance: <checks and expected observations>
Prohibitions: <task-specific bans, or none>
Return: <return format, normally the shared handoff>
```

`Required sections` names which skill-specific report sections the caller will
read; the worker skips the rest. The shared handoff is always returned in full.

The session baseline is the commit a session's work is diffed against, fixed
when the session starts. The baseline and every other machine-derivable
identity value in a brief are copied from `Get-AgentWorktreeSessionContext`
output, never retyped from memory or scrollback. From the session worktree
root:

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
records; `.agents/skills/next-plan/references/follow-up-provenance.md`,
`## Conversation session ID`, owns where that value comes from.

On a direct `reviewer` dispatch the targets file where the assigned skill
requires one, and the changed-file inventory where the assigned skill documents
a run, come from that documented run of
`.agents/scripts/Get-SessionChangeInventory.ps1`, which supplies the script's
mandatory `-RepositoryRoot` and `-Baseline` along with any switch the skill
requires — `.agents/skills/repo-code-review/SKILL.md` `## Inputs` for the
`-EmitTargets` form, and `.agents/skills/adversarial-review/SKILL.md`
`## Inputs` for the plain changed-file inventory run — rather than from a call
composed ad hoc or from anything retyped into the brief.

Use `none` when a field has no value. Add only fields required by the invoked
skill. Cite repository paths; do not paste root instructions, skill bodies, the
complete Plan, manager transcript, unrelated handoffs, or raw XML/logs. When a
completed Plan file has already been deleted from the tree, the manager supplies
its approved text or the Git-history path to read it from.

Workers verify supplied hints against the tree, stay within ownership, and
expand only for a concrete dependency, decision, or failure. They never route
work to another worker.

## Handoffs

A handoff is the short structured result a subagent returns to its manager.
Return only decision-relevant evidence:

```text
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <review roles only; one row each: ID Critical|Required|Recommended path:line — claim — evidence>
Changed files: <one row each, path and region; or none>
Decisive checks: <one row each, command or read and its result>
Build required: <exact targets, or none>
Evidence: <existing or Temp/ path plus selector, or none>
Executor: <own model id> <own effort>, each unknown when unreadable
Residuals: <actionable blocker or none>
```

Every row is one line. Do not quote code and do not repeat a row from another
field. A field over 10 rows, or a whole handoff over 40 lines or 20,000
characters, moves its full material to an existing file or log, or to a `Temp/`
file when no existing file holds it, and cites it under `Evidence` as path plus
selector. A selector into a Markdown file is a `##` heading in it. The handoff
itself still carries everything main needs; the file is for the workers main
dispatches next, cited to them as path plus selector. A typed envelope a role
must return verbatim, such as the `broken-engine-build-result/v1` envelope the
`builder` returns and the typed receipts `/finalize-changes` consumes, is exempt
and does not count toward these limits.

Both `Executor` values are what the host reports to the worker itself: the model
identity the host states in the worker's own context, and the effort from the
worker's own `CLAUDE_EFFORT` shell environment variable when it is set. Never
read a session transcript for either value — in a subagent shell,
`CLAUDE_CODE_SESSION_ID` and the matching transcript under `~/.claude/projects/`
name the parent session, so that route mis-attributes the parent's model to the
child. A Codex worker has no runtime source for either value — config and CLI
pins state intent, not proof — so it writes `unknown`, and headless
`/codex-review` runs are instead proved by their commit-time wrapper pins. Each
of the two values is written `unknown` independently when its source is
unreadable.

A skill extends this form only by adding rows inside an existing field or by
declaring extra fields in its own `## Handoff` section, each one line or one row
per item, never a paragraph. `Build required` stays present and `Residuals`
stays last. A target that `/compile` builds in Release only carries
`Release|x64` and no other configuration/platform under `Build required`.
Independent review and verification use a context that did not produce the
work. A focused correction/retest also uses an independent context.

A worker ends its turn with the handoff as its final answer and never enters an
open-ended wait after delivering it; continuation goes through the host's resume
path. Any wait a worker issues mid-task carries a bounded timeout well under the
host tool cap. Ending a turn to await one's own background child is that
prohibited open-ended wait: a completion notification cannot resume a worker
whose turn has ended, so capture the child's result in-turn before delivering
the handoff.

Main consumes each dispatched worker's handoff once, from the host's own
delivery of it; it requests that worker's result again only through the
no-progress and terminal-failure route below.

What main does with each field:

| Field | What main does with it |
| --- | --- |
| `Status` | Routes the work: PASS advances; NEEDS_ACTION decides each finding, sending accepted fixes to changed artifacts to `/resolve-findings` and accepted plan findings back into the plan; BLOCKED resolves or asks the user. |
| `Findings` | Decided one at a time. |
| `Changed files` | Bounds what is re-reviewed. |
| `Decisive checks` | Feed the acceptance table. |
| `Build required` | Goes to a `builder`. |
| `Evidence` | Passed on to later workers as path plus selector; main reads it only when a decision needs it, and then only at the cited selector, not the file whole. |
| `Executor` | Proves routing. |
| `Residuals` | Go to the message footer or to `/create-follow-up-plans`. |

## Whether a worker is still running, and interruption

Judge whether a worker is still running only from host status and explicit
progress or partial handoffs.
A running host status, elapsed time, or wait boundary does not require a
progress ping or repeated manager status command. Forward progress is a newly
reported distinct action, narrowed search, new evidence, or synthesis; a loop
is explicit repetition without narrowing or new evidence. A worker messages its
manager mid-task only to ask a blocking question or hand off a partial result,
never to narrate status; manager-to-user narration is unaffected.

Documented no-progress means no recorded worker tool call or message within a
no-activity window the manager states in advance, measured from the worker's
last recorded action; elapsed turn time alone does not establish it. When
terminal failure or documented no-progress/loop evidence exists:

1. inspect host status and available partial evidence;
2. request the handoff immediately;
3. allow a fixed response window; and
4. interrupt or replace only if the failure or no-progress evidence persists.

Prefer resuming the same worker. Otherwise supply a recovery capsule containing
objective/scope, meaningful identity, completed evidence, unresolved issue, next
action, and the skill to resume with (for example `/implement-plan`,
`/resolve-findings`, or `/update-affected-code`) so completed exploration is not
repeated.
