---
name: refactor-candidates
description: Review the refactor candidates log, Documents/Investigations/Engine/RefactorCandidates.md - turn the rows worth doing into executable Plans, remove the rows it settles, and ask the user about the uncertain ones. Use only when the user explicitly invokes this skill.
disable-model-invocation: true
allowed-tools: [Read, Edit, Agent, PowerShell, AskUserQuestion]
---

# Refactor Candidates

## Purpose

Clear the refactor candidates log: rows worth doing become executable Plans,
settled rows leave the log, and uncertain rows go to the user.

## When to use

- The user explicitly invokes this skill to review the log,
  `Documents/Investigations/Engine/RefactorCandidates.md`.

## Steps

1. Read the log. Done when its `## Entries` rows are in context, or `## Entries`
   holds no row and the user has been told so, which ends the run.
2. Dispatch one `implementer` running `/create-follow-up-plans` over every row,
   with the log as `Evidence` and under `Exclusions`, and these points under
   `Fixed decisions`. Done when its handoff carries one `Row verdicts` row for
   every log row.

   - Test a row that says it needs a user decision against Git history, because
     the earlier Plan may have landed.
   - For a row about simulation code, state whether the refactor stays
     bit-identical; any behavior change is a `needs user decision` verdict.
   - A Plan cites a row it came from by the commit that holds that row, because
     the row is being removed and a present-tense citation stops resolving.
   - Return the `Row verdicts` field
     `.agents/skills/refactor-candidates/SKILL.md` `## Handoff` declares.

3. Read the code each `rejected` verdict cites, because deletion loses the row
   from the log. Done when each citation supports its reason, or its row has
   become an open decision asking the user what the citation failed to show.
4. Edit the log to remove every row with a `planned`, `duplicate of`, or
   supported `rejected` verdict, leaving each open decision's row. Done when
   only those rows remain under `## Entries`.
5. Present all results and each open decision under
   [`change-workflow.md`](../../references/change-workflow.md)
   `### User Interaction`. Done when the message gives each Plan path, each
   removed row with its verdict, and each open decision's question, options,
   and evidence.
6. Settle each answered decision. Done when each answered row has left the log
   or gone back to the user.

   - A chosen change gets a fresh step 2 dispatch over that row, carrying the
     user's words as a fixed decision in place of step 2's behavior-change
     verdict, because the answer settles it.
   - That dispatch's `planned` or `duplicate of` verdict removes the row; its
     `rejected` or `needs user decision` result keeps the row and goes back to
     the user.
   - Main removes a declined row; an unanswered row stays.

7. Carry the change through the Change Workflow from its
   `Run targeted pre-review checks` step. Done when `/finalize-changes` has
   landed it with the user's confirmation, or the user declined landing.

## Handoff

The step 2 worker's handoff adds one field, `Row verdicts`, with one row for
each log row, because main removes or asks about every one. Each row opens with
the first line of its log row:

```text
<first line> — planned: <Plan path>
<first line> — rejected: <reason with file:line>
<first line> — duplicate of: <Plan path>
<first line> — needs user decision: <question, options, evidence>
```

## Rules

- Leave the row investigation to the worker: main reads only the log and the
  citations step 3 checks.
