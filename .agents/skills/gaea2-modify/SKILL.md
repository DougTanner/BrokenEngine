---
name: gaea2-modify
description: Edit a Gaea 2 terrain previously loaded by /gaea2-load — add/remove/move/rewire nodes or change node properties. Operates on the Markdown file in Temp/ produced by /gaea2-load; the user writes the result back to a .terrain with /gaea2-save.
argument-hint: <name-or-Temp/path.md>
allowed-tools: [Read, Edit, Write, Glob, Grep, PowerShell]
disable-model-invocation: true
---

# gaea2-modify

## Purpose

Edit the Markdown view of a loaded Gaea terrain. Only the invoking session edits
`Temp/<name>.md`; the only script this skill runs is the read-only validator in
step 3 below. The companion `.passthrough.json` is read-only here; if a change
requires altering ports or modifiers, document the limitation and ask the user.

## When to use

Editing a Gaea 2 terrain previously loaded by `/gaea2-load` — adding, removing,
moving, or rewiring nodes, or changing node properties.

- Doesn't change ports/modifiers — those are in passthrough JSON.
- Doesn't change `$type` / type FQNs — pinned by passthrough.
- Doesn't validate that a property name is real for the given node type — the
  parser is permissive; if you invent `Foo: 1` on an Erosion2 it'll round-trip
  through but Gaea will likely ignore it.

## Inputs

`$ARGUMENTS` holds either a bare name (`Stylized Mountain`), a `.md` filename,
or a full path.

## Steps

1. Resolve the target in this order. Done when one existing `.md` path is in
   hand.
   1. If `$ARGUMENTS` is a path that exists, use it.
   2. Otherwise look for `Temp/<arg>.md` or `Temp/<arg>`.
   3. If still ambiguous, list `Temp/*.md` and ask the user which.

2. Apply the change via `### Edit operations` below. Before adding a node, read
   `../gaea2-shared/references/node-conventions.md`. For node type changes,
   route to the user and stop — `.passthrough.json` pins `$type`
   (`validate_markdown.py` flags mismatches). Done when applied (new nodes per
   that reference) or routed.

3. Run the validator on the edited file before reporting done, dispatching
   `validate_markdown.py` and handling `python.missing` / `python.stale` per
   [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#markdown-validator-gating).
   It is required, not optional. Done when the wrapper has printed one
   `broken-engine-gaea2-python/v1` envelope.

4. Gate on the validator's inner `broken-engine-gaea2-markdown/v1` envelope per
   [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#markdown-validator-gating)
   (including inner `code` parsing, `internal.error`, and
   `payload.entriesTruncated`). Done when the inner `code` is `ok`.

   - `markdown.entries-reported` — fix every `{nodeId, rule, message}` finding
     in `payload.entries` before reporting done (`CHECKS` and `MINIMUM_NODE_ID`
     in `../gaea2-shared/scripts/validate_markdown.py` own the rules).
   - Blocked input/parse codes — resolve the unreadable/missing file or parse
     failure before reporting done.

5. Then do the judgment the validator can't. Done when each of these has been
   checked.

   - Check that property values still parse as their original type — don't put a
     string where a float was, and see the type rules under "Edit a property".
   - For any new node, warn the user that saving emits an empty `Ports` list and
     drops incoming edges without a `.passthrough.json` port catalogue entry
     (`../gaea2-save/SKILL.md` `### Failure modes`), so they can copy one from a
     sibling file.
   - For any newly-added node, check the enum values against
     `## Per-type enum constraints` in
     `../gaea2-shared/references/node-conventions.md`; an invalid one silently
     deactivates the node and the validator won't see it.

### Edit operations

#### Edit a property
Find `### n<ID>: <Type>`, edit the `key: value` line beneath it.
- Floats like `Duration: 15.870041` — change the number, keep float syntax (a
  trailing `.0` if integer-valued).
- Ints like `Seed: 40828` — bare integer, no decimal.
- Strings — quote with `"..."` for `:`, `#`, leading/trailing whitespace,
  leading `[`/`{`, or boolean/`null`/numeric values (`parse_scalar` in
  `../gaea2-shared/scripts/save_terrain.py`).
- Booleans — `true` / `false` (lowercase).

#### Move a node
Edit the `position:` line: `position: 27225.0, 26250.0`. Two floats,
comma-separated. The X axis grows right; Y grows down (canvas space). Adjacent
grid spacing in the examples is typically ~150–300 units.

#### Rename a node
Edit the `name: ...` line under the node's heading. If no `name:` line exists
(the loader omits it when name == type), add one.

#### Add a node
1. Pick a free integer ID in the range 100-998: scan all `### n<ID>:` headings
   and pick `max(IDs) + 1`. Never use an ID below 100 — Gaea 2 silently
   deactivates it (details under `## Id must be ≥ 100` in
   `../gaea2-shared/references/node-conventions.md`).
2. Append a new node section at the bottom of `## Nodes`:
   ```
   ### n<NEW_ID>: <Type>
   position: <X>, <Y>
   <key>: <value>
   ...
   ```
3. Add `n<NEW_ID>["<Type>"]` to the Mermaid block (between `flowchart TD` and
   the edges).
4. The save step needs port info for new nodes, which lives only in
   `.passthrough.json`. Present the `### New node port-catalogue message` below
   to the user. Offer to find a node of the same type in the shipping examples
   (`C:/Program Files/QuadSpinner/Gaea 2/Examples/`) and quote its port block.
5. Check the per-type requirements in
   `../gaea2-shared/references/node-conventions.md`
   `` ## Types that require `Version: 2` `` and `## Per-type enum constraints`.
   Several node types need specific fields (e.g. `Version: 2`) or valid enum
   values, otherwise Gaea silently loads them as deactivated. The validator in
   step 3 catches the field-level ones; whether an enum value is legal it cannot
   judge — you must.

#### Remove a node
1. Delete the entire `### n<ID>: ...` section (heading + its property lines).
2. Delete the `n<ID>["<Type>"]` line from the Mermaid block.
3. Delete every Mermaid edge that mentions `n<ID>` (either side).

#### Rewire (change edges)
Edit edge lines inside the Mermaid block.
- Default port pair (`Out` → `In`): `nA --> nB`.
- Non-default ports: `nA -- "FromPort → ToPort" --> nB`. Use the U+2192 arrow
  `→` (not `->`); the loader emits this exact char.
- To re-target an edge, change either side. To add an edge, append a new line.
  To remove, delete the line.
- A node's input port can be wired to at most one producer (Gaea graph rule). If
  you add an edge into an input port that's already wired, remove the old edge
  first; otherwise the saver keeps only one Record per port and Gaea may reject
  the file.

#### Edit Notes
The `## Notes` section uses `### note<ID>` blocks with key/value lines. The
`markdown:` value supports a `|` block for multi-line text — keep the 2-space
indentation on continuation lines.

#### Edit Variables
Plain `key: value` lines under `## Variables`. Replace `(none)` with one or more
lines if adding variables.

## Handoff

### New node port-catalogue message

"I added <Type> as node n<NEW_ID> without a `.passthrough.json` port catalogue
entry — saving emits an empty `Ports` list and drops incoming edges unless you
add one with matching target port names (e.g., from a same-type sibling file)."
(See `../gaea2-save/SKILL.md` `### Failure modes`.)

## Rules

- The Markdown is the source of truth for topology, properties, positions,
  names, notes, and variables. The Mermaid block and per-node sections must stay
  in sync.
- Wrapper path resolution, stderr pass-through, and no-inline-reconstruction
  rules live in
  [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#no-inline-reconstruction).
- One validator limit is worth knowing: the reaches-output rule is skipped
  entirely when the graph has no output anchor at all — no `Export` node and no
  node carrying `SaveDefinition`. That is common in the shipping examples
  (`Mesa` is one), so a dead-end chain in such a file goes unreported; examples
  that do carry a `SaveDefinition` anchor are checked normally.
