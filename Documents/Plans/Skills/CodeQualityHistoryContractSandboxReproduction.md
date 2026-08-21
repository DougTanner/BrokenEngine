<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T12:59:42.706Z","dependsOn":[]} -->
# Fix: code-quality history Contract mode — no typed JSON and no usable error inside the review sandbox

## Context

At a `/next-plan` landing gate, the `/verify-changes` reviewer dispatched through
`/codex-review` (read-only sandbox) tried to independently reproduce the frozen
`broken-engine-code-quality-history-contract/v1` receipt that approval
preparation had returned. Two things went wrong in that one reviewer round.

First, the reproduction command it was handed omitted the mandatory
`-RepositoryRoot`, and `Invoke-CodeQualityMetricsHistory.ps1` declares it
`[Parameter(Mandatory = $true)]`
(`.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1:6-7`),
so `pwsh -File` failed at parameter binding with no result object. The tracked
template at `.agents/skills/finalize-changes/references/scripts.md:26` does carry
`-RepositoryRoot`, so the truncated form came from the command as assembled for
that reviewer, not from a tracked file; the reference needs no change and this
half of the symptom is recorded as context only.

Second, re-run with the sandbox repository root and `-Mode Contract`, the script
emitted no typed JSON at all — only the single line
`CodeQualityMetricsHistory: True` — and the reviewer returned
`Verification: BLOCKED: frozen code-quality Contract reproduction returned no
typed contract JSON`. That line is the script's own failure channel: `Fail` and
`Write-Diagnostic` write `"CodeQualityMetricsHistory: $Message"` to stderr and
exit 2
(`Invoke-CodeQualityMetricsHistory.ps1:24-25`), reached from the single top-level
`catch { Fail $_.Exception.Message }` at `:531`. The message `True` is not a
sentence because of the Git wrapper's rethrow at `:74`:

```powershell
if ($process.ExitCode -ne 0) { throw (($stderr.Trim()) -or 'git command failed.') }
```

In PowerShell, `-or` coerces both operands to boolean, so a non-empty `git`
stderr becomes the literal `$true` and the real Git failure text is destroyed
before it ever reaches the reviewer. The observed evidence is therefore
consistent with a `git` invocation failing under the sandbox while reporting
nothing recoverable; proving which invocation and why is deferred to the review
named in `## Design`.

Cost: one full blocked `/verify-changes` round. Workaround: the manager ran the
same command host-side, where it emitted the full 13,795-byte receipt with exit
0, inlined that receipt verbatim into the reviewer's scope file, and
re-dispatched; the rerun passed. Both reviewer outputs were retained only as
machine-local `Temp/` files and are not reproducible after cleanup.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: b88a6bb2-6008-407d-b6fb-1669597c61f4
- Worktree/branch UUID: 7d5ecbe4-034b-4ed4-be7a-0df5b811f8f7
- Session branch: claude/7d5ecbe4-034b-4ed4-be7a-0df5b811f8f7
- Worktree: .claude\worktrees\BrokenEngine\7d5ecbe4-034b-4ed4-be7a-0df5b811f8f7
- Landed commit of the observing session:
  a342ffa2dc075356fa1b8e0d169471586e762c92 — this session's own landing, and the
  immutable review ref `/next-plan-review` targets. The `Landing ref` line below
  names the mutable branch only because its tree, unlike this commit's, contains
  this Plan.
- Landing ref: claude/7d5ecbe4-034b-4ed4-be7a-0df5b811f8f7 — the observing
  session recorded this Plan itself, so its own branch tip carries it, and that
  branch tip is the landed commit a342ffa2dc075356fa1b8e0d169471586e762c92 plus
  this Plan.
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Skills/CodeQualityHistoryContractSandboxReproduction.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

In a new session, run
`/next-plan-review a342ffa2dc075356fa1b8e0d169471586e762c92` — the observing
session's landed commit above — supplying the recorded client `claude` and the
recorded conversation session ID `b88a6bb2-6008-407d-b6fb-1669597c61f4`.
Root-cause the friction from the proven transcript, then make the smallest fix
inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The decided mechanism is a single error-path repair in
`Invoke-CodeQualityMetricsHistory.ps1:74`: replace the boolean-coercing
`throw (($stderr.Trim()) -or 'git command failed.')` so the rethrown message is
the trimmed stderr text when that text is non-empty and the literal
`git command failed.` fallback otherwise. A Contract-mode failure then reports
the real Git diagnostic through the existing `Fail`/`Write-Diagnostic` channel
instead of `CodeQualityMetricsHistory: True`, which is what left the reviewer
with nothing to act on.

Nothing else changes. The sandbox's own behavior is not touched. The
reproduction-command template at
`.agents/skills/finalize-changes/references/scripts.md:26` is already correct —
it carries `-RepositoryRoot` — so the binding failure recorded above came from
the command as assembled for that reviewer, not from a tracked file, and no
template edit is authorized. The host-side reproduction route in
`/codex-review` and `/verify-changes` stays exactly as documented and remains
the fallback when a sandboxed reviewer cannot run the producer.

Nothing here may change what the Contract receipt contains, how it is digested,
or how landing binds it; the receipt is a frozen approved contract.

## Critical files

- `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1` —
  the sole authorized fix boundary: the Git rethrow at line 74, whose message the
  `Fail`/`Write-Diagnostic` channel at lines 24-25 and the top-level `catch` at
  line 531 carry to the caller.

## In scope

- Root-cause investigation via /next-plan-review, run with client `claude`, the
  review ref named in `## Design`, and the recorded conversation session ID.
- The smallest resulting fix, confined to the rethrown message at
  `Invoke-CodeQualityMetricsHistory.ps1:74` as decided in `## Design`.

## Out of scope

- The landed change the session produced.
- `.agents/skills/finalize-changes/references/scripts.md` and its Contract
  reproduction template, which is already correct.
- The `/codex-review` and `/verify-changes` host-side reproduction route and
  every other skill body; the review sandbox's own behavior and configuration.
- The history receipt's contents, schema, digests, byte contract, and every
  landing-time guard that binds them; `-Mode Generate` and the SVG/JSONL
  production path.
- Any other part of `Invoke-CodeQualityMetricsHistory.ps1`, including its
  metric computation, history parsing, and capture-mode classification.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches the landing
history contract, build/bootstrap coordination, or any primary-advance guard.
Never embed transcript paths or home paths. The invariant that must survive: a
successful Contract run still writes exactly one canonical JSON receipt to
stdout with no absolute or volatile paths, and its receipt digest is unchanged.

## Acceptance criteria

- The recorded symptom no longer reproduces: a forced failing `git` invocation
  makes Contract mode report that invocation's own stderr text, not
  `CodeQualityMetricsHistory: True`, and a failure with empty stderr reports
  `git command failed.`.
- Contract mode on a valid `RepositoryRoot`/`BaseCommit`/`TipCommit` triple still
  emits its typed `broken-engine-code-quality-history-contract/v1` JSON on
  stdout, byte-identical to what the same inputs produced before the change.
- plan validate exits 0.
