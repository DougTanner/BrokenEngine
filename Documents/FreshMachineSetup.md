# Fresh-Machine Setup

Ordered bootstrap for a new Windows machine, from empty disk to a working agent worktree session. Each step's checks fail loudly, but nothing else sequences them — follow the order below. Install details (versions, winget commands, optional maintainer preferences) live in [README.md](../README.md); this document owns the sequence and the worktree-specific steps.

## 1. Enable Windows Developer Mode (before cloning)

Settings -> System -> For developers -> Developer Mode -> On. This grants the privilege Git needs to create symlinks. Without it, symlinked paths — notably `.claude/skills`, which exposes the tracked `.agents/skills` directory to Claude Code — check out as plain text files, and the session wrappers refuse to start.

Recovery for a clone made without Developer Mode: enable it, open a new terminal, then run `git config core.symlinks true` followed by `git checkout -- .claude/skills` in that clone.

## 2. Install prerequisites

Per [README.md](../README.md): Visual Studio 2026 Community (Desktop development with C++, Game development with C++, Windows 11 SDK), the Vulkan SDK, Git for Windows, PowerShell 7 (`pwsh`), Windows Terminal, x64 CPython 3.12 or newer, Gaea 2 (session start may bake island terrain), and the agent CLIs (Claude Code and/or Codex CLI).

Every skill that needs host Python requires x64 CPython 3.12 or newer. `code-quality-metrics` selects the first `python` Application resolved through normal PATH precedence per its metric contract; the `gaea2-*` and `analyze-diagsession` skills locate an interpreter through `.agents/scripts/Detect-Python.ps1`, which also probes well-known install directories. Neither route uses the `py` launcher as a fallback.

## 3. Clone with symlinks and submodules

```
git -c core.symlinks=true clone --recurse-submodules <repository-url>
```

This clone is the **primary checkout**. Agent sessions run in linked worktrees that share its immutable build outputs; the steps below prepare those outputs once. Session creation also needs per-worktree Git configuration, which requires `git config extensions.worktreeConfig true` in this clone once; the first wrapper run (step 6) applies it for you if it is not set, and it changes nothing about the primary checkout itself.

The first recursive submodule initialization requires network access. For an existing checkout that needs the metrics dependency, run `git submodule update --init --recursive -- ThirdParty/scb-check`; its first fetch also requires network access. The first `code-quality-metrics` run also requires network access to bootstrap its locked dependencies.

## 4. Build primary ThirdParty in all three configurations

The primary game data export in step 5 hard-fails without `ThirdParty.Debug.lib`, `ThirdParty.Profile.lib`, and `ThirdParty.Release.lib` under `ThirdParty/Prebuilts/Platforms/VisualStudio2026/Output/`, so this build is required once before it. Build `ThirdParty/Prebuilts/Platforms/VisualStudio2026/ThirdParty.sln` (x64) in Debug, Profile, and Release — from Visual Studio, or from MSBuild with `/p:Configuration=<config> /p:Platform=x64`. After this one-time build, every wrapper session start (step 6) rebuilds all three incrementally so they track primary HEAD.

If provisioning later reports a submodule **pin mismatch**, the worktree predates a primary submodule update — rebase the worktree onto the primary tip so the pins agree (the error message names the command).

## 5. Export primary game data (Shared data mode)

Agent worktree builds default to Shared data mode, which consumes the primary checkout's exported data at `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data/`. Produce it once by building `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.sln` (x64, any configuration) in the primary checkout; its pre-build events build DataPacker and export the data. Later refreshes are usually automatic: each wrapper session start (step 6) re-runs this export in the primary from its current asset files, but skips it when the primary has uncommitted `Tools/`, `ThirdParty/`, `DataPacker/`, or `Common/` changes, or when the session-start DataPacker prebuild does not finish cleanly. When a `BrokenEngineSandbox` client or server is running, session start waits up to its wait budget (500 seconds by default) for you to close it and then exports; only if the game is still open when that budget expires does it skip with a warning. Re-run the build above yourself after a skip.

## 6. First wrapper run

The session wrappers need no one-time coordination-state initialization:

- Claude Code, from Git Bash at the primary checkout root: `./.claude/claude-worktree.sh`
- Codex CLI, from PowerShell 7 at the primary checkout root: `.\.codex\codex-worktree.ps1`

At each session start the wrapper creates a UUID-named worktree (or resolves the existing one when reattaching), writes the session's private-Git receipt into it, rebuilds the shared primary binaries incrementally — the AgentTools executables (WorktreeCli, AgentHarness) and ThirdParty in Debug/Profile/Release — so they always match primary HEAD, re-exports the primary game data with the rebuilt DataPacker, provisions links to the primary ThirdParty/tool outputs, verifies the worktree's `.claude/skills` link resolves, pre-builds the worktree's DataPacker, and launches the agent CLI inside the worktree. Do not bypass the wrapper.

A newly created worktree skips the `Engine/Data/Islands` and `Engine/Data/Textures` source trees (~989 MB) through Git sparse checkout, so those paths are simply absent from disk; Git treats them as unchanged rather than deleted. Run `git sparse-checkout disable` in that worktree to bring them back before editing or adding an asset under either tree — an authorized Local generation build restores them on its own. Reattaching changes nothing here: an existing worktree is never sparsified after creation.

The export runs at any start that does not skip it (step 5), new session or reattach. It can move primary data under sessions already running, failing their next data check and forcing a recompile there, and a rare island bake or another session's DataPacker run can hold session start long enough to time other starts out. An export error stops session start, so a broken committed asset blocks every session — including reattaching to a worktree with a rebase in progress, whose banner is then never shown — until it is fixed in the primary checkout outside an agent session.

No plan-queue bootstrap is required. Executable `Documents/Plans` metadata is tracked in Git; WorktreeCli creates short-lived PC-local claim records only when a session claims a plan or runs a transient operation. `Documents/Features` is manual.
