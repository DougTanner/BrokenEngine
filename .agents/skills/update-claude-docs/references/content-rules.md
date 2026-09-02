# Content Rules

## Current State and Vocabulary

- State how the code works now. Do not narrate sessions, plans, commits, migrations, removals, former names, dates, or before/after history; git owns that record.
- Use vocabulary established by root and governing documents, including *Collection*, *Frame*, phase names, *workbuffer*, `gp*` singleton, SOA, EWNS, and deterministic CRC. Search the tree before inventing a near-synonym.
- A *hub* is a subsystem's main AGENTS.md that links to the detail docs below it. Existing wording also calls those detail docs *leaves*; *detail doc* is the term to use in new prose.
- Do not silently override a parent or sibling invariant. Pause on a meaningful contradiction and identify both sources; report an inconsistency that is not meaningful as a residual.
- Use direct, normal emphasis. Avoid all-caps directives and repeated rules.
- Apply the one-term-per-concept and plain-words rules in root `AGENTS.md` `## Directives`. Reintroducing a second name — including one the repository previously retired in favor of the established term — is a finding, not a style choice.
- Write concrete actions, not abstract noun-stacks: say who does what ("the loop that keeps processing until the queue is empty"), not a compressed label ("drain loop") — unless the label is an established defined term or a code identifier.

## Architecture, Not Inventories

- Describe responsibilities, ownership, relationships, data flow, algorithms, and reasons a design constraint exists.
- Do not list members, enum values, variables, files, uniforms, bindings, push constants, or method call chains. Do not name-drop a symbol introduced by the current change merely to document the diff.
- Link to the authoritative owner instead of duplicating parent, sibling, parallel-hierarchy, or `Documents/Architecture/` guidance. Use `@path` only for an intentional Claude import; use Markdown links for see-also references.
- Every ancestor AGENTS.md loads automatically alongside a child document, so a child never references, links, or "See Also"s its parent or any other ancestor AGENTS.md. Remove such a reference whenever you edit the text around it.

## Removing Text

- Remove a sentence unless its absence would plausibly cause a worse future decision.
- Never delete knowledge that still changes a decision; shorten it instead.
- When a rule moves out of a document, put it in a named place — a specific AGENTS.md or a specific source comment (`file:line` or function) — and say where. It may move; it may not just disappear. Shortening a rule that stays in the same document is not a move.
- Before saying a rule now lives somewhere else, open that file and confirm it is really there. Never assume a source comment exists.
- Keep knowledge that reading the current code cannot bring back — debugging findings, driver or build-tool behavior, questions already investigated and settled — while it still changes a decision. "The code cannot prove it" is not a reason to remove it.
- A size target never justifies deleting a sentence that still changes a decision. When a needed sentence pushes a file over its target, report the excess or propose splitting the file instead.

## Authoritative Source Exemplars

A source exemplar may replace procedural prose only when it points to one stable file and symbol, labels the concern and the applicability variant, and leaves the governing invariant and reason in AGENTS.md. A concern may cite at most three exemplars. The source demonstrates implementation shape; documentation remains authoritative for ownership and constraints.

Do not establish one-off code introduced by the current change as authoritative until an existing repository pattern supports it. During every affected documentation sync, verify that each cited path and symbol still demonstrates its label; retarget a stale symbol in the same edit. Keep distinct applicability variants distinct rather than presenting one exemplar as a universal policy.

## Stub Contract

Directory memory lives in `AGENTS.md`. Its sibling `CLAUDE.md` is only the one-line `@AGENTS.md` import. Put all guidance in AGENTS.md. Enforce the pairing bidirectionally, excluding `CLAUDE.local.md` unless explicitly authorized.

