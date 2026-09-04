<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T21:09:52.909Z","dependsOn":["Documents/Plans/Engine/ExecutionCardChecker.md"]} -->
# Emit vcxproj candidates and the triggered acceptance-table skeleton from the session inventory

## Context
Two hygiene triggers are computed by hand today, and each has a silent-miss
failure mode.

`update-affected-code/references/worker.md:59-63` requires an `/update-vcxproj`
handoff "for every added or removed C++/GLSL file and every existing C++ file
that gained or lost a whole-file `BT_CLIENT`/`BT_SERVER` guard". The first half
is already computed: `Get-RoutingTrigger` sets `updateVcxproj` from
`$sourceMembership`, the add/delete/rename/type-change classes
(`.agents/scripts/Get-SessionChangeInventory.ps1:263-266`, `:272`, `:278`). The
guard half is computed nowhere, although it is a deterministic
baseline-versus-working comparison of the same two blobs the inventory already
resolves.

`finalize-changes/references/worker.md:79-85` has the finalizer fill the landing
acceptance table "from the hygiene handoffs that already exist — the
session-change inventory's `triggers` object reports which ones the session
owes: `/code-style-review` for changed C++, `/update-vcxproj`,
`/validate-skill`, `/update-claude-docs`, `/progressive-disclosure-review`" and
give each missing one its own row.
`finalize-changes/references/landing-acceptance-table.md:92-103` adds the
Executable Plan check row, required whenever the diff touches
`Documents/Plans/**`. Nothing stops a triggered row from being silently omitted:
the agent transcribes the trigger set by hand.

## Design
1. Add `vcxprojCandidates` to the `triggers` object returned by
   `Get-RoutingTrigger` (`:255-284`): an array of `{path, reason}` rows, `reason`
   `membership` for a path already counted by `$sourceMembership`, and `reason`
   `guard` for an existing C++ file whose baseline and current blobs differ in
   whether the whole file sits inside a `BT_CLIENT` or `BT_SERVER` guard. A
   `guard` row also sets `updateVcxproj` to `$true`, because the skill must run
   for it; that is the behavior change this half makes.
2. Add an `-EmitAcceptanceSkeleton` mode emitting one row per triggered hygiene
   check — `/code-style-review`, `/update-vcxproj`, `/validate-skill`,
   `/update-claude-docs`, `/progressive-disclosure-review`, and the Executable
   Plan check when `planTouched` — each with `status: BLOCKED` for the agent to
   overwrite with evidence, plus one row per approved criterion read from the
   execution card that this Plan's prerequisite makes machine-readable.
3. Shrink `finalize-changes/references/worker.md:79-85` to "fill the emitted
   skeleton", delete the hand-transcribed trigger list there, and reduce the
   trigger sentence in `landing-acceptance-table.md:92-103` to the row the
   skeleton emits, keeping the WorktreeCli `plan validate` invocation and its
   `--lint-only` conditions unchanged. Replace
   `update-affected-code/references/worker.md:59-63` with reading
   `vcxprojCandidates`. Those deletions are the point of this Plan.

## Critical files
- `.agents/scripts/Get-SessionChangeInventory.ps1`
- `.agents/skills/update-affected-code/references/worker.md`
- `.agents/skills/finalize-changes/references/worker.md`
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md`

## In scope
- `Get-RoutingTrigger` (`:255-284`) and the emission that calls it (`:664-666`)
- The new `-EmitAcceptanceSkeleton` mode and its row fields
- `update-affected-code/references/worker.md:59-63`
- `finalize-changes/references/worker.md:79-85`
- `finalize-changes/references/landing-acceptance-table.md:92-103`, the trigger
  statement only

## Out of scope
- `verify-acceptance/references/worker.md:12-25` and `:27-28`, and its `## Rules`
  at `:38-49` — what counts as evidence, what makes a duplicated check
  independent, and the read-only rules all stay prose
- Deciding any row's verdict, or replacing a `BLOCKED` row with evidence
- The receipt's existing usability reporting — `status` and `truncated` — which
  this Plan neither reshapes nor adds a field beside
- A `coherenceReview` trigger, and any resolver that turns the trigger set into
  the Step 3-7 dispatch list: neither exists, and the Plan that proposed them was
  completed by reducing the duplicated workflow prose to one owning statement
  instead, so this Plan adds neither
- Project XML itself: the inventory names candidates, `/update-vcxproj` still
  owns membership, filters, and exceptions
- The WorktreeCli `plan validate` run required at
  `landing-acceptance-table.md:94-103`, which the inventory never runs or
  replaces (`landing-acceptance-table.md:38-41`)
- Declined by the audit that produced this Plan: the tier pre-classifier that
  would flag Tier-3 path patterns such as `Common/Crc.h` or
  `Engine/Source/Network/` — build it only if missed escalations are observed

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one tool plus two consuming skills); this
author's classification, confirmed by main at Step 1. Invariants to preserve:
`triggers` keeps every existing boolean field and its name, because
`/finalize-changes` reads them at `worker.md:79-82`; counts and triggers keep
describing the complete inventory, never the truncated emission (`:664`); the
schema name `broken-engine-session-change-inventory/v1` and the 0/1/2 exit
mapping are unchanged; no triggered row can be omitted from the skeleton.

## Acceptance criteria
- A C++ file that gains a whole-file `BT_SERVER` guard with no membership change
  appears in `vcxprojCandidates` with `reason: guard` and sets `updateVcxproj`
  to true; a file whose guard is unchanged appears in neither
- An added `.cpp` appears with `reason: membership`
- For a session that changed C++, one `SKILL.md`, and one Plan, the skeleton
  carries exactly the rows for `/code-style-review`, `/update-vcxproj`,
  `/validate-skill`, `/update-claude-docs`, `/progressive-disclosure-review`, and
  the Executable Plan check, each `BLOCKED`, matching by hand what
  `finalize-changes/references/worker.md:79-85` produces today
- For a docs-only session the skeleton carries no `/code-style-review` row
- `Validate-Skill.ps1` reports `VALID` for every changed `SKILL.md`
