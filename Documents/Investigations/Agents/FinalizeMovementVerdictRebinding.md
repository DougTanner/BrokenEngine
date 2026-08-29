# Finalize Movement Verdict Rebinding

Status: Open investigation; no acceptance-rule change has been chosen.

Area: Agents / finalize-changes

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

## Observed symptom

During a `session-landing` run of `/finalize-changes`, after the landing
candidate was prepared and `/verify-changes` passed, the read-only movement
checker

`pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1 ...`

returned exit `0`, `status` `needs-review`, `code`
`primary.disjoint-needs-review`
(`Invoke-FinalizePrimaryMovementCheck.ps1:377`), because other agent sessions
had landed on `main` while this session was verifying.

The documented handling
(`.agents/skills/finalize-changes/references/workflow.md:97-113`, terminal
mapping in `.agents/skills/finalize-changes/references/scripts.md:123`) is one
fresh focused dependency reviewer whose verdict is bound to the candidate
commit, candidate tree, candidate parent, and live primary it reviewed, followed
by a confirming rerun of the same checker that "accepts the verdict only when
the result is still `needs-review` and all four identities match exactly"
(`workflow.md:110-112`); "A live-primary change discards the verdict and follows
the new checker result" (`workflow.md:113-114`).

Observed loop: each dependency review runs through `/codex-review` and takes
roughly three to five minutes, and `main` moved during every one of them, so the
confirming rerun reported a different live primary and the verdict was thrown
away. Primary advanced `4d6f933 -> 8560cd1 -> 3756c9d -> 62bcaff -> 297d325`
during this landing. Three consecutive reviewer verdicts — bound to `3756c9d7`,
then `62bcaff5`, then a third — were all `independent` and all discarded; the
fourth, bound to `297d325f`, was finally accepted. A fifth foreign commit
`80d93b7` landed during the landing script itself and was absorbed by that
script's own bounded internal rebase.

Rework forced: three full review rounds, roughly ten to fifteen minutes of
reviewer time plus the surrounding orchestration, produced no information. In
every round the checker reported overlap `0` — no foreign commit touched any
path this session owned (`primary.path-overlap`,
`Invoke-FinalizePrimaryMovementCheck.ps1:369`, never fired) — and no verdict
ever differed from `independent`. The loop is not bounded by anything in the
current rules: on a machine where foreign landings arrive faster than a review
completes, it never terminates on its own.

Evidence retained only in the observing session's gitignored `Temp/` directory
(three dependency-reviewer output files and the checker's own JSON result file);
those are machine-local scratch files, not tracked repository paths, and they
are gone once the worktree is cleaned up.

## Open alternatives

These are alternatives for a future decision, weighed on the same criteria: does
it bound the loop, what explicit verdict coverage it keeps, and what it costs
other sessions. Their order is not a recommendation, and no behavior is selected
here.

1. **Rebind the verdict to what was actually reviewed.** Replace the
   live-primary identity in the acceptance rule with the reviewed
   foreign-commit set: the confirming rerun accepts an `independent` verdict
   when the candidate commit, tree, and parent still match and the movement the
   reviewer saw is still a prefix of the current movement — that is, when the
   only additional foreign commits are ones the checker itself reports as
   disjoint from the owned paths. It matches the verdict's actual subject, since
   the reviewer reasons about the foreign changes rather than about which commit
   happens to be the tip, and it removes the unbounded loop rather than
   shortening it. Its cost is that new foreign commits then land without an
   explicit human-visible dependency verdict of their own, resting on the
   checker's overlap-zero partition.
2. **Incrementally review only the new movement.** Keep the live-primary
   binding, but when a rerun finds new movement, dispatch the next reviewer over
   only the commits added since the accepted-so-far set instead of the whole
   range. It preserves an explicit verdict for every foreign commit; its cost is
   that the loop still exists and merely gets cheaper per round, so it does not
   by itself terminate under fast foreign landings.
3. **Hold the lease across the review.** Let the dependency reviewer be
   dispatched under a short-lived landing lease so primary cannot move
   underneath it. It is conceptually the simplest and keeps every existing
   identity rule intact; its cost is that it blocks every other session's
   landing for the length of a review round, which the current rules
   deliberately avoid ("No lease is held across the reviewer or user wait.",
   `workflow.md:114`), so it likely needs user approval as a coordination-policy
   change.

## Decision that blocks a Plan

None of the three alternatives satisfies every requirement at once — bounding
the loop, keeping an explicit verdict for every foreign commit, and never
blocking other sessions' landings — so the trade-off has to be chosen before an
implementation exists. The unresolved question is which of those requirements
may be relaxed, and, if alternative 3 is chosen, whether holding a lease across
a review round is an acceptable coordination-policy change.

Constraints any chosen fix must respect: the checker stays read-only — no ref,
index, checkout, or worktree change, and no lease — and any overlap between
foreign movement and an owned non-history path must still block with
`primary.path-overlap`. The blocking codes `primary.path-overlap`,
`primary.not-descendant`, `primary.owned-history-path`, and
`primary.evidence-truncated` behaved correctly throughout and need no change, as
do the landing lock and compare-and-swap, the history overlay, the confirmation
contract, and `/verify-changes`. No transcript path or transcript text belongs
in the repository.

## Files a future Plan would touch

- `.agents/skills/finalize-changes/references/workflow.md` — movement-check
  handling and the four-identity acceptance rule, lines 87-114
- `.agents/skills/finalize-changes/references/scripts.md` — movement-check
  contract and terminal mapping, lines 88-131
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1`
  — assessment and result codes, lines 330-378
- `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1` —
  movement fixtures, lines 1858-2040; it already exercises the `needs-review`
  movement cases (lines 2015 and 2035)
- `.agents/skills/finalize-changes/SKILL.md` — only if the chosen fix changes a
  rule this file summarizes

## Earning an executable Plan

After the trade-off above is chosen, this investigation can be converted into a
decision-complete Plan under `Documents/Plans/Agents/` carrying the required
byte-zero scheduler metadata. Expect Tier 3 (landing coordination that can block
other sessions), because the change alters when a session is allowed to advance
shared primary; a fix confined to reviewer-dispatch wording that changes no
acceptance condition would be Tier 2. Root-causing the recorded friction from
the proven transcript is available through
`/next-plan-review claude/ca447260-1257-440a-889e-71d936639fcd`, supplying
client `claude` and the conversation session ID recorded below.

## Provenance

Machine-local; not reproducible after worktree cleanup. The Client through
Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach.

- Client: claude
- Conversation session ID: bf5fb2c4-ef24-4315-9cbe-2ab34ae1ec00
- Worktree/branch UUID: ca447260-1257-440a-889e-71d936639fcd
- Session branch: claude/ca447260-1257-440a-889e-71d936639fcd
- Worktree: .claude\worktrees\BrokenEngine\ca447260-1257-440a-889e-71d936639fcd
- The observing session's work landed as commit
  `9ca74c155653633db4bd8853be5807d95623812d` on `main`; that is the stage whose
  finalization exhibited the friction, and it does not contain this document.
- This document lands in a later, separate commit made from session branch
  `claude/ca447260-1257-440a-889e-71d936639fcd`.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- Documents/Investigations/Agents/FinalizeMovementVerdictRebinding.md`,
  but a periodic history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.
- The friction lies entirely in `/finalize-changes` movement-check rules and
  scripting, outside the scope of the Plan the observing session had claimed
  (`Documents/Plans/Engine/CrashPathStartupPreresolution.md`, an engine
  crash-path startup change, completed and removed by that landing), so it was
  not an in-scope blocker of that change.
- No option above is a chosen behavior; no script or workflow change is
  authorized by this record.
