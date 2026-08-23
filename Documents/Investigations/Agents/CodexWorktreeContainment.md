# Codex worktree containment

## Question

What enforceable containment policy should protect a Codex worker's filesystem,
process, and network scope, how should primary-branch protection and the
landing broker be divided, and which negative tests prove the boundary?

## Current evidence

`.codex/codex-worktree.ps1` starts a worktree session with a deliberate
dangerous bypass for the interactive host, while `.codex/codex-review.ps1`
launches headless review with `--sandbox read-only` and an explicit worktree.
The root `AGENTS.md` landing workflow owns a lease and compare-and-swap
advance of primary, and `/finalize-changes` owns the landing scripts. These are
different protection layers; the repository does not yet compare their
enforceability as one policy or define a negative-test matrix for escapes.

## Options to compare

1. CLI sandbox plus repository/worktree checks. This reuses the current host
   mechanisms, but leaves process and network enforcement dependent on the
   CLI's documented guarantees.
2. OS-level containment around each worker (filesystem ACL/job boundary and
   explicit network policy), with the current landing scripts as the only
   primary writer. This strengthens enforcement but adds host-specific setup
   and maintenance.
3. A brokered model: workers remain read-only or worktree-scoped, while a
   narrow landing broker validates a receipt and owns every primary mutation.
   This centralizes primary protection but requires a precise broker protocol
   and failure/lease behavior.

## Evidence to collect

- For each option, the enforceable filesystem roots, process creation/kill
  rights, network allow/deny behavior, and behavior on a path or command that
  crosses the boundary.
- Which protection is primary: CLI sandbox, OS policy, or broker; what happens
  when one layer is unavailable or disagrees; and which existing landing lease
  and compare-and-swap steps remain authoritative.
- A negative-test matrix covering reads/writes outside the worktree, primary
  writes before confirmation, process escape, network access, symlink/reparse
  traversal, stale receipts, and interrupted landing. Tests must inspect
  effects without mutating real primary state.
- Host portability, setup cost, evidence retention, and the smallest policy
  that protects the existing workflow.

## Promotion criteria

Promote to a Tier 3 Plan only after one option is selected with an enforceable
filesystem/process/network policy, an explicit primary-protection owner, a
landing-broker boundary (or a documented reason it is unnecessary), and a
passing negative-test matrix. The Plan must name the exact scripts and trust
boundaries; it must not rely on prose claims about sandbox behavior alone.

## Non-goals

- No change to worktree launch flags, landing scripts, network policy, or
  primary state is authorized here.
- No credential handling, prompt/transcript capture, or engine/runtime change.

## Notes

The current read-only review launch and the current host bypass are evidence of
two different execution contexts, not proof that either is sufficient for all
worker actions. The investigation must compare them before proposing a new
enforcement layer.
