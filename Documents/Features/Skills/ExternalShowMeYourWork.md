# External show me your work
Revisit When: a long-running session produces a documented important decision that cannot be reconstructed from its execution card, approved Plan, handoffs, and transcript, and the local opt-in ledger would have prevented the failure.

## Context

The source `show-me-your-work` skill records decisions in an append-only TSV so a reviewer can see what was chosen, why, and what evidence supports it. Its shell helper is not the repository's cross-client scripting convention, and its transcript audit would expose private session data. Broken Engine needs a local, opt-in log that is safe to open in a spreadsheet and stays separate from the Change Workflow records that already govern a change.

## Why this is worth integrating

Long or unattended work is easier to review when important forks and checkpoints have one concise record. A local decision ledger gives a human a useful trail without turning every command into a log row or making the ledger a second execution card. The default ignored location keeps that trail available without creating an accidental tracked artifact.

## Design

- Add one standalone package at `.agents/skills/external-show-me-your-work/`. The manual feature document will live at `Documents/Features/Skills/ExternalShowMeYourWork.md` when this design is adopted.
- Default the log to `Temp/external-show-me-your-work/<task>.tsv`. `<task>` is a caller-supplied slug containing letters, digits, and hyphens only. Reject empty slugs, path separators, `..`, and absolute paths. The repository already ignores `Temp`.
- Keep the six source columns exactly: `ts`, `phase`, `decision`, `why`, `evidence`, and `result`. The exact header is `ts<TAB>phase<TAB>decision<TAB>why<TAB>evidence<TAB>result`, with literal tab separators. A missing or zero-byte file receives that exact header. Before appending to a nonempty file, the appender requires its first line to match the exact header; a mismatch rejects the append, reports a schema error, and leaves the file byte-identical. Every later append adds one UTC row and never rewrites an earlier row.
- Replace the source shell helper with one PowerShell 7 row appender at `.agents/skills/external-show-me-your-work/scripts/Add-DecisionLogRow.ps1`. It accepts a task slug plus the five non-timestamp row fields (`phase`, `decision`, `why`, `evidence`, and `result`), stamps `ts` internally with the current UTC time, and does not accept a caller-supplied timestamp. The appender remains responsible only for structural validation and formula protection: it validates all inputs first, then opens the selected TSV once and appends one complete line; on validation or file-open/write failure it reports failure and does not claim success. It does not promise safety when another writer is active or when power is lost. It creates only the default task directory, strips tabs, newlines, and carriage returns from each cell, and prefixes a cell that starts with `=`, `+`, `-`, or `@` with a single quote before writing it. Semantic privacy enforcement belongs to the skill before this call; the appender accesses only its validated output path, treats evidence/path-looking cell text as opaque pre-screened text, and does not discover, enumerate, classify, open, or dereference evidence/transcript paths, invent secret-detection patterns, or read transcripts, environment values, or hidden credentials.
- Before calling the appender, the skill applies semantic privacy enforcement to each proposed row. When the user or agent identifies any proposed content as a secret, or identifies an absolute home path or transcript path, the skill rejects the append, requests redacted or repository-relative evidence, does not call the appender, and leaves the TSV byte-identical. This gate uses explicit identification and does not invent secret-detection patterns.
- Use concise evidence pointers such as a commit, review identifier, `file:line`, artifact, or screenshot. Do not record an absolute transcript path, absolute home path, secret, or raw transcript text. The skill does not seek out those values or copy them into the ledger.
- Log decisions, checkpoints, pivots, reverts, and blockers. Do not log every command. A correction appends a new row that supersedes an earlier row; it never deletes history.
- State in the skill that the ledger supplements, but never replaces, the execution card, plan reviews, correctness reviews, acceptance checks, or landing confirmation. The ledger does not authorize any workflow step.
- Keep the log local by default. Committing a ledger is a separate, explicit user decision that enters the normal tracked-change workflow. The skill never stages, commits, or lands the file automatically.

### Trigger and client policy

The frontmatter description names explicit opt-in decision-ledger requests for long-running, unattended, or multi-phase work. The exact human interfaces are `/external-show-me-your-work <task-slug>` in Claude Code and `$external-show-me-your-work <task-slug>` in Codex. The skill is explicit and manual in both clients. Set Claude `disable-model-invocation: true`, Codex `policy.allow_implicit_invocation: false`, and keep the user-invocable entry point. Manual invocation prevents silent writes, accidental secret capture, and a ledger being mistaken for a required repository control.

### Attribution and license

Adapt `pstack/skills/show-me-your-work` at commit `60c641e4fad674784b30abcf9f8915dea39df38d`. The standalone package keeps a `LICENSE` file with the full MIT notice and Lauren Tan's 2026 copyright notice. Preserve that notice for the substantially adapted skill and do not cite an absolute source path.

## Critical files

- `Documents/Features/Skills/ExternalShowMeYourWork.md`
- `.agents/skills/external-show-me-your-work/SKILL.md`
- `.agents/skills/external-show-me-your-work/agents/openai.yaml`
- `.agents/skills/external-show-me-your-work/scripts/Add-DecisionLogRow.ps1`
- `.agents/skills/external-show-me-your-work/LICENSE`

## In scope

- One standalone, manual, local decision-ledger skill for Claude and Codex.
- The ignored default path, safe task slug, six-column TSV shape, append-only behavior, and UTC timestamp.
- The PowerShell row appender with tab/newline cleanup and spreadsheet formula protection.
- Evidence and secret boundaries, the relationship to Change Workflow controls, explicit commit authority, client policy files, source attribution, and the package license.
- Validator and inbound-link checks, plus the smallest observable appender fixture.

## Out of scope

- Any actual skill or appender implementation in this drafting task.
- The source Bash helper, a second appender, a service-backed ledger, or a repository-wide logging framework.
- Transcript reads, transcript paths, home scans, environment capture, secrets, or raw private context.
- Automatic invocation, automatic row creation, automatic staging, commits, landing, or workflow gates.
- Treating a ledger as an execution card, Plan, review, acceptance table, or landing approval.
- Root workflow, `.gitignore`, or unrelated documentation changes.
- Unit tests.

## Risk tier and invariants

Tier 2. The feature writes a small local developer artifact and adds one bounded PowerShell tool. It does not touch engine state, determinism, CRC, wire data, `.pack` data, threading, or runtime behavior. The following invariants are mandatory:

- The default log is under ignored `Temp/external-show-me-your-work/` and remains untracked unless the user explicitly chooses otherwise.
- Appends are single-line, six-column TSV rows with UTC timestamps and formula-protected cells.
- Before the appender is called, the skill rejects any proposed row with content explicitly identified by the user or agent as a secret, an absolute home path, or an absolute transcript path; it requests redacted or repository-relative evidence, makes no appender call, and preserves the TSV bytes.
- The appender handles structural validation and formula protection only: it validates all inputs first, then opens the selected TSV once and appends one complete line; on validation or file-open/write failure it reports failure and does not claim success. It does not promise safety when another writer is active or when power is lost. It does not infer secret-detection patterns or perform semantic privacy enforcement; it accesses only its validated output path, treats evidence/path-looking cell text as opaque pre-screened text, and does not discover, enumerate, classify, open, or dereference evidence/transcript paths. It never reads transcripts, environment values, or hidden credentials.
- The output-file schema is exact: a missing or zero-byte file gets `ts<TAB>phase<TAB>decision<TAB>why<TAB>evidence<TAB>result`, and a nonempty file must have that exact header on its first line before an append. A header mismatch reports a schema error, rejects the append, and leaves the file byte-identical.
- The ledger supplements every Change Workflow control and cannot satisfy or authorize one.
- Claude and Codex expose the same manual-only behavior. A committed log is governed by the ordinary tracked-change tier.

## Acceptance criteria

- The future package passes `validate-skill` for Claude and Codex metadata, and its bundled links and inbound references resolve.
- The PowerShell appender passes one disposable smallest-observable fixture: it creates the exact header for missing and zero-byte files, appends to a matching-schema file, preserves six columns, stamps UTC, strips tabs/newlines, prefixes every formula-leading cell, and proves that a schema mismatch reports a schema error and leaves the existing file byte-identical; a validation failure likewise leaves an existing file byte-identical.
- Independent realistic Claude and Codex scenarios create a ledger only after explicit invocation, keep it under the ignored default directory, and leave all workflow records and source files untouched.
- A semantic-privacy scenario submits one proposed row containing the explicitly identified synthetic secret `SYNTHETIC_SECRET_FOR_TEST_ONLY`, absolute home path `C:\Users\Example\`, and absolute transcript path `C:\Users\Example\session-transcript.json`. The skill rejects the append, requests redacted or repository-relative evidence, makes no appender call, and proves the TSV is byte-identical before and after the attempt.
- Static inspection proves that the skill says the ledger supplements execution cards, reviews, acceptance, and landing, and that committing it requires separate authority.
- The package preserves the required MIT notice. No unit tests are added.

## Notes/Coordination

This is a manual Feature document under `Documents/Features/Skills/`, without executable-Plan metadata. Ordinary ledger use is outside Change Workflow. Implementing the tracked skill or choosing to commit a log is separately authorized work and follows the normal review and landing rules for the changed artifact. The source remains `pstack/skills/show-me-your-work` at the cited commit.
