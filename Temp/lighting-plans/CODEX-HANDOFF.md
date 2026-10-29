# Codex handoff: lighting foundation

The user runs Codex directly in the primary checkout `C:\Users\dougt\Documents\BrokenEngine`, with
no session worktree. This user direction overrides the repository's own-worktree rule for this
work. The primary checkout normally stays on `main`, where other sessions land, so each session
switches to the work branch at the start and back to `main` at the end.

## Prompt for the first session

```text
Resume the lighting foundation work from origin/lighting-foundation-handoff-20261029, working
directly in this primary checkout (user direction: no new worktree).

Setup:
1. Confirm `git status` is clean and the current branch is main; stop and report if not.
2. git fetch origin
3. git switch -c lighting-foundation-work
4. git merge origin/lighting-foundation-handoff-20261029 — local main may be ahead of the
   snapshot's base aa47e4a3; resolve any conflicts against current source, preserving both sides'
   intent, and report them.
5. The commit force-adds Temp/lighting-plans/ and a few Temp/ evidence files (gitignored working
   notes, not /next-plan Plans). Do not move them under Documents/Plans.

Then read Temp/lighting-plans/00-context.md and run Temp/lighting-plans/01-build-and-static-checks.md
only. Wherever a plan or skill asks for the session worktree, branch or baseline, use this checkout,
branch lighting-foundation-work, and baseline = `git merge-base main lighting-foundation-work`.
No old Output binaries or packs count as verification; plan 01 builds them.

At the end: commit the work on lighting-foundation-work (a WIP commit is fine), update
Temp/lighting-plans/00-context.md ## Progress in that commit, then `git switch main` so other
sessions can land, and report what was built and verified and any residuals.
```

## Prompt for later sessions

```text
In this primary checkout: confirm `git status` is clean on main, then
`git switch lighting-foundation-work`. Read Temp/lighting-plans/00-context.md and
Temp/lighting-plans/CODEX-HANDOFF.md, then run Temp/lighting-plans/0N-<name>.md only.
At the end commit on lighting-foundation-work, update ## Progress, `git switch main`, and report.
```
