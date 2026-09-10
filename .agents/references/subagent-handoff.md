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
Evidence: <existing or Temp/ path plus selector, or none>
Residuals: <actionable blocker or none>
```

That fenced form is the whole return — nothing precedes or follows it except the
extension fields, row forms, and typed blocks the assigned skill's `## Handoff`
declares — and `Status` carries exactly one of the three tokens alone on its
line. Every row is one line. Do not quote code and do not repeat a row from
another field. A field over 10 rows moves its full material to an existing file
or log, or to a `Temp/` file when no existing file holds it, and cites it under
`Evidence` as path plus selector. A selector into a Markdown file is a `##`
heading in it. The handoff itself still carries everything main needs; the file
is for the workers main dispatches next, cited to them as path plus selector.

A skill extends this form only by adding rows inside an existing field or by
declaring extra fields in its own `## Handoff` section, each one line or one row
per item, never a paragraph, and never by re-rendering the form itself.
`Build required` stays present and `Residuals` stays last. A target that
`/compile` builds in Release only carries `Release|x64` and no other
configuration/platform under `Build required`. Independent review and
verification use a context that did not produce the work. A focused
correction/retest also uses an independent context.

A worker ends its turn with the handoff as its final answer and never enters an
open-ended wait after delivering it; continuation goes through the host's resume
path. Any wait a worker issues mid-task carries a bounded timeout well under the
host tool cap. Ending a turn to await one's own background child is that
prohibited open-ended wait: a completion notification cannot resume a worker
whose turn has ended, so capture the child's result in-turn before delivering
the handoff.

Main consumes each dispatched worker's handoff once, from the host's own
delivery of it; it requests that worker's result again only through the
no-progress and terminal-failure route in
[`subagent-reporting.md`](subagent-reporting.md).
