# Skill Skeleton

The body shape every `.agents/skills/` skill follows, and the checklist a
reviewer applies to it. Frontmatter and package mechanics belong to
`/validate-skill`; layering and size thresholds belong to
`/progressive-disclosure-review`.

## Section order

1. `Purpose` (`SKILL.md`) — what the skill produces, in at most 3 lines.
2. `When to use` (`SKILL.md`) — triggers only.
3. `Inputs` (`SKILL.md`) — which task-brief fields from
   [`subagent-reporting.md`](subagent-reporting.md) the skill consumes.
4. `Steps` (`references/worker.md`) — numbered, one imperative each, each ending
   on a checkable done-condition, no paragraph over 4 lines.
5. `Handoff` (`SKILL.md`) — the skill's extension fields, fixed shared values,
   and narrowed row forms as plain lines, or as a row-form fence holding no
   shared field name; it never re-renders the shared form from
   [`subagent-reporting.md`](subagent-reporting.md), `## Handoffs`. Applying
   that reference's 'What main does with each field' table, mandate inline only
   what the table gives main an action for, and no more text than that action
   needs, however short the excess is; that reference's `## Handoffs` size caps
   still govern the return. An extension field's own line counts when main acts
   on it as it acts on the field it extends, text main must present or ask
   verbatim stays inline per `## Public and private files`, and everything else
   is cited as path plus selector.
6. `Rules` (`references/worker.md`) — judgment no step owns, as bullets.
7. `References` (`SKILL.md`) — each linked file owning one topic.

Omit a section the skill has no content for; never reorder.

## Public and private files

`SKILL.md` is the public file, read by main and by the worker;
`references/worker.md` is the private file, read by the dispatched worker, or by
main when it runs the skill itself.
`## Section order` above marks each section's file. Text main must present or
ask verbatim is a subsection of `Handoff`.

`SKILL.md` links `references/worker.md` from `References` as its worker-entry
line, which opens with the private marker so a session that only dispatches
never loads the private steps into its context:

```
- [`references/worker.md`](references/worker.md) — private: read it only if you are the session executing this skill. <what it holds>.
```

`SKILL.md` on its own must suffice for main to dispatch the worker and to
present or ask anything verbatim; a `SKILL.md` missing text main must deliver
verbatim is the failure this rule prevents. Text main reads to decide whether or
how to dispatch — triggers, inputs, verbatim presentations and questions — is
public even when it would otherwise fall under `Steps` or `Rules`.

Each other reference is linked from `references/worker.md` when only the worker
reads it, or from `SKILL.md` when main reads it.

## Shared vocabulary

Status and severity words are the ones the shared handoff form in
`subagent-reporting.md` lists. `Critical` and `Required` are defined in
`.agents/skills/repo-code-review/SKILL.md`, and `Recommended` in
`.agents/skills/validate-skill/SKILL.md`. Introduce no other status or severity
term.

## Reviewer checklist

- Every action the previous body required is present exactly once across the two
  files.
- Each section lives in the file `## Section order` names; `SKILL.md` carries
  only what main reads.
- `SKILL.md` links `references/worker.md` with the private marker line from
  `## Public and private files`.
- `## Handoff` follows `## Section order` item 5: it renders no return fence
  holding a shared handoff field, and it mandates inline only what item 5
  allows.
- Size and layering pass `/progressive-disclosure-review`.
