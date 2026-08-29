---
name: progressive-disclosure-review
description: >-
  Review session-changed instruction prose — AGENTS.md, CLAUDE.md,
  `.agents/skills/**/*.md`, `.agents/references/**/*.md` — against the root
  AGENTS.md
  progressive-disclosure directive. Use during Change Workflow Step 6 after
  `/update-claude-docs` whenever the session changed such a file, and when a
  reviewer suspects a skill body restates a reference, a script, or a parent
  AGENTS.md, or exceeds the skill size thresholds. Findings only; never edits.
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Progressive Disclosure Review

Verify that changed instruction prose obeys the `Progressive disclosure`
directive in root [AGENTS.md](../../../AGENTS.md) `## Directives`. That
directive is the sole statement of the layering; this skill only enforces it.

## Inputs

- Session baseline (full 40-character SHA) and the changed instruction-doc list,
  or the diff needed to derive it.
- The immutable snapshot of those files as changed.

If the baseline or the changed bytes are unavailable, return `BLOCKED` naming
the missing input.

## Execution Context

Run in the delegated execution context of
`../../references/subagent-reporting.md` — in Claude Code that dispatch goes
through `/codex-review`, while a Codex caller is already the Sol reviewer
mapping and executes the `reviewer` role directly rather than invoking
`/codex-review` recursively; inline review is prohibited. Dispatch is once per
review round; after the manager accepts findings and fixes land, only the
affected files receive a focused re-review.

## Review

1. Take the changed regions from the read-only inventory rather than
   re-deriving hunks, using the invocation and result contract in
   `../scope-review/SKILL.md` step 1, filtered to instruction-doc paths.
2. Duplication: flag changed prose that restates or paraphrases content another
   location already owns — a reference doc, a script's own documented usage, a
   parent `AGENTS.md`, another skill, elsewhere in the same file, or code and
   comments reached through pointers in the changed prose. Cite the changed
   location and the owning location, and give the reference that replaces it.
3. Layer misplacement: flag detail sitting above its owning layer — a schema,
   mechanics, or a long example in a `SKILL.md` body instead of `references/`
   or a script; a subsystem constraint narrated in a skill instead of the
   owning `AGENTS.md`; local rationale in an `AGENTS.md` instead of a code
   comment. This includes changed skill prose that computes a repeatable
   verdict from explicit machine-readable inputs through Git/range/path
   parsing, set operations, schema-field mechanics, or exit/status/retry
   algorithms. Each such finding names the changed prose, its deterministic
   inputs and result, and the script/reference that owns the computation or
   where it should be extracted. Keep when-to-invoke rules, ordering, typed-
   result branching, roles, authority, user interaction, and irreducible
   judgment in the skill; those are orchestration, not misplaced mechanics.
4. Size: measure each changed skill markdown file with `pwsh -NoProfile -File
   .agents/scripts/Measure-Tokens.ps1 -Path <file>`. A `SKILL.md` body over
   10,000 `bt-token-v1` needs a stated reason why the detail cannot move to a
   reference or script, 15,000 is the ceiling, and a reference file over 2,000
   needs a table of contents. These are `NEEDS_ACTION` findings, not advice.
5. Scope guard: judge only session-changed bytes. Report pre-existing excess in
   untouched prose as a residual and never demand trimming it.
6. Precision guard: every finding names the owning location or the exceeded
   threshold. No owner named, no finding.

## Exclusions

- Frontmatter, discovery, invocation policy, and bundled-link mechanics —
  `/validate-skill`.
- `AGENTS.md` content correctness, chain sync, and leaf/hub size targets —
  `/update-claude-docs`.
- C++ comment style and formatting — `/code-style-review`.
- Scope authorization and unnecessary extra work — `/scope-review`.

## Output

One line per finding:

```text
file:line — class: duplication | misplacement | size — evidence: owning location or measurement — fix direction
```

Example:

```text
.agents/skills/code-quality-metrics/SKILL.md:120 — class: duplication — evidence: references/history-contract.md:14-38 states the same JSONL field list — replace with a link to that reference
```

Then the summary block:

```text
Skill: progressive-disclosure-review
Baseline: <full SHA>
Files checked: <count and paths>
Findings: <count or none>
Status: PASS | NEEDS_ACTION | BLOCKED
Changed files: none
Decisive checks: <measurement commands and results>
Build required: none
Executor: <own model id> <own effort>
Residuals: <pre-existing excess or none>
```

`Changed files` and `Build required` are `none` because this findings-only
review never edits a file; the remaining shared lines follow
`../../references/subagent-reporting.md`, `## Handoffs`. Before `/codex-review`
prompt assembly will assemble a `/verify-changes` prompt over changed
instruction prose, this block must carry four things, none of which may be
dropped or reworded: the `Skill: progressive-disclosure-review` marker line, the
`Files checked:` line, a `Status: PASS` verdict, and a `Baseline:` line whose
SHA is either the `/verify-changes` dispatch baseline or another resolvable
commit at which the instruction docs that diff changes do not differ from the
dispatch baseline, which is what makes those docs' reviewed bytes the bytes
this review read. A `NEEDS_ACTION` or `BLOCKED` handoff, and one whose
`Baseline:` is absent, unresolvable, or a commit at which those docs differ,
are gated exactly like a missing handoff — so re-run only when those docs
differ at the two baselines.

The manager decides each finding on whether the failure is concrete and
meaningful under the standard defaults; this review adds no extra rounds.
