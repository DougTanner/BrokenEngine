# Codex handoff: lighting foundation

## Setup (user)

From PowerShell 7 at the primary checkout root `C:\Users\dougt\Documents\BrokenEngine`:

```powershell
git fetch origin
.\.codex\codex-worktree.ps1
```

The wrapper creates the session worktree and branch from current main and starts Codex there.
Then give Codex the prompt below.

## Prompt

```text
Resume the lighting foundation work from origin/lighting-foundation-handoff-20261029.

Setup, in this wrapper session worktree (keep the wrapper's branch and session setup):
1. git fetch origin
2. git merge origin/lighting-foundation-handoff-20261029 — fast-forward when main has not moved
   past aa47e4a3; otherwise resolve conflicts against current source, preserving both sides'
   intent, and report them.
3. The commit force-adds Temp/lighting-plans/ and a few Temp/ evidence files (gitignored working
   notes, not /next-plan Plans). Do not move them under Documents/Plans.

Then read Temp/lighting-plans/00-context.md and run Temp/lighting-plans/01-build-and-static-checks.md
only. Stop after 01, update 00-context.md ## Progress, and report what was built and verified and
any residuals. The user will start a fresh session for each later plan (02, 03, ...).

No old Output binaries or packs in this new worktree count as verification; plan 01 builds them.
```

## Later sessions

Each later session uses the same wrapper session worktree (reattach with
`.\.codex\codex-worktree.ps1 -ReattachWorktree <worktree path>`) and the prompt:
`Read Temp/lighting-plans/00-context.md, then run Temp/lighting-plans/0N-<name>.md only; stop after
it and update ## Progress.`
