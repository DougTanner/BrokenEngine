# 04 Commit and create the `lighting-foundation` branch

Read `Temp/lighting-plans/00-context.md` first. Requires plans 01-03 done.

## Objective

Publish the verified foundation as the reusable `lighting-foundation` branch next to main, without
changing primary, and offer the separate main-side script fix.

## Steps

1. Main runs `/verify-acceptance` through a fresh read-only `reviewer` over the handoff's
   acceptance table and plan 03's evidence (this stage completes without landing on primary).
2. Ask the user, in one message with full context:
   - commit shape: one squashed foundation commit on top of `aa47e4a3` (recommended: candidates
     branch from one clean commit) or keep the WIP history;
   - whether to push `lighting-foundation` to origin;
   - whether the old `lighting-foundation-handoff-20261028` (origin) and
     `lighting-foundation-before-rebase-20261028` (local) refs should be kept.
   Check first that no `lighting-foundation` ref exists locally or on origin; never overwrite one.
3. After confirmation: untrack the snapshot's force-added `Temp/` files
   (`git rm -r --cached` on their exact paths; keep them on disk) so they stay out of the foundation
   commit, then commit (end the message with the attribution lines the session's system
   reminder gives), create the branch at that commit, push if approved. The wrapper session branch
   stays.
4. The `update-vcxproj` one-line ModelViewer exclusion (`Update-EngineVcxprojItem.ps1:301`) also
   belongs on main, where the same bug blocks `/update-vcxproj` for every session. Offer the user a
   separate Tier-2 change landing just that line through `/finalize-changes`; do not mix it into the
   foundation commit's landing.

## Acceptance

`lighting-foundation` points at the verified commit; primary untouched (unless the user approves
the separate script fix landing); the five prototype docs name that branch as their start.

## Finish

Tick 04 in `00-context.md` `## Progress` with the commit SHA and branch name.
