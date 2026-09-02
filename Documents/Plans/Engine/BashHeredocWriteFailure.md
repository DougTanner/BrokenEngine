<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-02T00:07:39.243Z","dependsOn":[]} -->
# Fix: Bash tool multi-line file writes — quoted heredoc fails with `unexpected EOF while looking for matching '` on Windows

## Context
In a Claude Code `/next-plan` session on Windows, where the Bash tool runs Git Bash, two Bash tool calls that wrote a multi-line Markdown file through a quoted heredoc failed before executing anything. The first ran `cat > Temp/next-plan-compile/plan.md <<'EOF' ... EOF` and the second ran `cat >> Temp/next-plan-compile/plan.md <<'PLANEOF' ... PLANEOF`. Both returned exit code 2 with `/usr/bin/bash: -c: line 71: unexpected EOF while looking for matching `''` for the first call and the same message at `line 47` for the second, so neither call created or appended any content.

Both heredoc bodies were Markdown containing backtick code fences, single quotes inside a quoted pattern such as `'warning [A-Z]+\d+'`, and angle-bracket placeholders such as `<retainedLog.path>`. Each failure cost one retry: the identical content was re-issued through the host Write tool and succeeded both times, so the workaround was proven but the trigger was not.

Which character sequence inside a quoted heredoc the host Bash tool mangles on Windows is not established from this session's evidence. The checkpoint reviewer classified both occurrences as a fixable defect and attributed the emitter to `.agents/skills/next-plan/SKILL.md`, but that skill prescribes no heredoc, and no tracked repository instruction found in this session prescribes one either. This Plan is therefore investigation-first: the cause must be reproduced before any instruction is changed.

Session provenance (machine-local; not reproducible after cleanup). The Client through Worktree fields name the session that observed the friction — the session `/next-plan-review` must reach — while the `Landing ref` line names a ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4bf5c39a-5f59-4155-b3c4-3dcb84ccf327
- Worktree/branch UUID: 90537e83-2783-43e3-af8a-807253d96b0d
- Session branch: claude/90537e83-2783-43e3-af8a-807253d96b0d
- Worktree: .claude\worktrees\BrokenEngine\90537e83-2783-43e3-af8a-807253d96b0d
- Landing ref: claude/90537e83-2783-43e3-af8a-807253d96b0d
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic Plan-history squash can make it return an unrelated aggregate commit, so review its result only when the commit is attributable to one session alone (its diff limited to that session's files); never review an aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above: Claude review requires the exact conversation session ID above.

## Design
In a new session, run `/next-plan-review claude/90537e83-2783-43e3-af8a-807253d96b0d`, supplying client `claude` and the conversation session ID recorded above. Root-cause the friction from the proven transcript, then make the smallest fix inside the `## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

As a starting approach rather than a decision, the author recommends first reproducing the failure with a minimal quoted heredoc in the same host and shell, narrowing the body down to the single character sequence that triggers `unexpected EOF while looking for matching '` — the candidates the two failing bodies share are backtick code fences, an embedded single quote, and angle-bracket placeholders. The rationale is that the workaround is already proven while the trigger is not, so identifying the sequence is what decides whether any instruction change is warranted at all.

Once the trigger is known, the author recommends one of two outcomes and no more. If no tracked instruction prescribes a heredoc, record a single guidance line at the layer that owns tool-usage rules, stating that a multi-line file under `Temp/` is written with the host Write tool rather than a quoted heredoc. If the review instead finds a tracked instruction that does prescribe the failing form, fix that instruction in place and add no new rule. If the review proves the failure is purely a host-side defect with no repository instruction able to prevent it, recording that conclusion and changing nothing tracked is an acceptable outcome.

## Critical files
- `AGENTS.md` — the repository-wide directive layer that owns command and tool-invocation guidance; the only place a general "write multi-line `Temp/` files with the host Write tool" rule would belong.
- `.agents/skills/next-plan/SKILL.md` — the skill the checkpoint reviewer named as the emitter; inspect it for any instruction that leads to the failing form and fix it there instead if one exists.

## In scope
- Root-cause investigation via /next-plan-review, run with client `claude`, the review ref named in `## Design`, and the recorded conversation session ID
- Minimal reproduction of the quoted-heredoc failure sufficient to name the offending character sequence
- The smallest resulting instruction fix, confined to the tool-invocation guidance in `AGENTS.md` and to any heredoc-producing instruction in `.agents/skills/next-plan/SKILL.md`

## Out of scope
- The landed change the observing session produced
- Changes to Git Bash, the host Bash tool, or any host tool implementation
- New scripts, wrappers, or helper abstractions for writing files
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation-only instruction wording) if the fix is a single guidance line that changes no workflow step, delegation, or contract; escalate to Tier 2 if the fix changes a skill's behavior as the root `AGENTS.md` Step 2 trigger defines behavior. Never embed transcript paths or home paths.

## Acceptance criteria
- The offending character sequence is named from a reproduction, not inferred
- The recorded symptom no longer reproduces for an agent following the resulting repository instruction, or the review records that no tracked instruction could have prevented it and nothing tracked changes
- `/validate-skill` passes if `.agents/skills/next-plan/SKILL.md` changes; plan validate exits 0

## Notes
No duplicate exists: a search of `Documents/` for heredoc, here-string, Write-tool, and `unexpected EOF` terms returned no match, and no Plan under `Documents/Plans/Engine/` owns Bash-tool file-writing friction.
