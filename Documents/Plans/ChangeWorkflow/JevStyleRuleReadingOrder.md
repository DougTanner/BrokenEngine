<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-19T13:44:03.977Z","dependsOn":[]} -->
# Jev reading order for the hand-read rules in /code-style-review

## Context

`/code-style-review` hand-reads every session-changed C++ range for the rules
its candidate scanner emits nothing for: 3, 14, 16 with its vector `.at()`
clause, 21, 49, 51, 56, 61, 62, and the "always write `std::`" half of 41
(`.agents/skills/code-style-review/references/worker.md:36-42`). The scanner
`.agents/scripts/Find-SessionCandidates.ps1` covers the other rules as
`style-rule-<n>` kinds (`Find-SessionCandidates.ps1:36-55`), one regex per
line attributed first-match (`Find-SessionCandidates.ps1:173-180`), and its
step-9 adjudication consumes them (`worker.md:62-69`).

`Documents/Investigations/JevStyleRuleJudgment.md` measured, over the 41
hand-labelled blocks in
`.agents/skills/code-style-review/references/style-rule-judgment/cases.json`,
that one Jev request per function with one `noul` per rule meets its success
bar for rules 14, 16, 21, 41, 49, 51, and 62 at probability 0.5 (every planted
violation flagged, at most one clean block flagged per rule); that rule 3
needs 0.9; that rule 56 must be asked per identifier over the function's name
list; and that rule 61 is undecidable for the model on Allman braces but is a
two-line scanner check. The measurement script
`.agents/scripts/Test-StyleRuleJudgment.ps1` carries the question texts and
the corpus mode, and `Invoke-Jev.ps1` is the only network caller
(`JevDecisionModelWorkflowUses.md`, `## The caller`). Neither the worker nor
the scanner uses either yet.

## Design

1. Session mode for `Test-StyleRuleJudgment.ps1`. `-CasesPath` stops being
   mandatory; the script takes either `-CasesPath` or the session set
   `-RepositoryRoot`, `-Baseline`, optional `-Head`, and the
   `-IncludeUntracked` switch, with exactly the meanings
   `Find-SessionCandidates.ps1` gives them, and reports `error`
   `input.mode` when both or neither are supplied. The inventory writes its
   document to the raw console handle (`Get-SessionChangeInventory.ps1:82`)
   and has no `-OutputPath`, so it must run as a child process with
   redirected stdout, as the scanner does: `Invoke-CandidateProcess`
   (`Find-SessionCandidates.ps1:83-108`) moves into `AgentScriptCommon.psm1`
   as the exported `Invoke-AgentProcess`, unchanged in body, and the scanner
   calls it from there; the export's comment records that the other scripts
   with their own `ProcessStartInfo` runners keep them and convert only when
   next changed, so the parallel copies are deliberate. The session mode
   imports the module, resolves the root with `Get-AgentCanonicalPath`,
   prepends `-C <root> --no-pager` to every `Invoke-AgentGit` argument list
   (the module helper runs in the current directory,
   `AgentScriptCommon.psm1:14-18`), and runs
   `Get-SessionChangeInventory.ps1 -Regions` through the process helper with
   `GIT_OPTIONAL_LOCKS=0` in the child environment, forwarding `-Head`
   and, for `-IncludeUntracked`, the untracked list from `Invoke-AgentGit
   @('ls-files', '--others', '--exclude-standard')` (its one remaining
   `Invoke-AgentGit` use) as the inventory's
   comma-separated `-IncludeUntracked`, and treats any inventory result other
   than `status` `pass` with `truncated` false as `blocked` with the
   inventory's own code. It takes the regions of `cpp` and
   `dual-language-header` paths and reads each path's head-side text once,
   from `git show <headSha>:<path>` run through `Invoke-AgentProcess` (so its
   UTF-8 stdout redirection applies and a non-ASCII literal reaches the
   service intact) when the inventory reports a `headSha`, and from the file
   as UTF-8 otherwise.

   A block is delimited by the repository's enforced Allman-plus-tab shape,
   not by a brace parser, because a wrong boundary costs only a worse reading
   order. From a region's head-side start line, first look down: when the
   next column-0 `{` comes before any column-0 `}` and every line between is
   non-blank, the region starts on a declaration (a changed signature) and
   that brace opens the block. Otherwise walk up to the nearest line whose
   trimmed text is `{` at column 0 and whose preceding non-blank line does
   not start with `namespace`. Either way the block runs from the non-blank
   line before that `{` (the declaration) through the first following line
   whose trimmed text is `}` or `};` at column 0. A class or struct body is
   one block; its members are never enumerated separately. When no such `{`
   is found before a column-0 `}` in both directions, the region sits at
   namespace scope and its own lines are the block. Blocks are deduplicated
   by path and start line. A block is judged with the corpus mode's two
   requests, and a session result row carries only `path`, `line` (the
   declaration line), `endLine`, and `flagged` (each entry a rule and its
   probability, plus the flagged names for rule 56), in flagged-first order
   by each block's highest flagged probability; the per-name and per-rule
   probability lists stay in corpus mode. The inventory gate above bounds
   the rows to at most 400 regions, and a row without those lists is a few
   hundred bytes, so the document stays inside the 131072-byte budget the
   sibling scripts keep without a cap of its own. The corpus mode, its
   `measurement` object, and `blocked` handling are unchanged; the script
   writes no file inside the worktree except at `-OutputPath`.
2. A `style-rule-61` kind in `Find-SessionCandidates.ps1`. The kinds table is
   single-line and first-match, so the two-line check runs before
   `Test-CandidatePattern` at its caller (`Find-SessionCandidates.ps1:197-203`)
   and short-circuits the line: the added line matches `^\s*(?:if|else)\b`
   (rule 61 covers `if` and `else` bodies only, `Documents/C++StyleGuide.txt:288`),
   is not `else if`, and either carries a statement on the same line (after
   the `if` condition's closing parenthesis, or after the `else` keyword) or
   is followed by a non-blank head-side line
   that does not start with `{`, `&&`, or `||` (an Allman brace or a rule 51
   leading-operator continuation; a wrap with the operator at the end of the
   line is itself a rule 51 violation, so a hit there is a legitimate
   candidate for step 9). The hit row is the added line
   with kind `style-rule-61`, and `counts` gains `style-rule-61` by an
   explicit entry beside the table loop (`Find-SessionCandidates.ps1:213-214`).
   The scanner stays candidates-only and read-only (its header, lines 1-6).
3. Worker wiring in `.agents/skills/code-style-review/references/worker.md`.
   Step 6 drops rule 61 from the hand-read list, because the scanner now
   emits it for step 9. A new step after step 8, for a session-changed scope
   only (a caller-supplied cleanup scope has no baseline and skips it), runs
   `pwsh -NoProfile -File .agents/scripts/Test-StyleRuleJudgment.ps1
   -RepositoryRoot <absolute repository toplevel> -Baseline <full SHA>`, with
   the same optional `-Head` and `-IncludeUntracked` as step 7, and states:
   only `status` `ok` is usable; `blocked` means the reading order is
   unavailable and step 6 proceeds exactly as before; `error` `jev.partial`
   keeps the answered blocks. Step 6's hand read of rules 3, 14, 16, 21, 41,
   49, 51, 56, and 62 then
   reads the flagged blocks first, in the result's order, and still reads
   every other selected range, because this Plan is reading-order only under
   the series' no-gate rule (`JevDecisionModelWorkflowUses.md`, `## Decisions
   every Plan in the series shares`, item 4). The thresholds are the script's
   defaults and are not restated. The step records, for each flagged block,
   whether the hand read agreed, in the handoff's `Judgment` field; the
   worker writes no documentation, and no log accumulates across sessions:
   the second measurement the Investigation asks for is a rerun of the
   session mode over recent landed commits, which needs no rows from past
   reviews. `## Rules` gains two lines: the corpus at
   `references/style-rule-judgment/cases.json` is the measurement the
   thresholds come from and `-CasesPath` re-measures after a question
   change; and the session mode sends every changed block's source text to
   the TypeSafe service, so a clone without `TYPESAFE_API_KEY` sends nothing
   and loses only the reading order.
4. `.agents/skills/code-style-review/SKILL.md` gains one handoff extension
   field before `Residuals`: `Judgment` — one row per block the script
   flagged: file:line, the rules flagged with their probabilities, and
   `agreed` or `disagreed` from the hand read; or `not run` with the script's
   `code` when the result was `blocked`; or none. The corpus folder is linked
   from `.agents/skills/code-style-review/references/worker.md` only, per
   `.agents/references/skill-skeleton.md` `## Section placement`; `SKILL.md`
   `## References` is unchanged.
5. `Documents/Investigations/JevStyleRuleJudgment.md` records that the Plan
   landed, and `JevDecisionModelWorkflowUses.md`'s candidate row moves the
   next test to "session mode over the last ten landed C++ commits, flagged
   blocks hand-labelled for rules 3 and 56".

## Critical files

- `.agents/scripts/Test-StyleRuleJudgment.ps1` — the corpus-mode script that
  gains the session mode, the block enumerator, and the session row output.
- `.agents/scripts/AgentScriptCommon.psm1:7-37` — `Get-AgentCanonicalPath`
  and `Invoke-AgentGit`, which the session mode imports, and the export list
  `Invoke-AgentProcess` joins.
- `.agents/scripts/Find-SessionCandidates.ps1:19-55,83-108,132-151,153-171,173-180,197-203,212-214` —
  the caps and kinds table, the process helper that moves to the module, the
  inventory run and added-line walk the session mode parallels, the
  first-match pattern test the rule 61 check precedes, and the per-kind
  counts the new kind joins.
- `.agents/scripts/Get-SessionChangeInventory.ps1:82` — the raw console
  write that forces the child-process run.
- `.agents/scripts/Get-SessionChangeInventory.ps1:498-543` — the `regions`
  rows (`path`, `startLine`, `endLine`) the enumerator consumes.
- `.agents/scripts/Test-CitationSupport.ps1:178-190` — the in-process
  `Invoke-Jev.ps1` call and `blocked` handling the session mode keeps.
- `.agents/skills/code-style-review/references/worker.md:8-22,36-69,100-121` —
  the scope and inventory steps that decide session-changed versus
  caller-supplied, the hand-read and scanner steps, and the rules block.
- `.agents/skills/code-style-review/SKILL.md:34-51` — the handoff fields.
- `.agents/skills/code-style-review/references/style-rule-judgment/cases.json`
  — the labelled corpus the acceptance re-measurement runs over.
- `Documents/Investigations/JevStyleRuleJudgment.md` and
  `Documents/Investigations/JevDecisionModelWorkflowUses.md:95,137-140` — the
  candidate document and its overview row and recommendation item.

## In scope

- `.agents/scripts/Test-StyleRuleJudgment.ps1`: making `-CasesPath` optional
  and adding the `-RepositoryRoot`, `-Baseline`, `-Head`, and
  `-IncludeUntracked` session mode with its mode check; the module import,
  child-process inventory run, and head-side text read; the Allman-shape
  block enumerator; the `path`/`line`/`endLine`/`flagged` session rows and
  flagged-first ordering; and the header comment describing both modes.
- `.agents/scripts/AgentScriptCommon.psm1`: `Invoke-AgentProcess`, the
  process helper moved from the scanner, added to the export list.
- `.agents/scripts/Find-SessionCandidates.ps1`: replacing its
  `Invoke-CandidateProcess` with the module's `Invoke-AgentProcess`; the
  `style-rule-61` two-line check before the pattern test at its caller, its
  `counts` entry, and the kinds-table comment naming it.
- `.agents/skills/code-style-review/references/worker.md`: step 6's rule list
  without 61 and with the flagged-first reading order; the new
  session-scope-only judgment step with its usability, agreement-recording,
  and fall-through statements; and the `## Rules` lines on the corpus and on
  what the session mode sends off the machine.
- `.agents/skills/code-style-review/SKILL.md`: the `Judgment` handoff field.
- `Documents/Investigations/JevStyleRuleJudgment.md` and
  `Documents/Investigations/JevDecisionModelWorkflowUses.md`: the landed
  status and the next-test wording.

## Out of scope

- Any gate: the script never hides a block from the hand read, skips a
  rule, or changes what the worker auto-fixes or routes; reading order is the
  whole effect until a second measurement on real changes.
- `Invoke-Jev.ps1`, the question texts, the thresholds, and the corpus: they
  are the measured artifacts; a question change re-measures in corpus mode
  first and is its own change.
- A brace-matching parser, a per-block line cap, a row cap, a `skipped`
  list, or `counts` in the session result: reading order tolerates a wrong
  boundary, the per-request failure path already isolates an oversized
  block, and the inventory gate already bounds the rows.
- `Documents/C++StyleGuide.txt`, including whether `Diff` or `ack` become
  rule 56 exceptions (the Investigation's open user decision).
- The `LOG` level `choice`, the Rule 2 forms step 6 hand-reads, every rule
  the scanner already covers, and every other candidate in
  `JevDecisionModelWorkflowUses.md`.
- `/comment-review`, `/repo-code-review`, the finding, fix, or rename steps
  of `/code-style-review` (worker steps 10-16), and the worker's step 2
  inventory command, which keeps its baseline-to-working-tree form.
- A cross-session agreement log in any tracked or ignored file: the
  `Judgment` field is the only record, so no review edits a document outside
  its change.
- `Get-SessionChangeInventory.ps1` and the inventory's region schema, and
  any change to `AgentScriptCommon.psm1` beyond the moved process helper.
- Any C++, shader, project, or `AGENTS.md` file.

## Risk tier and invariants

Expected Change Workflow Tier 2: scoped behavior of one skill's tooling, no
C++, no landing-gate path. Triggers: a new script mode and a new scanner kind
feed a review worker's reading order, and the script now sends changed
source text to an external service.

Preserve these invariants:

- Both scripts stay read-only and candidates-only: no file is edited, nothing
  is written inside the worktree except an explicit `-OutputPath`, and
  `GIT_OPTIONAL_LOCKS=0` holds on every Git call the session mode makes.
- What leaves the machine is exactly the text of each changed block and its
  identifier list, sent only when `TYPESAFE_API_KEY` is set; a missing key,
  an unreachable service, or an unusable inventory is `blocked`, and the
  worker then behaves exactly as it does today.
- No handoff row, fix, or routing decision cites a probability as evidence;
  the hand read against the guide remains the evidence of record, and a
  `Judgment` row records agreement, never a verdict.
- Response pairing holds in both modes: block `i` reads responses `2i` and
  `2i + 1`, and a block with no identifier still sends its placeholder name
  request.
- The `style-rule-61` check never fires on an Allman brace, an `else if`, or
  a rule 51 leading-operator continuation, fires on no `for` or `while`
  line, and a line it fires on is never re-attributed to another kind.
- Corpus mode output for `measurement` is unchanged, so the Investigation's
  numbers remain reproducible.

## Acceptance criteria

- Corpus re-measurement: `pwsh -NoProfile -File
  .agents/scripts/Test-StyleRuleJudgment.ps1 -CasesPath
  .agents/skills/code-style-review/references/style-rule-judgment/cases.json`
  returns `ok` with `measurement` showing zero `missed` for rules 14, 16, 21,
  41, 49, 51, 62, and 3, at most one `falseFlag` for each of those, and for
  rule 56 at most one `missed` and at most two `falseFlag`.
- Session-mode enumeration over a landed commit whose diff changes at least
  three C++ functions across two files, one of them a header, with
  `-Baseline` its parent and `-Head` the commit, and whose diff includes one
  changed function signature line: the result is `ok`, every changed free
  function or member definition in a `.cpp`, the signature-changed one
  included, appears exactly once
  as a block whose `line` is its declaration line and whose `endLine` is its
  column-0 closing brace, a changed member inside a header class body appears
  inside that class's single block, a changed namespace-scope declaration
  appears as its own block, and blocks are in flagged-first order.
- Session-mode fall-through: the same run with `TYPESAFE_API_KEY` unset
  returns `blocked` with code `jev.key-missing`, exit 2, and leaves the
  worktree unchanged (`git status --porcelain` identical before and after).
- Scanner: with an untracked scratch file `Engine/Source/StyleRule61Scratch.cpp`
  holding one unbraced `if` on two lines, one `if (x) return;` on one line,
  one unbraced `else` on two lines, one `else DoThing();` on one line, one
  unbraced `for`, one `else if` with braces, one Allman `if`, one
  `if (fRange > 1.0f)` unbraced, and one `if` condition split with a leading
  `&&` on its second line,
  `pwsh -NoProfile -File .agents/scripts/Find-SessionCandidates.ps1
  -RepositoryRoot <absolute toplevel> -Baseline <full SHA> -IncludeUntracked`
  emits exactly five `style-rule-61` hits on that path (the two-line `if`,
  the one-line `if`, both `else` forms, and the `fRange` line, which is not
  attributed `style-rule-27`), none on the `for`, and `counts.style-rule-61`
  counts them; the scratch file is deleted afterwards.
- Worker dry run: a `mechanic` runs `/code-style-review` over this Plan's own
  session, in which a scratch C++ edit to one engine function is made and
  reverted after the run, and returns a handoff whose `Judgment` field lists
  that block with probabilities and an `agreed`/`disagreed` value, and whose
  other fields are unchanged in form from today's `SKILL.md`.
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  a healthy tree after the Plan file is removed at completion.
- The changed-path diff contains exactly the `## In scope` paths plus the
  removal of this Plan file.

## Notes

The corpus mode, the question texts, and the corpus were landed with the
Investigation so the Plan starts from a reproducible measurement; the pilot's
rejected phrasings and the per-name rule 56 variant are recorded in the
Investigation, not in the script.
