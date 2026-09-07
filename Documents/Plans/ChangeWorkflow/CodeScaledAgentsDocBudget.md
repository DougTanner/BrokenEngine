<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T18:05:14.245Z","dependsOn":[]} -->
# Code-scaled AGENTS.md documentation budget

## Context

`.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1` assigns
each chain document an advisory size target from structure alone:
`$script:LeafTokenTarget` 2,000 `bt-token-v1` for a leaf,
`$script:HubTokenTarget` 4,000 for a hub, and `$script:RootHubTokenTarget` 8,000
for the repository-root `AGENTS.md`, where `Test-HubDocument` calls a document a
hub whenever any other `AGENTS.md` lives below it and the `sizes` result loop
picks the target from that classification (line numbers as of `babc93b0`:
constants 38-44, `Test-HubDocument` 285-299, loop 390-402).
Structure is the wrong axis: it says nothing about how much code the document
governs, and hub size does not track child count either.

Measurement method: at commit `fd6c7dc842aa888b5555c6eab773306c12a11039`, the
`bt-token-v1` count from `.agents/scripts/Measure-Tokens.ps1` was taken over
each `AGENTS.md` and over the `.cpp`, `.h`, `.comp`, `.vert`, and `.frag` files
directly in that document's own folder (non-recursive, comments included). The
measured population is every tracked `AGENTS.md` outside `ThirdParty/` and
`Documents/` — 76 documents at that commit. The repository has 80 tracked
non-`ThirdParty` `AGENTS.md`; the four under `Documents/` are excluded because
they govern no code. All 76 were measured; they total about 84,000 doc tokens
against
1,084,936 code tokens repository-wide excluding `ThirdParty/`. The rows this
Plan's argument relies on:

| document folder | doc tokens | same-folder code tokens | direct child `AGENTS.md` | current classification |
| --- | --- | --- | --- | --- |
| `Engine/Source/Graphics/Managers` | 3,979 | 114,058 | 0 | leaf (2,000) |
| `Engine/Source/Memory` | 676 | 2,110 | 0 | leaf (2,000) |
| `Engine/Source/Frame/Collections` | 1,784 | 12,481 | 11 | hub (4,000) |
| `DataPacker/Source` | 2,884 | 23,507 | 1 | hub (4,000) |
| `Engine/Source/Input` | 1,849 | 5,358 | 0 | leaf (2,000) |
| `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026` | 2,003 | 0 | 0 | leaf (2,000) |
| repository totals (76 documents) | ~84,000 | 1,084,936 | — | — |

These numbers are only the calibration starting point: the executing session
re-measures with the new comment-free script, and its measurement governs.

Key observations:

- `Engine/Source/Graphics/Managers` is the largest code folder in the repository
  (114,058 same-folder code tokens with comments) and is classified `leaf` at
  2,000, while `Engine/Source/Memory` (2,110 code tokens) gets the same 2,000.
- Hub size does not track children: the `Engine/Source/Frame/Collections` hub has
  11 child documents at 1,784 doc tokens; the `DataPacker/Source` hub has 1 child
  at 2,884.
- The whole-repository ratio is about 78 doc tokens per 1,000 code tokens, but
  the per-document ratio varies enormously, because a small folder still needs a
  content floor. Among folders with at least 1,000 same-folder code tokens the
  ratio spans 21 (`Engine/Source/Graphics/Objects`) to 495
  (`Projects/BrokenEngineSandbox/Source/Profile`); below that cutoff it runs far
  higher still, up to 15,807.7 for
  `Projects/BrokenEngineSandbox/Data/Shaders`, whose folder holds only 26 code
  tokens.

No existing script measures code size per folder with comments excluded, so the
only measurement available to a budget today is the comment-inclusive
`bt-token-v1` count from `.agents/scripts/Measure-Tokens.ps1`, which rewards
heavily commented code with a larger documentation allowance.

This is a measurement-and-rule change only. No document is trimmed by it.

## Design

### Budget formula (user-decided)

Each `AGENTS.md` gets an advisory budget:

`budget = floor + slope x (comment-free code tokens in the same folder,
non-recursive, in thousands) + childAllowance x (number of direct child
directories that carry their own AGENTS.md)`

- `floor` = 1,000 and `childAllowance` = 150 are fixed.
- The slope term uses fractional thousands: code tokens divided by 1,000 without
  truncation, so 29,401 code tokens contribute `slope x 29.401`. The reported
  budget is rounded to the nearest integer token.
- The subtree is never counted; counting it would balloon every hub.
- The repository-root `AGENTS.md` keeps its fixed 8,000 budget and takes no
  formula terms.
- A code-free document (zero comment-free code tokens in its own folder) gets
  floor plus the child term only.

`slope` was provisionally fitted at 25 per 1,000 code tokens against counts that
still included comments, so it must be recalibrated once the comment-free
measurement exists. The recalibration is deterministic and user-decided: after
the new script exists, re-measure the same population — every tracked
`AGENTS.md` outside `ThirdParty/` and `Documents/`, 76 documents at the
calibration commit — hold `floor` and `childAllowance` fixed, and set `slope` to
the smallest multiple of 5 at which
`Engine/Source/Graphics/Managers/AGENTS.md` — trimmed to operative rules only at
3,979 doc tokens, on the repository's largest code folder — sits at or under its
budget. Record the
chosen slope in the script and in this Plan's acceptance evidence. A naive
comment strip of that folder gives about 95,093 comment-free tokens, which would
select slope 35 (30 yields a 3,853 budget, below the document's 3,979; 35 yields
4,328); the implementing session's measurement from the real script governs, not
that approximation.

### Code-free override list

`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` is the one
clear false positive in the evidence: 2,003 doc tokens governing project
membership rules for code that lives elsewhere, so a floor-only budget would
flag it forever. The author recommends a small documented per-path override list
of `document path -> budget` in `Get-AffectedAgentsDocs.ps1` beside the other
budget constants, so the measurement script stays generic, with that one path at
its current 2,000 as the only initial entry.

### New measurement script

A new PowerShell 7 script measures comment-free code tokens per folder. The
author recommends `.agents/scripts/Measure-CodeTokens.ps1`, taking `-Path` as
one or more folder paths and emitting one JSON object with one entry per
requested folder (folder path, code file count, comment-free `bt-token-v1`
tokens), in the requested order. It is deterministic: same inputs, same numbers.

- Counted extensions (user-decided): `.cpp .h .hpp .inl .c` and GLSL `.comp
  .vert .frag .geom .tesc .tese .glsl`. Non-recursive: only code files directly
  in the named folder.
- Comments are removed before tokenizing. A single-pass scanner tracks normal
  code, `//` line comments, `/* */` block comments, `"` string literals with
  backslash escapes, `'` character literals with backslash escapes, and C++ raw
  string literals `R"delim( ... )delim"`. A `//` or `/*` inside any literal is
  literal text, not a comment.
- Removal rule: a line comment is removed up to but not including its newline; a
  block comment is replaced by a single space. Nothing else is normalized — no
  whitespace collapsing, no blank-line removal.
- A `//` comment ends at the newline even when the line ends with a backslash
  continuation. That C++ corner is a documented simplification chosen for a
  deterministic single-pass scan; it does not occur in this repository.
- An unterminated block comment or literal at end of file consumes the rest of
  the file, matching how the compiler would read it.

### Sharing the tokenizer

The `bt-token-v1` estimate must not be reimplemented or wrapped (root
`AGENTS.md` bundled-script rule). The author recommends factoring it into
`.agents/scripts/AgentScriptCommon.psm1`, which already carries the
`Agent`-prefixed shared helpers and is already imported by
`Get-AffectedAgentsDocs.ps1`: add `Get-AgentNormalizedText` (BOM strip, strict
UTF-8 decode, and CRLF/CR to LF normalization from a byte array) and
`Measure-AgentTokenCount` (normalized UTF-8 byte count divided by four, rounded
up), export both, and have
both `Measure-Tokens.ps1` and the new script call them. The strict decode is the
one `Measure-Tokens.ps1` performs today with
`[System.Text.UTF8Encoding]::new($false, $true)` (lines 32, 49), which throws on
invalid bytes; the shared helper keeps that behavior, so an invalid-byte file
still fails instead of being silently counted. `Measure-Tokens.ps1`
keeps its parameters, output shape, and exit codes exactly as they are; only its
inline normalization and arithmetic move into the module. Adding a second
tokenizer module is rejected as unnecessary.

### Consumption and reporting

`Get-AffectedAgentsDocs.ps1` replaces `$script:LeafTokenTarget` and
`$script:HubTokenTarget` with the formula constants, keeps
`$script:RootHubTokenTarget`, and computes each chain document's budget in the
`sizes` result loop. It obtains code tokens for each document's own
folder from the new script through a single delegated call, in the same style as
the existing `Measure-DocumentToken` helper, and it counts direct
child `AGENTS.md` documents from the `$script:AgentsPaths` set it already builds.

Each `sizes` item reports the document path, its comment-free code tokens, its
direct child-document count, its computed budget, its doc tokens, and an
over/under verdict; the author recommends also reporting which rule produced the
budget (formula, root, or override) so a reader can see why. The `hub`/`leaf`
`kind` and fixed `target` fields go away. `Test-HubDocument` has no remaining
caller after that and is deleted; `Get-HubCandidate`, which feeds `chains`, is
untouched.

Chain totals stay exactly as they are (`$script:ChainTokenTarget` 15,000 and
`$script:ChainTokenWarning` 20,000), as does
the `broken-engine-affected-agents-docs/v1` schema name, its output cap, and its
error channels.

### Guidance text and trimming policy (user-decided)

Every place that states the 2,000/4,000 hub/leaf targets, or defines the hub/leaf
classification they rest on, states the new rule instead; the classification is
removed, not kept alongside. Known
places are `.agents/skills/update-claude-docs/references/audit-mode.md` line 18
(Conciseness rubric row), `.agents/skills/update-claude-docs/references/worker.md`
lines 33-40 (the `sizes` result contract),
`.agents/skills/progressive-disclosure-review/references/worker.md` line 58
(which defers "leaf/hub size targets" to `/update-claude-docs`), and
`.agents/skills/update-claude-docs/references/content-rules.md` line 7, the
numberless hub/leaf definition reworded into its current form by primary commit
`babc93b0`. The
implementing session searches the repository for any other statement of those
numbers or of the classification, and updates what it finds inside the two skill
packages.

Budgets stay advisory exactly as today: a size verdict never authorizes trimming
on its own. The guidance states the trimming policy: when a document is over
budget, trim least-value content first — file and member inventories, repeated
introductions, navigation pointers, restated mechanics that already live in code
comments or references, and prose whose operative rule is already retained. A
document whose remaining content is all operative rules and contracts earns its
size, is kept as concise as possible, and its excess is reported as advisory
rather than cut.

### Change Workflow tier

Tier 2, scoped tool behavior: this changes the `update-claude-docs` skill
package — its size rule and its discovery script's output contract — the size
statement in the `progressive-disclosure-review` skill package, and the shared
`.agents/scripts/` module and its new companion script. All of it is agent
tooling, with no exposure to determinism/CRC, wire or protocol formats,
serialization or data layout,
save/replay compatibility, threading, or trust boundaries. The
`/plan-simplicity-review` trigger "when a skill edit is behavior" fires, because
the reported verdicts change.

## Critical files

- `.agents/scripts/Measure-CodeTokens.ps1` (new)
- `.agents/scripts/AgentScriptCommon.psm1`
- `.agents/scripts/Measure-Tokens.ps1`
- `.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1`
- `.agents/skills/update-claude-docs/references/worker.md`
- `.agents/skills/update-claude-docs/references/audit-mode.md`
- `.agents/skills/update-claude-docs/references/content-rules.md`
- `.agents/skills/progressive-disclosure-review/references/worker.md`

## In scope

- Add `Get-AgentNormalizedText` and `Measure-AgentTokenCount` to
  `.agents/scripts/AgentScriptCommon.psm1` and extend its `Export-ModuleMember`.
- Replace the inline BOM/CRLF normalization and `($byteCount + 3) -shr 2`
  arithmetic in `.agents/scripts/Measure-Tokens.ps1` (lines 32-33, 44-50, 79-80)
  with calls to those exports, leaving its parameters, JSON shape, and exit codes
  unchanged.
- Add `.agents/scripts/Measure-CodeTokens.ps1` implementing the extension list,
  the non-recursive per-folder walk, the comment-and-literal scanner, and the
  JSON result described in `## Design`.
- In `.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1`:
  replace `$script:LeafTokenTarget`/`$script:HubTokenTarget` with the `floor`,
  `slope`, `childAllowance`, and per-path override constants; keep
  `$script:RootHubTokenTarget`; add the delegated code-token helper beside
  `Measure-DocumentToken`; compute budget, code tokens, child count, and verdict
  in the `sizes` result loop; delete the now-unused `Test-HubDocument` (line
  numbers as of `babc93b0`: constants 38-44, `Measure-DocumentToken` 304-335,
  loop 390-402, `Test-HubDocument` 285-299).
- Record the recalibrated `slope` as a named constant in
  `Get-AffectedAgentsDocs.ps1`.
- Restate the size rule and the trimming policy in
  `.agents/skills/update-claude-docs/references/audit-mode.md` line 18,
  `.agents/skills/update-claude-docs/references/worker.md` lines 33-40, and
  `.agents/skills/progressive-disclosure-review/references/worker.md` line 58,
  removing the 2,000/4,000 hub/leaf targets rather than keeping them alongside.
- Replace the hub/leaf definition sentence at
  `.agents/skills/update-claude-docs/references/content-rules.md` line 7 with the
  code-scaled budget rule wording; the edit is bounded to that sentence.
- Update any further statement of the 2,000/4,000 targets, or of the hub/leaf
  classification, found by a repository search inside the `update-claude-docs`
  and `progressive-disclosure-review` skill packages.

## Out of scope

- Trimming, splitting, or rewriting any `AGENTS.md` the new rule flags, including
  the ones named in `## Notes`.
- Changing the chain totals (15,000 target, 20,000 warning), the root
  `AGENTS.md` 8,000 budget, the `bt-token-v1` unit, or the numbers
  `Measure-Tokens.ps1` returns.
- Changing the `broken-engine-affected-agents-docs/v1` schema name, its output
  cap, its error codes, `chains`, `hubCandidates`, or the stub-pairing sweep,
  including `$script:StubSweepExclusions`.
- Other skills' unrelated size thresholds: `/reduce-file`,
  `/external-refactor-clean`, and the 10,000-token skill-body threshold in
  `/progressive-disclosure-review`.
- Editing any other Plan document under `Documents/Plans/`.
- Adding unit tests, a tracked fixture file, or a CI check.
- C++, GLSL, project membership, or engine runtime behavior.

## Invariants

- Budgets stay advisory: no verdict from this script authorizes trimming a
  document, and pre-existing excess in untouched prose is reported, not cut.
- Both scripts stay deterministic: identical inputs produce identical numbers on
  any machine, with no dependence on file order, locale, or line endings.
- The `bt-token-v1` unit is unchanged, and `Measure-Tokens.ps1` returns exactly
  the numbers it returns today for the same inputs.
- Chain totals and the root `AGENTS.md` budget are unchanged.
- Every remaining size statement in the two skill packages describes one rule;
  no document is measured against both the old classification and the new budget.
- `Get-AffectedAgentsDocs.ps1` keeps its exit-code and envelope contract, so a
  failure in the new measurement maps into its existing error channel rather than
  raising.

## Acceptance criteria

- A fixture written under `Temp/` containing `//` inside a string literal, `/*`
  inside a raw string literal, a real `/* */` block comment, and a real `//` line
  comment returns the token count computed by hand from the retained bytes; the
  fixture and the expected number are recorded in the acceptance evidence and the
  fixture is not tracked.
- `Measure-CodeTokens.ps1` on `Engine/Source/Graphics` reports only that folder's
  14 direct code files, not the 4 child folders' files.
- `Measure-Tokens.ps1` returns byte-identical numbers before and after the module
  refactor for a recorded sample of at least one Markdown document and one code
  file.
- The recalibrated `slope` is recorded in the script and in the acceptance
  evidence, together with the re-measured `Engine/Source/Graphics/Managers` code
  tokens and budget showing that document at or under budget and that the next
  smaller multiple of 5 would not be.
- `Get-AffectedAgentsDocs.ps1` on a changed path under `Engine/Source/Graphics`
  returns `status=pass` and reports, for every chain document, its comment-free
  code tokens, direct child-document count, computed budget, doc tokens, and
  over/under verdict, with no `hub`/`leaf` `kind` or fixed `target` field.
- `Get-AffectedAgentsDocs.ps1` on a changed path under
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026` reports that
  document's budget as 2,000 from the override list.
- A repository search finds no remaining hub/leaf classification outside
  `Documents/Plans/` — neither the 2,000 leaf or 4,000 hub target nor a
  numberless definition of hub and leaf — and every surviving size statement in
  the two skill packages carries the trimming policy.
- `/validate-skill` passes for `update-claude-docs` and
  `progressive-disclosure-review`, and the applicable static checks in
  `.agents/references/static-checks.md` pass for the changed PowerShell and
  Markdown.

## Coordination

- `Engine/Source/File/AGENTS.md` was condensed by a completed Plan in primary
  commit `0dbd60df`, so no coordination remains for it.

## Notes

- At calibration commit `fd6c7dc8`, under the provisional constants, 14 of the 76
  measured documents exceeded budget, against 5 under today's fixed targets — 13
  and 4 respectively at primary tip `ffe86bb6`, after
  `Engine/Source/File/AGENTS.md` was condensed to 1,796 doc tokens, under both
  its provisional budget of about 2,064 and the fixed 2,000 leaf target.
  Trimming them is out of scope here; the first `/update-claude-docs` run that
  touches one reports the excess as advisory.
- `Documents/AGENTS.md` and the three other documents under `Documents/` sit
  outside the measured population, so no budget applies to them. Of those,
  `Documents/Plans/AGENTS.md` is worth knowing about anyway: it measures 1,073
  doc tokens and has no same-folder code, so were it ever brought into the
  population it would be a marginal advisory overage against a floor-only 1,000
  on an operative-rules document. This Plan neither trims it nor adds an
  override for it.
- `Common/AGENTS.md` is in the measured population and gets an ordinary formula
  budget: 1,994 doc tokens against 26 same-folder code files carrying 29,401
  comment-inclusive code tokens and 3 direct child `AGENTS.md`, which puts its
  budget comfortably above its size.
- The final slope depends on the real scanner's output; the naive-strip estimate
  in `## Design` exists only so a reviewer can tell whether the implementing
  session's number is in the expected range.
