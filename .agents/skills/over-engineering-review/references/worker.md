# Over-Engineering Review Worker

The finder and validator steps and the judgment rules for
`/over-engineering-review`. The public contract main reads — passes, inputs, and
handoffs — is [`../SKILL.md`](../SKILL.md).

## Steps

The finder pass runs steps 1-3; the validator pass runs steps 4-8.

1. Fix the scope with the read-only inventory.
   - Create `Temp/` if absent, in a call of its own, then run
     `pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1
     -RepositoryRoot '<absolute repository toplevel>' -Baseline <full
     40-character SHA> -Regions -OutputPath
     Temp/over-engineering-review-inventory.json`.
   - Add `-IncludeUntracked '<comma-separated paths>'` when the brief supplies
     untracked paths.
   - The run prints one summary line naming its status, code, and message.
     Read the file only after that line; only `status` `pass` is usable.
   - Keep the `entries` rows whose `class` is `cpp`, and the `regions` rows on
     those paths.
   - Done when the kept regions are fixed, or the unusable run is reported as
     `BLOCKED`.
2. Search the kept regions for every class in `### Candidate classes`, reading
   code outside them only to build a proof. Leave out every protected check.
   Done when every kept region has been searched.
3. Write one row per candidate under `## Removal candidates` in
   `Temp/over-engineering-review-candidates.md`, in this form:

   ```text
   <ID> <path:line> — class: impossible-state | unreachable-fallback | ultra-rare | unread-hash | speculative-generality — <claim> — proof: <callers, writers, readers, or occurrence evidence relied on> — form: delete | ASSERT | failure channel
   ```

   Then return the finder handoff in `../SKILL.md` `## Handoff`, recording each
   root-cause design suggestion. Done when it is the final answer.
4. Read the candidate rows at the `Removal candidates` input's path plus
   selector, in the step 3 form, and confirm they, the inventory output file,
   and the authorization source are in hand. Done when all three are, or the
   missing one is reported as `BLOCKED`.
5. Locate each candidate's code by symbol and confirm it still exists. Done when
   each candidate is located, or dropped as gone.
6. Re-derive each candidate's proof independently to its group's standard in
   `### Proof standard`, then test it against every reason in
   `### Drop reasons`. Judge every candidate before applying any edit, so the
   inventory file's line numbers still match the code. Done when each candidate
   is proven or dropped with its reason.
7. Apply each proven candidate's form as the smallest edit that replaces the
   code with that form, then reread each edited file. Done when every proven
   candidate is applied and every edited file has been reread.
8. Return the validator handoff in `../SKILL.md` `## Handoff`. Done when it is
   the final answer.

## Rules

- Never delegate. The finder reports only and edits no tracked file; the
  validator edits only proven sites and returns builds and every review to main.
- Remove only definitely useless code: a proof that is incomplete drops the
  candidate.

### Candidate classes

Each class is owned elsewhere; read the owner and apply it.

- Checks for impossible states, unreachable fallbacks (OS or API version
  fallbacks included), ultra-rare-event protection, and unread hashes: the
  "Error handling at trust boundaries only" bullet of
  [`../../../references/cpp-conventions.md`](../../../references/cpp-conventions.md).
- Speculative generality — an option, parameter, verb, or function with no
  current consumer: the minimum-sufficient-change and KISS/YAGNI bullets of root
  `AGENTS.md` `## Directives`.
- The replacement form (delete, `ASSERT`, or the boundary's failure channel):
  the "No useless ASSERTs" bullet of `cpp-conventions.md`, and
  `.agents/skills/repo-code-review/references/checks.md` `### ASSERT behavior`.
- How rare an ultra-rare event is: the occurrence-evidence classes in
  `.agents/skills/plan-simplicity-review/references/worker.md` `## Rules`, and
  question 1 of its `## Review questions`.
- Root-cause design suggestions: question 6 of that `## Review questions`.

### Proof standard

- Proof group — impossible-state, unreachable-fallback, unread-hash, and
  speculative-generality candidates: prove unreachability, or that no reader or
  consumer exists.
  - Read every caller repository-wide with `git grep` outside `ThirdParty/`,
    applying the submodule caveat in root `AGENTS.md` `## Environment`, every
    writer of the guarded state, every reader of the hash or digest, and the
    invariant that makes the case impossible.
  - Reading every caller, writer, and reader is required work, never a reason
    to drop.
- Ultra-rare group — ultra-rare candidates: the code matches a class the
  "Error handling at trust boundaries only" bullet names, it is not a protected
  check, and no `observed` or `credible exposure` evidence of the event exists.
  The event is reachable by definition, so no unreachability proof is needed.

### Drop reasons

Drop a candidate, giving its reason, when:

- proof group: any caller, writer, or path makes the case reachable, or any
  reader or consumer exists;
- proof group: the proof needs runtime evidence, or behavior the code does not
  show, such as an OS or driver behavior the repository does not document;
- ultra-rare group: the code matches no class the bullet names, or `observed`
  or `credible exposure` evidence of the event exists;
- the edit would touch code outside the session-changed regions in the
  inventory output file, including a declaration whose users lie outside them;
- the edit would change on-disk, wire, save, replay, or Collection layout: a
  Tier-3 surface (`.agents/references/risk-tiers.md` `### Risk tiers`) that
  needs a format version bump (the backward-compatibility bullet of root
  `AGENTS.md` `## Directives`). Also list it as a validator design suggestion;
- the candidate contradicts an `AGENTS.md` rule or
  `Documents/C++StyleGuide.txt`, per
  [`../../../references/authority-order.md`](../../../references/authority-order.md);
- the approved plan, the execution card, or a user instruction requires the
  check, per `authority-order.md`;
- the candidate weakens a protected check.

### Protected checks

The finder proposes none of these, and the validator drops any candidate on
one:

- Trust-boundary validation and file reads, and the check that detects an OS or
  third-party API failure together with its route to the boundary's failure
  channel: the "Error handling at trust boundaries only" bullet of
  `cpp-conventions.md`, and the trust-policy bullet of root `AGENTS.md`
  `## Key Patterns`. Only that detection and route are protected; a fallback,
  retry, or extra durability layered on the check is a candidate under its
  class.
- The determinism CRC and every input to it: the opening paragraph of root
  `AGENTS.md`.
- Format and version checks: the backward-compatibility bullet of root
  `AGENTS.md` `## Directives`, and `Engine/Source/File/AGENTS.md`
  `## File Contracts`.
- Atomic one-shot writes: `Engine/Source/File/AGENTS.md` `## File Contracts`.
