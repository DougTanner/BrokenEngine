<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T23:47:13.891Z","dependsOn":[]} -->
# Give `runtime-data-mode.md` the structure its size requires

## Context

`.agents/skills/compile/references/runtime-data-mode.md` is a flat document: a
single `#` title on line 1 and 32 further lines of bullets and paragraphs, with
no `##` heading anywhere in the file and therefore no table of contents.

It is over the reference-size threshold that makes a table of contents
mandatory. Measured with the repository metric:

```
pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 -Path .agents/skills/compile/references/runtime-data-mode.md
```

- 2135 `bt-token-v1` at commit `f1c59ec50ce24eda4c8cbe60796530b37d6abed7`
- 2313 `bt-token-v1` in the observing session's worktree, after two authorized
  sentences were added to the existing bullets on lines 6 and 29

The overage is therefore pre-existing: it reproduces at the baseline commit
above with no session change applied, and the two sentences added afterwards
neither created it nor are the reason it exceeds the threshold.

The rule is `.agents/skills/progressive-disclosure-review/references/worker.md:43-44`
— "a reference file over 2,000 needs a table of contents" — inside the
measurement step at lines 40-46 of that file, which requires each threshold
breach to be reported as a `NEEDS_ACTION` finding rather than advice. The root
`AGENTS.md` progressive-disclosure directive is the layer rule behind it: skill
`references/` carry mechanics, schemas, and long detail, and a reader must be
able to reach the one part they need without reading the whole file.

The practical cost is that every `/compile` worker, and every session routed
here from root `AGENTS.md` for the sparse-checkout mechanics, reads all 2313
tokens of mode selection, invocation properties, generated-output inventory,
wrapper bootstrap behaviour, and Local generation rules to answer one narrow
question.

## Design

Recommended approach: add structure in place rather than split the file.
Introduce `##` sections over the content that is already grouped by topic, and
a `## Contents` list linking them, following the shape used by
`.agents/references/subagent-reporting.md`. The author's recommended section
boundaries, from the current file, are:

- mode selection rules and the exemptions and exceptions to them (lines 3-10)
- repository-root detection and the resolved context's mode derivation
  (lines 12-14)
- the build invocation and the properties it derives (lines 16-21)
- the generated Data directory's expected inventory (line 23)
- wrapper bootstrap Shared-data refresh (lines 25-27)
- Local generation, including materialization and the Gaea and expensive-export
  guards (lines 29-31, and the prohibition on line 33)

Adding headings and a contents list raises the token count slightly, which the
rule accepts: the threshold governs whether a table of contents is required,
not a size the file must be reduced to.

Splitting the file into two references is the alternative. It is not the
recommendation because six inbound links across
`.agents/skills/compile/SKILL.md`, `.agents/skills/compile/references/worker.md`
(four sites), `.agents/skills/compile/references/prefast-mode.md`, and root
`AGENTS.md` would each have to be re-pointed at the correct half, and the
mode-selection rules and the invocation that derives the data properties from
them are read together. If the implementing session does split, every one of
those inbound links must be updated in the same change.

Binding outcome either way: no rule, trigger, exemption, exception, typed code,
switch name, or prohibition currently stated in the file may be dropped,
weakened, merged with another, or moved somewhere a `/compile` worker cannot
reach from the contents list.

## Critical files

- `.agents/skills/compile/references/runtime-data-mode.md` — the flat
  over-threshold reference; the whole file is the subject.
- `.agents/skills/compile/SKILL.md:46` — inbound link; also the file
  `/validate-skill` checks if bundled links change.
- `.agents/skills/compile/references/worker.md:53,95,143,164` — four inbound
  links.
- `.agents/skills/compile/references/prefast-mode.md:3` — inbound link.
- `AGENTS.md:12` (root) — inbound link routing sessions here for the
  sparse-checkout mechanics.

## In scope

- Adding `##` headings and a `## Contents` list to
  `.agents/skills/compile/references/runtime-data-mode.md`, and reordering or
  regrouping its existing bullets and paragraphs only as far as those headings
  require.
- Purely local wording adjustments in that file where a sentence's meaning
  depended on the flat ordering that headings change.
- Updating the five inbound link sites listed under `## Critical files` only if
  the implementing session chooses the split alternative, or if a link's
  surrounding sentence names a part of the file that moved.

## Out of scope

- Changing, adding, or removing any rule, trigger, exemption, exception, typed
  code, switch, property name, or prohibition the file states.
- `/compile` behaviour, `WorktreeCli`, `DataPacker`, the build invocation, and
  every script.
- Other files under `.agents/skills/compile/references/`, and every other
  over-threshold document elsewhere in the repository.
- Changing the 2,000-token threshold, `Measure-Tokens.ps1`, or
  `/progressive-disclosure-review`.

## Risk tier and invariants

Expected Change Workflow Tier 1: mechanical documentation work with no public
signature or invariant exposure, and no executable artifact changed. The
trigger is documentation reorganization only.

No determinism/CRC, serialization, `.pack`/`kiVersion`, replay, wire,
threading, allocation, shader, or project-membership exposure, and no compile
is required. Escalate only if the implementing session finds a stated rule that
cannot survive restructuring without a script or skill behaviour change.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 -Path .agents/skills/compile/references/runtime-data-mode.md`
  runs, and the file contains a `## Contents` list whose entries match its `##`
  headings one-for-one, in file order.
- A reviewer maps every rule, trigger, exemption, exception, typed code, switch
  name, and prohibition present in the file before the change to its location
  after the change, with none dropped, weakened, or merged.
- `/progressive-disclosure-review` reports no threshold or layering finding
  against the changed file.
- If the split alternative was taken, every inbound link listed under
  `## Critical files` resolves to the half that owns the fact its sentence
  cites, and `/validate-skill` passes for the `compile` skill package.

## Notes

Observed and proven by the `/progressive-disclosure-review` reviewer of an
unrelated session; this document depends on nothing.

Session provenance (machine-local; not reproducible after cleanup):

- Client: claude
- Conversation session ID: 1af20171-b83a-4f30-a4ed-335f78190ee7
- Worktree/branch UUID: e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Session branch: claude/e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Worktree: .claude\worktrees\BrokenEngine\e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Landing ref: claude/e05e48ea-58ba-4b02-908d-5b9c76c49e60
