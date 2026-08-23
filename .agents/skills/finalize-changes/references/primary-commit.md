# Explicit Primary Commit

Load only for an explicitly requested non-session commit on the primary
checkout. Require primary checkout identity and a clean authorized scope.
`../scripts/Invoke-FinalizeCandidateCommit.ps1 -Route primary-commit` uses a temporary
index to build the source candidate without moving the primary ref or real index.
Before verification it runs the read-only history Contract against the expected
primary and source candidate through the exact `RepositoryRoot,BaseCommit,TipCommit,DateUtc,OutputDirectory`
producer interface (Contract supplies the first three values) and returns the typed receipt plus compact generator,
capture/runtime, patch, and mode identities. Contract writes no tracked history and
holds no landing lock.

Bind `/verify-changes` to that candidate with one dispatch in the primary
checkout — no wrapper session, temporary branch, or detached checkout:
`../../codex-review/scripts/New-CodexReviewPrompt.ps1 -AssignedSkill
verify-changes -Baseline <primary tip SHA> -Head <candidate SHA>` with the
usual `-RepositoryRoot`, `-ScopeFile`, and `-PromptPath`, then
`.codex/codex-review.ps1` on the receipt's prompt. The candidate is
uncommitted by design, so a reviewed path the landing inventory reports dirty
whose bytes match the reviewed `-Head` tree is expected state and classifies as
`intentionally persisted`.

After `/verify-changes` binds acceptance to that diff, present the primary
summary and the authoritative confirmation from the skill's `## Landing
confirmation` section. Only its affirmative response permits claiming the normal
3600-second landing lease and resuming the same candidate script with
`-AdvancePrimary`, `-OwnerToken`, `-SessionLabel`, and the approved Contract
scalars. The direct-primary route never performs omitted-token implicit mutation.
Under that lease it re-evaluates Contract/mode, requires the primary tip and aggregate
Contract digest to remain exactly equal to the approved values, and follows the [root
`AGENTS.md` Step 8 landing invariant](../../../../AGENTS.md) and exact primary/landing
mode mechanics in [`references/scripts.md`](scripts.md). The direct-primary route
advances by guarded CAS with guarded rollback on postcondition failure.
When the commit changes
`Documents/Plans/**`, run WorktreeCli `plan validate` afterwards.
