# Delegated Handoff Form

The form every subagent returns and the rules for authoring its fields. The task
brief that starts a delegation, what main does with each returned field, and the
waiting and interruption mechanics belong to
[`subagent-reporting.md`](subagent-reporting.md).

## Handoffs

A handoff is the short structured result a subagent returns to its manager.
Return only decision-relevant evidence:

```text
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <review roles and skill-declared verification failures only; one row each: ID Critical|Required|Recommended path:line — claim — evidence>
Changed files: <one row each, path and region; or none>
Decisive checks: <one row each, command or read and its result>
Build required: <exact targets, or none>
Evidence: <one row each, existing or Temp/ path plus selector — one-line description of what it holds; or none>
Residuals: <actionable blocker or none>
```

That fenced form is the whole return — nothing precedes or follows it except the
extension fields, row forms, and typed blocks the assigned skill's `## Handoff`
declares — and `Status` carries exactly one of the three tokens alone on its
line. Every row is one line. Do not quote code and do not repeat a row from
another field. The whole returned handoff stays under 4,000 characters, or
under the character cap a skill's `## Handoff` declares in its place. When it
would exceed the cap, the full material of the fields that push it over moves to
an existing file or log, or to a `Temp/` file when no existing file holds it,
with one `##` heading per section of that material, and `Evidence` carries one
table-of-contents row per section:
`<path> ## <heading> — <one-line description of what the section holds>`. A
selector into a Markdown file is a `##` heading in it. Text a skill's
`## Handoff` requires inline never moves to a file, even past the cap. The
handoff itself still carries the decision-relevant summary of each moved field,
such as the count in `Changed files`, so main decides without opening the file;
the file is for the workers main dispatches next, cited to them as path plus
selector, and for main when a decision needs the detail. The report-file
exemption for that file is in root `AGENTS.md` `## Environment`. The Claude Code
host still refuses a Claude Code subagent's `Write` of a `.md` file whose name
begins with `analysis`, `report`, `findings`, or `summary` in any case (a
host-internal list that can change), so the brief or worker naming a `Temp/`
file a handoff cites under `Evidence` prefixes such a name, for example
`temp-analysis.md`.

A skill extends this form only by adding rows inside an existing field or by
declaring extra fields in its own `## Handoff` section, each one line or one row
per item, never a paragraph, and never by re-rendering the form itself.
`Build required` stays present and `Residuals` stays last. A target that
`/compile` builds in Release only carries `Release|x64` and no other
configuration/platform under `Build required`. Independent review and
verification use a context that did not produce the work. A focused
correction/retest also uses an independent context.

A worker ends its turn with the handoff as its final answer and never enters an
open-ended wait after delivering it; whatever comes next is a fresh dispatch
carrying a continuation capsule (`## Continuation capsule` below). Any wait a
worker issues mid-task carries a bounded timeout well under the host tool cap.
Ending a turn to await one's own background child is that prohibited open-ended
wait: a completion notification cannot resume a worker whose turn has ended, so
capture the child's result in-turn before delivering the handoff.

Main consumes each dispatched worker's handoff once, from the host's own
delivery of it; the no-progress and terminal-failure route in
[`subagent-reporting.md`](subagent-reporting.md) interrupts that worker and
replaces it.

## Continuation capsule

The next step after a pause — user confirmation, external verdicts, interview
answers, an accepted finding, recovery — is a fresh worker of the same role with
the ordinary task brief plus a `Continuation capsule:` field carrying only what
that step consumes:

- objective and scope;
- the identity of the prior worker's artifacts — commit hashes, paths, receipt
  files, tokens — as path plus selector where a file holds them;
- the decision or answers that unblocked the step;
- the unresolved issue;
- the next action; and
- the skill to run.

Never the prior worker's transcript or reasoning.
