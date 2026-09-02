# Update AGENTS.md Documentation — Worker

Steps and rules for the dispatched `implementer`. The public `SKILL.md` owns
the modes, the inputs, and the handoff.

## Steps

1. Fix the mode from the request and confirm both required inputs are present;
   request only a missing input. Done when the mode, the changed-file list, and
   the session baseline are all fixed.
2. Run the discovery script once with the caller's changed-file list, from the
   repository root:

   ```powershell
   pwsh -NoProfile -File .agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1 <changed path> [<changed path> ...]
   ```

   - Pass the paths as separate arguments; a relative one resolves against the
     repository root, never the shell's working directory.
   - Exit `0` with `status` `pass` returns the payload below; exit `1` with
     `status` `error` carries the failing `code` and `message` and nothing
     usable.
   - A list that exceeds the output cap reports a nonzero `omittedCount`; rerun
     with fewer changed paths when something you need is omitted.
   - Never reconstruct these operations inline — walking the hierarchy,
     sweeping stub pairs, or measuring sizes by hand.
   - Done when a `pass` payload is in hand or the `error` code is reported.
   - `chains` — per changed path, the root-to-leaf governing `AGENTS.md`
     documents in `documents`, the nearest one in `governing`, and
     `hubCandidates`, the immediate descendant documents whose duplicated
     guidance a hub edit can make stale.
     - A directly named documentation path governs itself.
   - `sizes` — the `bt-token-v1` size of each chain document with its `hub` or
     `leaf` classification and target (4,000 and 2,000; the repository-root
     `AGENTS.md` alone uses 8,000), plus each chain's `totalTokens` against the
     15,000 target and 20,000 warning.
     - These deterministic normalized-byte estimates are advisory, not exact
       model tokens, and no verdict authorizes trimming: reduce only prose this
       change affects, and report pre-existing unrelated excess without
       trimming it.
   - `stubPairs` — the repository-wide bidirectional pairing sweep, excluding
     `ThirdParty/`, `Documents/Plans/`, `Documents/Features/`, the linked
     `Engine/Source/Graphics/Managers/*.AGENTS.md` references, and local
     overrides.
     - `stub.missing` is a directory `AGENTS.md` with no sibling `CLAUDE.md`,
       `stub.orphan` a `CLAUDE.md` with no same-directory `AGENTS.md`,
       `stub.malformed` a stub whose bytes are not exactly `@AGENTS.md` plus
       one line ending.
     - Fix a reported defect inside the authorized scope in the same edit;
       report one outside that scope as a residual.
3. Read every governing document and relevant parent or sibling rule before
   deciding whether to edit. Compare the current code and changed regions
   against present-tense documentation. Done when every `chains` document has
   been read.
4. In an audit mode, read [`audit-mode.md`](audit-mode.md) completely and emit
   the quality report using its discovery boundary, rubric, report format, and
   content examples. Done when the report is written; in audit mode, return the
   handoff here without editing.
5. Update only affected sections, applying
   [`content-rules.md`](content-rules.md) to every sentence you write or remove.
   - Create a directory `AGENTS.md` only for a distinct subsystem, and create
     its sibling `CLAUDE.md` stub in the same edit. Never create directory
     memory for a single-file utility.
   - In audit-and-fix mode, apply only the improvements the request already
     authorized, show the affected diffs, and add no further approval pause.
   - Done when every affected section is either edited or deliberately left
     alone.
6. Re-read edited files, verify links, rerun the script to confirm stub bytes,
   and inspect the session-baseline diff limited to the authorized paths; in
   audit-and-fix mode also recheck scores, links, sizes, and stub integrity.
   - If a directory is no longer a distinct subsystem, report its `AGENTS.md`
     and stub as a proposed deletion rather than deleting them without
     authority.
   - Done when each check has run and its result is in the handoff.

## Rules

- Prefer no edit when the guidance remains correct. A method, member, local
  refactor, or ordinary bug fix normally needs no new prose. Document a new
  subsystem, cross-cutting pattern, or non-obvious invariant only when omitting
  it would cause a future editor to make a worse decision.
- Treat a new or meaningfully changed algorithm as a candidate only when
  correctness or performance depends on a non-obvious mathematical, numerical,
  coordinate/grid, ordering, or hardware assumption. Source comments own local
  rationale; AGENTS.md owns the subsystem constraint needed for future design
  decisions.
- Do not trim, rewrite, or normalize unrelated sections discovered during
  inspection.
- Choose the simplest resolution for minor wording and organization choices.

## References

- [`content-rules.md`](content-rules.md) — what AGENTS.md prose may say, and
  what it must not.
- [`audit-mode.md`](audit-mode.md) — audit discovery boundary, rubric, report
  format, and content examples.
