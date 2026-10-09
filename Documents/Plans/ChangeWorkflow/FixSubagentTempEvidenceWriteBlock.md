<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T22:55:21.125Z","dependsOn":[]} -->
# Fix: root `AGENTS.md` report-file exemption — Claude Code rejects a subagent's `Write` of a `Temp/` handoff-evidence file the exemption covers

## Context

Observed symptom. Main's brief for a harness `implementer` worker required the worker to write its run analysis to `Temp/NextPlan/EstimateLatestServerTickThroughArrivalGaps/harness-westcoast/analysis.md` and cite it under `Evidence`. The worker's host `Write` of that path failed with the tool error `Subagents should return findings as text, not write report files. Include this content in your final response instead.` The worker put the analysis into its handoff instead, and the file the brief asked for was never created. The same rejection hit a later dispatch's `Write` of `Temp/NextPlan/EstimateLatestServerTickThroughArrivalGaps/harness-westcoast-2/analysis.md`. In those same runs, the same workers' `Write` calls for `*.ps1` helper scripts under the same directories succeeded. So did other implementers' `Write` calls for `Temp/FollowUpPlans/*.body.md` Plan bodies in this session.

Root `AGENTS.md:12` (`## Environment`) says "Claude Code's host note against writing report files exempts every `Temp/` file a handoff cites under `Evidence`", and `.agents/references/subagent-handoff.md:23-40` (`## Handoffs`) tells a worker to move oversized material to a `Temp/` file with one `##` heading per section and cites that exemption. Subagents do receive the exemption: root `AGENTS.md` is loaded into a subagent's context, and `.claude/agents/implementer.md` adds nothing that withholds it. The gap is that the rejection is a tool-level error from the host, not only a prompt note, and repository prose cannot lift it. The exemption therefore promises a `Write` the host can refuse. No repository hook produces the message; `.claude/settings.json` has no matching hook.

The misbehaving files are outside the claimed Plan's `## In scope`, which covered engine networking code and its documentation only.

Session provenance (machine-local; not reproducible after cleanup). The Client through Worktree fields name the session that observed the friction — the session `/next-plan-review` must reach — while the `Landing ref` line names a ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 7e1a7312-98f6-437d-ad5c-61651f58a4f3
- Worktree/branch UUID: e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Session branch: claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Worktree: .claude\worktrees\BrokenEngine\e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Landing ref: claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97 (the observing session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/FixSubagentTempEvidenceWriteBlock.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`. Only when the transcript is genuinely needed, in a new session run `/next-plan-review claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97` in bounded friction mode, supplying client `claude` and the conversation session ID above. Then make the smallest fix inside the `## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

The root-cause step must find what the host keys the rejection on: the filename, the extension, the content, or the subagent context. The observed accepted and rejected paths above narrow it, but they do not settle it. Recommended shape, to be confirmed by that step: rewrite the exemption line in root `AGENTS.md` `## Environment` so that it states the host's refusal is enforced for subagent `Write` calls and gives the one route that reliably creates a cited `Temp/` evidence file. Depending on the finding, that route is either a naming convention that the host accepts or a shell write. A shell write is acceptable for an untracked `Temp/` file, because the BOM, CRLF, and trailing-newline rationale for using `Write` applies to tracked files. Change the sentence in `subagent-handoff.md` `## Handoffs` that cites the exemption only if its wording becomes wrong. The fix belongs in this repository's instructions. The host behavior is outside the repository and cannot be changed here.

No C++, determinism/CRC, wire, serialization, `.pack`, replay, or build surface changes.

## Critical files

- `AGENTS.md` — `## Environment`, the report-file exemption line (`:12`)
- `.agents/references/subagent-handoff.md` — `## Handoffs`, the overflow-file paragraph (`:23-40`), only if its wording becomes wrong

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the root `AGENTS.md` `## Environment` exemption line and, only as `## Design` bounds it, the overflow-file paragraph of `subagent-handoff.md` `## Handoffs`

## Out of scope

- The landed change the session produced
- `.claude/agents/*.md` agent definitions and `.claude/settings.json`
- The Claude Code host itself
- Individual skills' handoff sections and ad hoc brief wording
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Tier 2. Trigger: scoped tool behavior. The change alters how every Claude subagent persists `Temp/` evidence files, but it is confined to one root `AGENTS.md` line and one shared reference paragraph, with no build or bootstrap coordination. Codex and OpenCode behavior must remain unchanged. Never embed transcript paths or home paths.

## Acceptance criteria

- A Claude `implementer` subagent that follows the documented route creates a `Temp/` evidence file named the way an analysis file would be (for example `Temp/<dir>/analysis.md`), and the host raises no report-file error.
- The root `AGENTS.md` exemption line no longer states anything about the host's refusal that the host contradicts.
- The static-checks runner, invoked as `.agents/references/change-workflow.md` `#### Step 5 — Run targeted pre-review checks` documents it, reports every row the change triggers passing.
