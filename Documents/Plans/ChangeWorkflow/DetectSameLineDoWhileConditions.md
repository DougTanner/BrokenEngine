<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-11T16:11:14.458Z","dependsOn":[]} -->
# Detect same-line do-while trailing conditions in style review

## Context

The user requested a deferred executable follow-up after observing
`} while (uiValue != 0);`: the trailing `while` must start on the next
physical line, and review scripts should automatically catch that form.
This applies Rule 2 of `Documents/C++StyleGuide.txt` with the user's explicit
placement instruction.

At commit `12bed6d5aa091cc8bafaa2b1e995c4736650ebfc`,
`Engine/Source/Frame/CellStaticData.cpp:21` in `WriteCompactInteger` contains
that form. Running `Find-SessionCandidates.ps1` against baseline
`c3b4501b9172d01346c7921caa12a76e812f23de`, narrowed with `-PathPrefix` to
that file, returned `pass`, zero candidates, and `style-rule-2: 0`.
The source-loop correction belongs to the originating change; this Plan
addresses the remaining automation gap.

The root cause is the `style-rule-2` entry in `$script:CandidatePatterns`
at `.agents/scripts/Find-SessionCandidates.ps1:57`: its closing-brace
alternative recognizes `else` and `catch`, without a trailing `while` form.
`Test-CandidatePattern` evaluates that table; the script itself owns the
detection rather than a wrapper or generator. The code-style-review worker's
step 6 currently assigns un-emitted Rule 2 forms to hand reading. This is a
scanner coverage improvement, not a change to reviewer conduct.

## Design

The author's recommendation is to extend the existing scanner with a narrow
Rule 2 candidate for a braced do-while whose closing brace and trailing
`while` share a physical line. Use the existing head-side file context and
candidate pipeline, retaining the scanner's read-only behavior. Recognize
both spaced and adjacent closing-brace/keyword forms. Account for comments,
character literals, ordinary and raw strings, including constructs opened on
an earlier line, and distinguish an ordinary while statement following a
completed block from a do-while trailing condition.

Make the requested trailing-while placement explicit once at the owning style
guide, as a narrow Rule 2 instruction or do-while example. Refer to that rule
from the worker rather than repeating the convention there.

Keep `style-rule-2` as the emitted kind and preserve the existing result
schema and session-added-line boundary. Align the worker's Rule 2 ownership
wording only where the new automatic coverage makes it inaccurate. Its
existing adjudication and meaning-preserving whitespace repair remain the
route for a confirmed candidate: place the trailing `while` on the next line
without changing the condition, terminator, or loop behavior.

## Critical files

- `.agents/scripts/Find-SessionCandidates.ps1` — candidate table,
  `Get-NewSideLine`, `Test-CandidatePattern`, and contextual detector dispatch.
- `.agents/skills/code-style-review/references/worker.md` — Rule 2 scope,
  candidate adjudication, and whitespace-repair ownership.
- `Documents/C++StyleGuide.txt` — Rule 2 trailing do-while placement.

## In scope

- Make only the requested do-while trailing-keyword convention explicit in
  the owning Rule 2 text or example.
- Detect this do-while form in changed C++ lines through the existing scanner,
  using surrounding file context where recognition requires it.
- Preserve whole-file scanning behavior for the same candidate kind.
- Update only the worker's Rule 2 scanner/manual-coverage description if
  necessary to describe the resulting coverage accurately.
- Verify positive and negative examples through the existing scanner and
  review machinery, with temporary verification inputs removed afterward.

## Out of scope

- Reformatting the originating source loop or a repository-wide style sweep.
- Banning empty `for` conditions, changing loop semantics, or expanding other
  style rules.
- A new formatter, parser framework, candidate schema, or automatic source
  mutation by the scanner.
- New unit tests, harness commands, build coordination, or claim operations.

## Acceptance criteria

- Rule 2 explicitly requires the do-while trailing `while` on the next line.
- A changed closing-brace line with the original do-while spelling emits a
  `style-rule-2` candidate at that line; adjacent and spaced spellings work.
- A correctly separated trailing `while` emits no candidate for this form.
- Comment and literal lookalikes, including multiline block comments and raw
  strings, emit no candidate for this form. An ordinary while following an
  unrelated completed block does not become a do-while finding.
- Session mode limits emitted rows to its selected changed C++ lines while
  permitting surrounding context reads; whole-file mode detects the same
  form. Existing Rule 2 candidates remain available.
- A style-review adjudication identifies the confirmed candidate and can
  repair it by a newline without changing loop tokens or behavior. Scanner
  execution leaves source files unchanged.

## Notes

Future implementation is Change Workflow Tier 2: behavior changes inside one
review-scanner subsystem, without determinism/CRC, wire, serialization,
replay, trust, threading, or independently owned integration exposure.
There are no dependencies or Coordination constraints with live Plans;
semantic searches found no Plan owning this scanner gap.

Plan alternatives, plan audit, and simplicity review apply to the future
behavior change. PowerShell and any changed skill prose require coherence
review; changing the skill package also triggers its validation and
progressive-disclosure review under the Change Workflow. No game build or
live harness evidence is needed for this script-only acceptance table.
No tooling-friction or context-efficiency provenance is required for this
ordinary user-requested automation improvement.
