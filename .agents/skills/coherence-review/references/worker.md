# Coherence Review Worker

The review steps and the judgment rules the dispatched reviewer runs. Triggers,
inputs, modes, and the handoff form live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Fix mode and components from the brief; on focused re-review, read its
   files, regions, and `First-round findings`, skipping step 2. Done when scope
   is settled.
2. First-round: run below with no disk writes (`-Head` from `Baseline` if
   committed; from `Scope`, `-UntrackedPath`/`-IncludeUntracked`, plus
   `-PathPrefix "<prefixes>"` if set). Require `status` `pass`, else return
   `BLOCKED` without rebuilding the diff. Done when regions and omitted count
   print.

```powershell
pwsh -NoProfile -Command 'Import-Module ./.agents/scripts/SessionChangeInventory.psm1; Get-SessionChangeInventory -RepositoryRoot (Get-Location).Path -Baseline <baseline SHA> -Regions -CoherenceOnly|Select-Object status,coretide,counts,entries,regions|ConvertTo-Json -Depth 5'
```

3. Read each region's old lines from baseline (`oldPath` on renames) and new
   lines from committed head or worktree; read `Projects/Coretide/` entries
   using `coretide` from step 2 or `Scope` per
   [`subagent-reporting.md`](../../../references/subagent-reporting.md). Done
   when every region is read.
4. Review those changed bytes against the rules they touch: a statement that
   contradicts the source it cites, a fact that now disagrees with its owning
   location, an instruction that cannot be followed as written, a path, link,
   or anchor that does not resolve, and a rewritten sentence that drops a
   number, unit, condition, exception, or "only when" scope the old text
   carried, or turns a hedge into a stronger claim, unless the text still
   states it elsewhere or the brief records the drop as intended. This
   component passes when no accepted semantic finding remains unresolved. Done
   when every changed region has been judged that way.
5. At Tier 2+, run the authorization, minimality, and KISS passes of
   [`scope-authorization.md`](../../../references/scope-authorization.md) over
   the same regions. Done when each pass has run against the supplied
   authorization source, or its absence is reported as that reference directs.
6. Report wording and formatting problems in the changed regions as findings
   alongside the semantic findings. Done when every such problem is reported.
7. In combined mode, run in this same context, in full, every Apply the
   triggered cleanup check the brief lists as carried; never route one to
   another worker. Each passes on its own skill's pass condition. Done when each
   triggered skill has run in full and its handoff is placed as `../SKILL.md`
   `## Handoff` states.
8. In combined mode, when the Verify the acceptance table step applies — a stage
   completing without landing — verify the acceptance table as
   [`../../verify-acceptance/SKILL.md`](../../verify-acceptance/SKILL.md)
   requires, under the Tier-1 evidence ceiling it applies. This component passes
   when every approved criterion and invariant maps to such evidence. Done when
   the `Criteria` field is filled per that handoff.

## Rules

- The Apply the triggered cleanup mechanic and implementer checks —
  `/update-vcxproj`, `/update-claude-docs` — are not reviewer dispatches and are
  unaffected by the combined mode.
- Edit no tracked file; in combined mode, write only the `Temp/` file
  [`../SKILL.md`](../SKILL.md) `## Handoff` requires. Every problem is a
  finding for the manager to decide.
- The Review and resolve correctness step's fix-round rule is unchanged: main
  decides once, accepted fixes go to a separate `implementer`, and only affected
  regions are re-reviewed.
