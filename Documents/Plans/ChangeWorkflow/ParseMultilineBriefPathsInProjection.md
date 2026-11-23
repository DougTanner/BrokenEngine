<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-23T00:04:14.732Z","dependsOn":[]} -->
# Parse multi-line brief path fields in transcript projection

## Context

During `/next-plan-checkpoint-review`, isolation checking relies on
`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
emitting a `<line> match <read path> read-at <read line>` row whenever a
delegation brief lists a path that an earlier qualifying main-session read
opened (`.agents/skills/next-plan-checkpoint-review/references/worker.md`,
step 12).

During `/external-skill-creator` validate-mode review of
`next-plan-checkpoint-review`, finding VS002 (Critical) identified a
pre-existing defect in `Write-ToolCallProjection`:

- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1:921`
  (baseline `b851e1f35d41dc089e36fc8a12575443c40dfb90` `:759`) selects brief
  path lines solely with
  `$pathLines = $fieldLines | Where-Object { $_ -like 'Governing paths:*' -or $_ -like 'Scope:*' }`.
- Only tokens on the same line as `Governing paths:` or `Scope:` reach the
  tokenizer at `:964-970`. Any path written on a continuation line beneath
  either label — such as multi-line `- <path>` bullets or wrapped lines — is
  dropped and produces no `$briefKeys` entry and no `match` row.
- In a recorded `agy` run, an unbounded 11,315-character read of
  `plan-alternatives/SKILL.md` (line 21) produced no `match` row for the
  delegation brief (line 24) that listed that file on a continuation bullet
  under `Governing paths:`.

Because task briefs routinely format `Governing paths:` and `Scope:` as
bulleted multi-line lists, main-session reads of delegated files silently
escape isolation detection whenever the brief uses continuation lines.

## Design

Recommended approach: replace the single-line `$pathLines` filter in
`Write-ToolCallProjection`
(`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1:921`)
with a sequential scan over `$fieldLines` that collects the `Governing paths:`
and `Scope:` header lines together with their continuation lines until the next
known task-brief top-level field label, feeding the existing tokenizer
(`:964-970`) unchanged. Rationale: a single sequential pass handles both
single-line and multi-line bulleted or wrapped bare brief paths without pulling
paths from unrelated sections (`Exclusions:`, `Fixed decisions:`, `Baseline:`,
`Evidence:`, `Acceptance:`, `Prohibitions:`, `Return:`,
`Continuation capsule:`), and without dropping valid continuation lines such as
unindented subheadings or Windows drive paths, while keeping `brief-missing`
detection (`:922-931`), shared tokenization (`:964-970`), and canonical path
matching (`:964-993`) intact.

Path-annotation coverage: this fix supports bare continuation paths and every
separator or trailing-punctuation form the existing tokenizer (`:964-970`)
already accepts (whitespace, backticks, `,`, `;`, and trailing `.`, `)`, `"`,
`'`). Stripping `:line`/`:start-end` selectors (such as `path:177-243`) and
leading-wrapper punctuation (`(`, `"`, `'`) is explicitly deferred so
single-line tokenization behavior stays strictly unchanged.

1. Near `$script:BriefLabels` (`:97`), define the known task-brief top-level
   field labels that terminate a `Governing paths:` or `Scope:` block: the
   other labels in `$script:BriefLabels` (`Role:`, `Skill:`, `Objective:`,
   `Required sections:`, `Exclusions:`, `Fixed decisions:`, `Baseline:`,
   `Acceptance:`, `Prohibitions:`, `Return:`) plus known supplementary
   top-level brief labels (`Continuation capsule:`, `Evidence:`,
   `Session provenance:`, `Authorization source:`, `Tier:`, `Mode:`,
   `First-round findings:`, `Pre-existing ownership snapshot:`,
   `Known build target/configuration:`).
2. In `Write-ToolCallProjection` (`:918-923`), track whether the scanner is
   currently inside a `Governing paths:` or `Scope:` block as it iterates over
   `$fieldLines`:
   - Start or continue collecting on a line matching `Governing paths:*` or
     `Scope:*`, and record `$hasGoverningPaths = $true` when a
     `Governing paths:*` line is seen.
   - Otherwise, while collecting, stop only when a line begins at column 0 with
     one of the known stop labels above (matched unindented via
     `-like "$label*"`). Keep unindented subheadings (such as
     `Affected files and regions:`), bare Windows drive paths (such as
     `C:\...`), indented lines, and bulleted lines (`- <path>`, `* <path>`,
     `- Label: <path>`) inside the active block.
   - Append each collected line to `$pathLines`.
3. Update the script header comment (`:10-13`) to state that `Governing paths:`
   and `Scope:` values include their continuation lines up to the next known
   task-brief top-level field label.

Change Workflow tier: Tier 2 — trigger: scoped behavior of one tooling
subsystem
(`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`),
changing how delegation briefs are parsed into `match` rows. No C++ source,
determinism/CRC, wire, serialization, `.pack`/`kiVersion`, replay, affinity,
threading, allocation, shader, build, live `/agent-harness`, or shared-tool
build/bootstrap coordination exposure.

## Critical files

- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
  — header contract (`:10-13`), brief-label constants (`:97`), and
  `Write-ToolCallProjection` brief path collection (`:918-923`).

## In scope

- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`:
  - Brief-label constants (`:97`) and `Write-ToolCallProjection` (`:918-923`):
    collect continuation lines under `Governing paths:` and `Scope:` up to the
    next known task-brief top-level field label so bare continuation paths
    (including under unindented subheadings and bare Windows drive paths) reach
    the existing tokenizer unchanged.
  - Script header comment (`:10-13`): align the `<line> match` contract
    description with multi-line `Governing paths:` and `Scope:` fields.

## Out of scope

- Changes to the shared `$pathLines` tokenizer (`:964-970`), including
  stripping `:line`/`:start-end` selectors on brief paths (such as
  `path:177-243`) or trimming leading wrapper punctuation (`(`, `"`, `'`).
- Small-read exemption thresholds or `CandidateMatches` filtering
  (`:104-106`, `:1232-1255`).
- Path canonicalization (`ConvertTo-CanonicalPathKey`, `:184-232`), checkpoint
  span grouping, Codex bridge parsing, or `-IncludeContextEnvelope`
  measurement.
- `.agents/skills/next-plan-checkpoint-review/SKILL.md` or `references/*.md`
  unless an exact documented contract changes.

## Acceptance criteria

1. Running `Get-TranscriptProjection.ps1` on a synthetic JSONL transcript
   containing unbounded `Read`/`read_file` calls followed by a delegation brief
   listing bare paths on `- <path>` continuation bullets under
   `Governing paths:` — and separately under `Scope:`, including beneath an
   unindented `Affected files and regions:` subheading and on a bare Windows
   drive path (`C:\...`) continuation line — emits a `<line> match` row pairing
   the brief line with each read line.
2. Paths appearing after `Governing paths:` or `Scope:` under a subsequent
   known top-level brief field label (`Exclusions:`, `Baseline:`, `Evidence:`,
   `Prohibitions:`, `Return:`, `Continuation capsule:`) do not emit false
   `match` rows.
3. Single-line `Governing paths:` and `Scope:` briefs, `brief-missing`
   detection, and existing shared tokenization (`:964-970`, leaving leading
   wrappers and `:line`/`:start-end` selectors unchanged) retain their existing
   behavior.
4. `pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/next-plan-checkpoint-review`
   and `Invoke-StaticChecks.ps1` exit `0` with all triggered checks passing.

## Notes

- Keep unindented subheadings (such as `Affected files and regions:`), bare
  Windows drive paths (`C:\...`), and bulleted or indented sub-labels (such as
  `- Proposal: ...` or `- Target: ...`) inside the active `Governing paths:`
  or `Scope:` block; only a known unindented task-brief top-level field label
  terminates the field.
- Because the named file lives under `.agents/`, this Plan belongs in
  `Documents/Plans/ChangeWorkflow/`.
