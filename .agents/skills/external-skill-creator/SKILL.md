---
name: external-skill-creator
description: Create, revise, or validate repository skills. Use explicitly to author skill workflows, triggers, handoffs, progressive disclosure, or client compatibility; use automatically after any `.agents/skills/*/` package changes to perform independent findings-only validation of frontmatter, layout, links, invocation policy, and semantics.
allowed-tools: [Read, Write, Edit, Glob, Grep, Bash, PowerShell]
---

# Skill Creator and Validator

## Purpose

Author repository skill packages from user intent, or independently validate a
package against the repository's mechanical and semantic contracts.

## When to use

- Author mode: when the user explicitly asks to create or revise a skill,
  design its trigger or handoff, organize progressive disclosure, or assess
  client compatibility as part of that authoring work.
- Validate mode: automatically after creating, revising, auditing, or
  final-tree verifying any `.agents/skills/*/` package, and whenever
  frontmatter, `agents/openai.yaml`, invocation policy, layout, trigger quality,
  or bundled links need independent findings-only review.
- Validate mode may be the fresh validation pass carried by the Tier-1
  `/coherence-review` combined review. The root Change Workflow still requires
  an independent reviewer; the author never validates their own change as that
  review.

## Inputs

Supply `mode: author` or `mode: validate` in the task brief.

In author mode, supply the user intent, intended clients, allowed package scope,
and any fixed workflow or compatibility decisions.

In validate mode, supply one repository skill directory or its `SKILL.md` as
`Path`. A deliberately disposable package outside `.agents/skills/` also
requires `Fixture: true`.

## Handoff

Return the shared handoff from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Handoffs`.

In author mode, extend it with:

- `Client evidence` — one row per intended client: client and version;
  structural, loader, observed runtime, and documentation evidence; and
  unverified behavior.
- `Size measurements` — one row per changed Markdown file: path, measured
  `bt-token-v1` count, and applicable threshold result.
- `Unresolved decisions` — one row per compatibility or workflow choice the
  manager must settle, or `none`.

Claim support only for the clients and behavior the evidence checks. A loader
result establishes discovery and parsing, not invocation or tool behavior.

In validate mode, each mechanical-run row under shared `Decisive checks` names
the command, its exit, and decisive output. Additional concise rows name the
semantic surfaces reviewed and their verdict and evidence class, and the
inbound-reference sweep scope and classification verdict. Use shared `Evidence`
for the source selectors or retained output needed to substantiate those rows.
Each `Findings` row is one line on this form:

```text
VS### Critical|Recommended path:line — finding — correction
```

Use `PASS` only when every mechanical run succeeds and no Critical finding
remains. Use `NEEDS_ACTION` for target content or semantic Critical findings.
Use `BLOCKED` for setup, invocation, read, or internal-validator failures. A
Critical finding blocks a passing result; a Recommended finding is advisory.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you are the session executing this skill. Mode routing and executor boundaries.
