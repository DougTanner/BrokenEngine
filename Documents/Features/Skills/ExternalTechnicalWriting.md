# External technical writing

## Context

The source `technical-writing` skill combines four writing standards: Diátaxis document modes, Google developer style, Simplified Technical English sentence rules, and Global English clarity rules. It also requires an `unslop` pass. This repository needs the four-layer writing guidance, but the two skills must remain independently invoked so a prose review cannot silently rewrite text or start another workflow.

## Why this is worth integrating

Feature plans, architecture notes, skills, and review records are read by tired engineers and by agents that do not share the planning conversation. A compact shared writing standard reduces ambiguous instructions, preserves real repository names, and makes the intended document mode clear. The standard helps only when it respects the artifact's schema, owner, facts, and voice.

## Design

- Add one standalone package at `.agents/skills/external-technical-writing/`. The manual feature document will live at `Documents/Features/Skills/ExternalTechnicalWriting.md` when this design is adopted.
- Make the four layers explicit and ordered: choose one Diátaxis mode; write direct sentences in the Google developer style; keep one thought per sentence under the STE rules; remove ambiguity under the Global English rules. Keep the source's useful examples and review checklist without copying unrelated product language.
- Establish precedence before applying a heuristic. User intent, the artifact schema, the artifact owner, required identifiers, facts, citations, quotations, and established voice all outrank a writing preference. When a heuristic would change one of those, preserve the artifact and report the conflict instead.
- Default to read-only drafting and review. A draft or review returns text or suggestions in the reply. A file edit requires an explicit user request and an exact file or range. The skill does not scan a repository or rewrite adjacent documents.
- Remove the source dependency on `unslop`. This skill neither invokes `external-unslop` automatically nor imports its pattern list. A user may invoke the two skills separately when both are wanted.
- Keep source paths, symbols, flags, commands, and counts exact. Do not change a code identifier or citation to make a sentence sound smoother. Do not invent facts, sources, links, or a voice.
- Keep one document mode per document. Link related modes when a document set needs both action and explanation. A review may identify a mixed mode, but it does not silently restructure the document.

### Trigger and client policy

The frontmatter description names explicit drafting, review, and rewrite requests for technical prose. The exact human interfaces are `/external-technical-writing <path-or-prose>` in Claude Code and `$external-technical-writing <path-or-prose>` in Codex. The skill is explicit and manual in both clients. Set Claude `disable-model-invocation: true`, Codex `policy.allow_implicit_invocation: false`, and keep the user-invocable entry point. Manual invocation keeps prose judgment attached to the user's target and prevents an automatic writing pass from changing a tracked artifact.

### Attribution and license

Adapt `pstack/skills/technical-writing` at commit `60c641e4fad674784b30abcf9f8915dea39df38d`. The standalone package keeps a `LICENSE` file with the full MIT notice and Lauren Tan's 2026 copyright notice. Preserve that notice for the substantially adapted skill and do not cite an absolute source path.

## Critical files

- `Documents/Features/Skills/ExternalTechnicalWriting.md`
- `.agents/skills/external-technical-writing/SKILL.md`
- `.agents/skills/external-technical-writing/agents/openai.yaml`
- `.agents/skills/external-technical-writing/LICENSE`
- `.agents/skills/validate-skill/SKILL.md` and its frontmatter schema, which govern the eventual package metadata and bundled links.

## In scope

- One standalone, manual, read-only-by-default writing skill for Claude and Codex.
- The four-layer writing standard, mode selection, precedence rules, exact-symbol handling, and the explicit-write boundary.
- Removal of the automatic `unslop` dependency.
- Client policy files, source attribution, the package license, validator and inbound-link checks, and independent realistic forward scenarios.

## Out of scope

- Any actual skill implementation in this drafting task.
- Automatic Change Workflow steps, automatic skill chaining, repository-wide prose scans, service access, or external source lookup.
- Changing artifact schemas, owner requirements, user intent, facts, citations, identifiers, quotations, or established voice to satisfy a style heuristic.
- Automatic file writes, tracked edits, commits, landing, or changes to root workflow rules.
- A replacement for `external-unslop`, which remains a separate explicit skill.
- Unit tests.

## Risk tier and invariants

The normal workflow uses the highest tier triggered by the actual changed artifacts. Once the tier is determined from the actual artifacts, only workflow steps triggered by that artifact type and tier apply: Tier-1 documentation changes skip plan audit and scope review, `validate-skill` applies only to changed skill packages, and landing confirmation applies only at a landing gate. A docs-only change may be Tier 1; changing skill behavior or another non-documentation artifact is Tier 2 or higher. The feature changes only an explicit documentation and drafting workflow. It does not touch engine state, determinism, CRC, wire data, `.pack` data, threading, or runtime behavior. The following invariants are mandatory:

- Ordinary drafting and review are read-only and do not start Change Workflow.
- A file write requires explicit user authority and an exact target. Tracked edits enter the normal workflow for that change.
- Artifact schemas, owners, user intent, facts, citations, identifiers, quotations, and established voice outrank style heuristics.
- The four layers remain present, and the `unslop` dependency is absent.
- Claude and Codex expose the same manual-only behavior.

## Acceptance criteria

- The future package passes `validate-skill` for Claude and Codex metadata, and its bundled links and inbound references resolve.
- Independent realistic Claude and Codex scenarios cover tutorial, how-to, reference, and explanation material. Each scenario identifies the mode, gives read-only suggestions, and leaves files unchanged.
- A scenario with a schema-required name, owner rule, citation, code identifier, quotation, or established voice proves that the precedence rules preserve it even when a heuristic disagrees.
- An explicit exact-range write changes only the named target. A default drafting or review request produces no file change and does not invoke `external-unslop`.
- Static inspection confirms no automatic Change Workflow trigger, root workflow edit, service access, or unsupported source citation.
- The package preserves the required MIT notice. No unit tests are added.

## Notes/Coordination

This is a manual Feature document under `Documents/Features/Skills/`, without executable-Plan metadata. Read-only use is outside Change Workflow. Implementing the tracked skill or authorizing a tracked edit is separate work: tracked prose changes use the highest tier triggered by their actual changed artifacts, with docs-only changes eligible for Tier 1 and skill-behavior changes Tier 2 or higher. They follow only the repository workflow steps triggered by those artifacts and tier; landing confirmation applies only at a landing gate. The source remains `pstack/skills/technical-writing` at the cited commit.
