# Explicit Primary Commit

Load only for an explicitly requested non-session commit on the primary
checkout. Require primary checkout identity and a clean authorized scope.
`../scripts/Invoke-FinalizeCandidateCommit.ps1 -Route primary-commit` uses a temporary
index to build the commit without moving the primary ref or real index.

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
confirmation` section. Only its affirmative response permits resuming the
landing script with `-AdvancePrimary`. Advance the named ref by guarded CAS with
guarded rollback on postcondition failure. When the commit changes
`Documents/Plans/**`, run WorktreeCli `plan validate` afterwards.
