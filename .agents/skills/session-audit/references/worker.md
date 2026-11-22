# Session Audit Worker

Receipt setup for the brief-writing `implementer` and audit steps for the
dispatched `reviewer`. Triggers, brief, and handoff live in
[`../SKILL.md`](../SKILL.md).

## Steps

### Inventory receipt

The brief-writing `implementer` creates `Temp/` if absent in its own call, runs
below for the target (`-Head` for a committed SHA, omit for working tree; add
`-IncludeUntracked` if needed), requires a successful CLI exit, and reads
`entries`, `counts`, `triggers`, and `regions`:

```powershell
pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot '<root>' -Baseline <baseline SHA> -Regions -OutputPath Temp/session-audit-inventory.json
```

### Audit

1. Confirm the checkout. Done when the adopted worktree matches the brief.
2. Verify the receipt's `baselineSha` and target (`headSha: null` is
   working-tree mode) against the brief; missing or unusable receipts follow
   `../SKILL.md` `## Inputs`. Use its `entries`, `regions`, `baselineSha`, and
   `headSha` instead of rebuilding the diff. Done when verified or stopped.
3. For each triggered mode, run its `../SKILL.md` `## Inputs` procedure on only
   named regions and minimum callers, consumers, mirrors, contracts, and context
   to prove or refute hypotheses; in committed mode, read Engine at `headSha`
   and `Projects/Coretide` at `coretide.headSha`. Stop once resolved. Done when
   traced.
4. Report only changed, reachable failures. Done when every reported finding
   names a changed, reachable failure.
5. Refute proposed findings against guards, preconditions, handoffs, and current
   contracts. Done when every proposed finding is refuted or retained.
6. Put proven pre-existing or out-of-scope defects in `Residuals`. Done when
   every such defect appears there.
7. Exclude stale citations in a claimed plan that is deleted when it completes.
   Done when no retained finding cites that plan.
8. Emit a single-claim API verification request for any candidate depending on
   a non-obvious external rule; do not present it as confirmed. Done when every
   such candidate carries one request.

## Rules

- The reviewer does not edit, run commands that change state, or implement
  fixes; shared reviewer conduct:
  [`../../../references/subagent-reporting.md`](../../../references/subagent-reporting.md).
