# Codex main-session delivery of the Change Workflow reference

Open question: how a Codex CLI main session receives the Change Workflow and
delegation-roles text now that it no longer lives in root `AGENTS.md`. No
mechanism is decided here; this document records the gap, the candidates, and
the check that would settle any of them. A Plan is minted only once a mechanism
is chosen.

## The gap

- The Change Workflow and delegation-roles sections moved verbatim out of root
  `AGENTS.md` into `.agents/references/change-workflow.md`, with the risk-tier
  definitions in `.agents/references/risk-tiers.md`. Root `AGENTS.md` keeps
  only the three session rules that also bind subagents, and no longer
  references either file.
- Claude Code main sessions get the moved text automatically from the
  `SessionStart` hook entries in `.claude/settings.json`. `SessionStart` never
  fires inside a subagent, which is exactly the intent: workers follow their
  brief instead of paying for the workflow text.
- Codex CLI sessions — launched through `.codex/codex-worktree.ps1`, configured
  by `.codex/config.toml`, with worker roles in `.codex/agents/*.toml` — load
  root `AGENTS.md`, and so do Codex subagents. Neither now receives the moved
  text automatically, and root `AGENTS.md` no longer points at it, so a Codex
  main session can make tracked changes without the workflow in context.

## Candidate mechanisms

1. **`developer_instructions` in `.codex/agents/*.toml`.** Present today on all
   four role files (for example `.codex/agents/opus.toml:3`). It is a per-worker
   channel, so it reaches dispatched agents and not the main session — the exact
   inverse of what is needed. Rejected as a main-session channel; noted so a
   later reader does not re-propose it.
2. **A Codex config-level or launch-time instruction channel.** Would deliver
   the reference file to the main session automatically, matching the Claude
   hook's behaviour. To be verified against the Codex CLI documentation for the
   installed version, not assumed: whether `.codex/config.toml` accepts an
   instructions-file key; whether `-c` config overrides or extra client
   arguments passed through `Start-AgentWorktreeSession.ps1` (which forwards
   `$ClientArguments` to the client executable) can carry one; and whether the
   installed Codex version has any session-start hook equivalent. Also verify
   whether any such channel is main-session-only or leaks into Codex subagents,
   since leaking would forfeit the context saving the move bought.
3. **A Codex-side pointer or read-on-start.** A Codex-only instruction, placed
   where Codex main sessions read it and subagents do not (not root
   `AGENTS.md`, which every Codex subagent loads), telling the main session to
   read both reference files on start. Costs almost nothing to build, but
   delivery depends on the session obeying it, and the text is absent until it
   does — including after a context compaction.

Candidate 2 is the only one that matches the Claude behaviour; candidate 3 is
the fallback if candidate 2 has no verified support. Both remain open.

## Acceptance check for whichever mechanism is chosen

Both observations are required; either alone does not settle it.

1. A fresh Codex main session in the worktree, asked to quote verbatim the first
   heading of the Change Workflow section in its context and name the file it
   came from, quotes
   `## IMPORTANT: Change Workflow (YOU MUST follow this when changing anything tracked in this repository)`
   and names `.agents/references/change-workflow.md`, and quotes the
   `### Risk tiers` heading naming `.agents/references/risk-tiers.md`.
2. A Codex subagent asked the identical question, with no other context,
   reports it absent.
