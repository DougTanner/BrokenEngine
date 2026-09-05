# Run Checkpoint

One external review covers the run for tooling friction and, on Claude, for
context efficiency and for content that a subagent could have consumed instead.
[worker.md](worker.md) owns when it runs; this reference owns how.

## Evidence and measurement

The reviewer's run evidence is main's own conversation transcript. Main resolves
its absolute path in its own session shell, with
[follow-up-provenance.md](follow-up-provenance.md) owning which session ID
applies:
`(Get-ChildItem "$env:USERPROFILE/.claude/projects/*/$env:CLAUDE_CODE_SESSION_ID.jsonl").FullName`
Then main captures the measurement, from the session worktree root and from
the PowerShell tool only (the redirect is a result-consuming form under the
root [AGENTS.md](../../../../AGENTS.md) bundled-script rule):
`pwsh -NoProfile -File .agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1 -SessionId <id> > Temp/checkpoint-envelope.json`
Main brings only the fields `## Measurement states` classifies on into its
context, from the PowerShell tool:
`Get-Content -Raw Temp/checkpoint-envelope.json | ConvertFrom-Json | Select-Object verdict, breachRowsTruncated, status, code`
A measurement envelope fills `verdict` and `breachRowsTruncated`; a blocked or
error envelope fills `status` and `code` instead. The whole captured file still
reaches the reviewer by the append below.

## Dispatch

Main dispatches one fresh `reviewer` subagent for `/next-plan-checkpoint-review`
with a scope file on this template. The envelope label is the
scope file's last line, and a captured envelope becomes its value only by
appending the captured file's bytes on the line after it, from the PowerShell
tool:
`Get-Content -Raw Temp/checkpoint-envelope.json | Add-Content <scope file>`

```text
Transcript: <absolute transcript path>
Claimed Plan: <Documents/Plans/... | no claim>
Context-efficiency envelope: <pass | blocked (<code>) | blocked (breach-rows-truncated) | none (codex) | the appended captured envelope>
```

The transcript path is scope-file content only; no transcript path or transcript
text ever enters a Plan body.

## Measurement states

The measurement's own state never blocks the friction lens.

| Measurement state | Value supplied to the reviewer | Handoff line recorded |
| --- | --- | --- |
| `pass` envelope | `pass` | `Context-efficiency follow-ups: <isolation-lens Plan path(s) or none>` |
| `needs-review` envelope | the appended captured envelope | `Context-efficiency follow-ups:` Plan path(s) or none, from the reviewer's findings |
| blocked or error envelope | `blocked (<code>)` | `Context-efficiency follow-ups: blocked (<code>), then any isolation-lens Plan path(s)` |
| `breachRowsTruncated: true` envelope | `blocked (breach-rows-truncated)` | `Context-efficiency follow-ups: blocked (breach-rows-truncated), then any isolation-lens Plan path(s)` |
| transcript path unresolvable | nothing — no reviewer is dispatched, because there is no run evidence to review | `blocked (transcript-unavailable)` on both `Friction follow-ups:` and `Context-efficiency follow-ups:`, routed through the post-checkpoint rule below |
| Codex main session | nothing — no lens runs | `none (codex)` on both lines |

The `Handoff line recorded` column is main's own record after `## Follow-up
routing`, not reviewer output: `/next-plan-checkpoint-review` returns findings
and its own summary block, and main writes both follow-up lines from what that
routing produced. The `Friction follow-ups:` line records `<Plan path(s) or
none>` in every row that does not name it. The `Context-efficiency follow-ups:`
line's `blocked (<code>)` form covers those codes and `transcript-unavailable`.
The reviewer skips the context-efficiency lens in every blocked and error case.
All of those states are themselves tooling friction the same reviewer sees in
the transcript it is already reading, so none of them needs recovery machinery
of its own. A Codex session's measurement reads Claude transcripts only, and
this repository documents no way for a Codex main to name its own live
transcript, so Codex coverage of all three concerns stays with
`/next-plan-review` after landing.

## Follow-up routing

For each accepted finding that the Verify the acceptance table step's own
fix-it-here rule in root
[AGENTS.md](../../../../AGENTS.md) does not fix inside this session, an
`implementer` routes it through `/create-follow-up-plans` as a tooling-friction
or isolation proposal, supplying the observed symptom with its citation plus the
provenance block sourced per [follow-up-provenance.md](follow-up-provenance.md).
An `active-change-blocker` finding never becomes a follow-up Plan: it returns
to the current change as a blocker. This Plan is a leftover routed after the
Review and resolve correctness review dispatch; the Verify the acceptance table
step in root [AGENTS.md](../../../../AGENTS.md) owns how it is authored and
verified.

Friction first observed after this checkpoint — including friction in running
the claim-exit script and in `/finalize-changes` itself — is recorded through
`/create-follow-up-plans` by an `implementer` and lands at a later gate as its
own content. On deferral, or when the run ends without a claim, the checkpoint's
Plans are themselves the landed content and the landing gate applies to them.
