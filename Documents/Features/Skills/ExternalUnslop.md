# External unslop

## Context

The source `unslop` skill removes common AI writing patterns such as filler, puffery, vague attribution, formulaic framing, passive sentences, dense prose, and formatting tells. It also asks the writer to add personality. A repository version must keep the useful pattern catalog without treating words as banned tokens or changing facts, citations, identifiers, quotations, or an established voice.

## Why this is worth integrating

Generated or heavily edited prose can hide the one fact a reviewer needs. A focused pass can cut filler and make a sentence concrete while preserving the author's meaning. The pass is safest when the user names the exact prose, file, or range. A broad automatic rewrite would make it too easy to alter a technical claim or a quote without noticing.

## Design

- Add one standalone package at `.agents/skills/external-unslop/`. The manual feature document will live at `Documents/Features/Skills/ExternalUnslop.md` when this design is adopted.
- Require an exact target: prose supplied in the request, one named file, or one explicit line or section range. Refuse a vague repository-wide request and do not infer adjacent files.
- Treat the source pattern catalog as clues, not a banned-token list. Remove a pattern only when the surrounding sentence shows that it is filler, puffery, vague attribution, formulaic framing, avoidable passive voice, needless density, or a formatting tell. Keep a legitimate domain term even when it appears in the catalog.
- Preserve facts, meaning, citations, identifiers, quotations, paths, code samples, and established voice. Do not invent a source, add an opinion, make a quote smoother, or replace a repository term with a synonym. Match the requested document mode and audience.
- Follow the original request verb. For supplied prose, return replacement prose. If an explicit manual request authorizes editing an exact file or range target, edit only that target within the active Change Workflow. That exact-target edit authority removes only a redundant second write-authorization prompt; tracked edits receive the workflow steps triggered by their artifacts/tier, and explicit landing confirmation applies only when a landing gate will change primary. If the request asks for review or a proposal, or does not authorize writing, return findings or replacement prose and leave the target unchanged. Never touch text outside the exact target.
- Keep the skill independent from `external-technical-writing`. It does not invoke another skill, run a workflow, or apply to every document by default.

### Trigger and client policy

The frontmatter description names explicit prose-cleanup requests. The exact human interfaces are `/external-unslop <exact prose, file, or changed range>` in Claude Code and `$external-unslop <exact prose, file, or changed range>` in Codex. The skill is explicit and manual in both clients. Set Claude `disable-model-invocation: true`, Codex `policy.allow_implicit_invocation: false`, and keep the user-invocable entry point. Manual invocation is required because a prose change can alter a factual or quoted record even when the change looks stylistic.

### Attribution and license

Adapt `pstack/skills/unslop` at commit `60c641e4fad674784b30abcf9f8915dea39df38d`. The standalone package keeps a `LICENSE` file with the full MIT notice and Lauren Tan's 2026 copyright notice. Preserve that notice for the substantially adapted skill and do not cite an absolute source path.

## Critical files

- `Documents/Features/Skills/ExternalUnslop.md`
- `.agents/skills/external-unslop/SKILL.md`
- `.agents/skills/external-unslop/agents/openai.yaml`
- `.agents/skills/external-unslop/LICENSE`
- `.agents/skills/validate-skill/SKILL.md` and its frontmatter schema, which govern the eventual package metadata and bundled links.

## In scope

- One standalone, manual, exact-target prose cleanup skill for Claude and Codex.
- The adapted pattern catalog, contextual judgment, preservation rules, request-verb write boundary, and review/proposal output.
- Independence from `external-technical-writing`, client policy files, source attribution, the package license, validator and inbound-link checks, and realistic forward scenarios.

## Out of scope

- Any actual skill implementation in this drafting task.
- Automatic invocation, automatic Change Workflow, automatic file writes, repository-wide scans, service access, or external source lookup.
- Treating pattern words as banned tokens or changing facts, meaning, citations, identifiers, quotations, paths, code, or established voice.
- Adding an "add soul" rewrite that invents personality, opinions, or facts.
- Changes to root workflow rules, commits, landing, or unrelated tracked files.
- Unit tests.

## Risk tier and invariants

Tier 2 for the standalone skill. A tracked prose edit inherits the active change tier instead of being downgraded because the requested operation is stylistic. The feature does not touch engine state, determinism, CRC, wire data, `.pack` data, threading, or runtime behavior. The following invariants are mandatory:

- The user supplies an exact prose, file, or range target.
- Facts, meaning, citations, identifiers, quotations, paths, code samples, and established voice remain unchanged unless the user explicitly asks for a substantive rewrite.
- Pattern names guide judgment. They never ban a token by themselves.
- Ordinary use does not invoke another skill. Tracked edits receive the workflow steps triggered by their artifacts/tier, and explicit landing confirmation applies only when a landing gate will change primary. An explicit manual edit request for an exact target removes only the redundant second write-authorization prompt; it does not waive those steps.
- A review/proposal request or a request without writing authorization returns findings or replacement prose and does not write the target.
- Claude and Codex expose the same manual-only behavior.

## Acceptance criteria

- The future package passes `validate-skill` for Claude and Codex metadata, and its bundled links and inbound references resolve.
- Independent realistic Claude and Codex scenarios remove filler from an exact prose or range target while preserving a citation, identifier, quotation, code sample, and established voice.
- A scenario uses a catalog word as a legitimate domain term and proves that the term remains when context gives it real meaning.
- A review/proposal scenario returns findings or replacement prose and leaves the target unchanged; an explicit edit request changes only the requested target within the active Change Workflow without a redundant second write-authorization prompt, while tracked edits receive the workflow steps triggered by their artifacts/tier, and explicit landing confirmation applies only when a landing gate will change primary.
- Static inspection confirms no automatic workflow, skill chaining, service access, broad scan, or "add soul" invention.
- The package preserves the required MIT notice. No unit tests are added.

## Notes/Coordination

This is a manual Feature document under `Documents/Features/Skills/`, without executable-Plan metadata. Ordinary use is outside Change Workflow. Implementing the tracked skill or authorizing a tracked prose edit is separate work. Tracked edits receive the workflow steps triggered by their artifacts/tier, and explicit landing confirmation applies only when a landing gate will change primary. The exact-target edit authority removes only a redundant second write-authorization prompt. The source remains `pstack/skills/unslop` at the cited commit.
