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
Findings: <review roles and skill-declared verification failures only; one row each: ID Critical|High|Medium|Low path:line — claim — evidence>
Changed files: <one row each, path and region; or none>
Decisive checks: <command/read, observed verdict (exit code/counts if helpful)>
Build required: <exact targets, or none>
Evidence: <one row each, existing or Temp/ path plus selector — one-line description of what it holds; or none>
Residuals: <actionable blocker or none>
```

That fenced form is the whole return — nothing precedes or follows it except the
extension fields, row forms, and typed blocks the assigned skill's `## Handoff`
declares, and the final verdict line a Codex bridge run places after the
complete handoff ([`codex-bridge.md`](codex-bridge.md) step 2) —
and `Status` carries exactly one of the three tokens alone on its line. Every
row is one line. Do not quote code, repeat a field, or restate a plan
snapshot/execution card's mechanism or check/fixture text: cite its
worktree-relative path + only needed `##` selectors (e.g., `## Execution card`);
keep results, verdicts, new focus, post-plan deltas, and required fields
inline. The whole returned handoff stays under 4,000 characters; no skill sets
its own cap. Before returning, the worker estimates the drafted handoff's
character count; when it may reach the cap, the full material of the fields
that push it over moves to an existing file or log, or to a `Temp/` file when
no existing file holds it, with one `##` heading per section of that material,
and `Evidence` carries one table-of-contents row per section:
`<path> ## <heading> — <one-line description of what the section holds>`. A
selector into a Markdown file is a `##` heading in it. The handoff itself still
carries the decision-relevant summary of each moved field, such as the count in
`Changed files`, so main decides without opening the file; the file is for the
workers main dispatches next, cited to them as path plus selector, and for main
when a decision needs the detail. The report-file exemption for that file is in
root `AGENTS.md` `## Environment`. The Claude Code host still refuses a Claude
Code subagent's `Write` of a `.md` file whose name begins with `analysis`,
`report`, `findings`, or `summary` in any case (a host-internal list that can
change), so the brief or worker naming a `Temp/` file a handoff cites under
`Evidence` prefixes such a name, for example `temp-analysis.md`.

A skill extends this form only by adding rows inside an existing field or by
declaring extra fields in its own `## Handoff` section, each one line or one row
per item, never a paragraph, and never by re-rendering the form itself.
`Build required` stays present and `Residuals` stays last. A target that
`/compile` builds in Release only carries `Release|x64` and no other
configuration/platform under `Build required`. Independent review and
verification use a context that did not produce the work, and treat every
conclusion a supplied handoff states — `Self-audit resolved` results, a
`Decisive checks` row that reports a read rather than recorded command output,
a claim that a site is safe or unaffected — as a hypothesis to disprove from
code they read themselves, never as evidence; recorded command, build, and
harness output stays evidence, and the handoff's changed regions and focus
areas still bound the scope. When the brief states the change's tier, a changed
byte that reaches a surface that tier excludes in
[`risk-tiers.md`](risk-tiers.md) `### Risk tiers` is a tier-escalation finding
naming the higher tier's trigger. A focused correction/retest also uses an
independent context.

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

## Severity

Every `Findings` row grades its finding by the worst consequence it reaches; a
plan review grades the consequence of the change the plan would make.

- `Critical` — changed code, shader, or script is wrong on a reachable path: a
  crash, hang, wrong result, data loss or corruption, leak, determinism or CRC
  break, wire, serialization, or save mismatch, missing trust-boundary
  validation, or a caller, client/server, or CPU/GPU mirror left inconsistent;
  or a build, static check, or harness check fails.
- `High` — an approved plan requirement or acceptance criterion is missing, a
  change falls outside approved scope, backward-compatibility code is added, a
  `.agents/references/cpp-conventions.md` rule other than its reuse bullet or a
  GLSL layout or binding contract is broken, or an instruction or script rule
  makes an agent take a wrong action or cannot be followed as written.
- `Medium` — words that do not match code (a comment class
  `comment-classes.md` rates `Medium`, a stale `AGENTS.md`), a near-copy of an
  existing helper under the `cpp-conventions.md` reuse bullet,
  over-engineering or dead code left behind, or a progressive-disclosure
  violation.
- `Low` — a nit: wording, size, or a naming preference.

What each level triggers is the step script's round close
(`.agents/scripts/Get-ChangeWorkflowSteps.ps1` header).

## Continuation capsule

After a pause (confirmation, external verdicts, interview answers, accepted
finding, recovery), dispatch a fresh same-role worker with the task brief plus
`Continuation capsule:` carrying only what the step consumes:

- objective and scope;
- prior artifacts (commits, paths, receipts, tokens) by path + selector when
  file-backed; saved review outputs per `subagent-reporting.md` `## Task brief`;
- unblocking decision or answers;
- unresolved issues;
- next action and skill to run.

Never the prior worker's transcript or reasoning.
