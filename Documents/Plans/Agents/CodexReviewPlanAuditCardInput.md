<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T18:50:27.212Z","dependsOn":[]} -->
# Fix: codex-review — scope-file guidance omits the Tier-3 plan-audit draft execution card

## Context

A Tier-3 `/plan-audit` was dispatched through `/codex-review` with a manager-authored `-ScopeFile` that
described the plan, the tier, and the review focus, but did not contain a draft execution card. The audit
returned no findings at all:

```
Traceability checked: blocked before execution-card comparison
Required next step: Supply the mandatory Tier-3 draft execution card: proposed implementation/review roles
and, for every acceptance criterion, its decisive check, expected result, and independent signal for
duplicated checks.
Status: BLOCKED
Residuals: Required Tier-3 draft execution card is missing from the supplied evidence.
BLOCKED: missing Tier-3 draft execution card
```

(symptom citation: `Temp/codex-plan-audit-out.md`, `Status: BLOCKED` and the final `BLOCKED:` line). The full
dispatch — prompt assembly, headless Codex run, and reviewer pass — was spent producing no review content.
The manager then re-authored the scope file with the card and re-ran the identical dispatch, which succeeded.

`/plan-audit` states the requirement in its own contract (`.agents/skills/plan-audit/SKILL.md:28`, "Draft
execution card for every Tier-2 and Tier-3 plan", consumed again at `:90` and `:96`). `/codex-review` is the
skill that assembles what the reviewer actually receives, and its scope-file guidance
(`.agents/skills/codex-review/SKILL.md:55-70`) tells the caller only to write "the exact scope, the files and
regions authorized for review, focus notes, and current residuals" — it enumerates no per-assigned-skill
required inputs. `New-CodexReviewPrompt.ps1` already branches on `$AssignedSkill` for `repo-code-review`
(`:490`) and `verify-changes` (`:480`, `:543`), so the assembly layer does know which skill it is serving; it
simply carries no obligation, check, or documentation for `plan-audit`'s card.

The two skills involved are outside the claimed Plan's `## In scope`
(`Documents/Plans/Frame/FrameUtilsSharedHelpers.md`), which authorizes only engine and game frame C++ plus the
frame AGENTS.md sentences naming moved helpers.

Session provenance (machine-local; not reproducible after cleanup):

- Client: claude
- Session: 0d0c7774-565c-4822-bf8c-ff2ca3578181
- Session branch: claude/0d0c7774-565c-4822-bf8c-ff2ca3578181
- Worktree: .claude\worktrees\BrokenEngine\0d0c7774-565c-4822-bf8c-ff2ca3578181
- Landing commit: `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/CodexReviewPlanAuditCardInput.md`
- Run the review before /cleanup-worktrees removes this worktree: Codex transcript discovery requires the
  producing worktree to remain registered, and Claude review requires the exact session id above.

## Design

In a new session, run `/next-plan-review <landing commit>` supplying the recorded client and session id,
root-cause the friction from the proven transcript, then make the smallest fix inside the `## In scope`
boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead
of expanding scope.

The symptom already narrows the candidates: either `/codex-review`'s scope-file guidance enumerates the
per-assigned-skill required inputs — at minimum the Tier-3 `plan-audit` draft execution card — so the caller
authors it before dispatching, or `New-CodexReviewPrompt.ps1` fails fast on a `plan-audit` dispatch whose
scope text carries no card, the way it already special-cases other assigned skills. Root-causing decides
between them; a caller-side documentation fix and a script-side guard are not both required.

## Critical files

- `.agents/skills/codex-review/SKILL.md`
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`
- `.agents/skills/codex-review/references/prompt-template.md`
- `.agents/skills/plan-audit/SKILL.md`

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance.
- The smallest resulting fix, confined to the files named above — specifically `/codex-review`'s scope-file
  authoring guidance and its assigned-skill handling in prompt assembly, and `/plan-audit`'s statement of the
  evidence its caller must supply.

## Out of scope

- The landed change the session produced.
- The separate read-only-sandbox metrics Compare friction, owned by
  `Documents/Plans/Agents/CodexReviewMetricsCompareSandbox.md`.
- Unrelated skills and scripts; any transcript path or transcript text in the repository.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches build/bootstrap coordination. Never embed
transcript paths or home paths. The script must keep copying manager-authored scope text verbatim and must
never author, summarize, or edit review judgment content.

## Acceptance criteria

- The recorded symptom no longer reproduces: a Tier-3 `/plan-audit` dispatched through `/codex-review` by a
  caller following the documented guidance does not return `BLOCKED: missing Tier-3 draft execution card`.
- `/validate-skill` passes for any changed `SKILL.md`; `plan validate` exits 0.
