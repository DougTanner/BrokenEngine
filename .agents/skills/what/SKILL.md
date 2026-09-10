---
name: what
description: Re-explain the agent's most recent message, question, or request in plain language with full standalone context, then re-ask any pending question. Use when the user explicitly invokes this skill because the last message was unclear, jargon-heavy, or assumed knowledge of the code, a plan file, or earlier session scrollback.
argument-hint: [topic]
disable-model-invocation: true
---

# What

## Purpose

The user did not understand your last message. Rewrite it so it stands alone,
following the repository User Interaction rules.

## When to use

- The user explicitly invokes this skill because the last message was unclear or
  jargon-heavy.
- The user explicitly invokes this skill because the last message assumed
  knowledge of the code, a plan file, or earlier session scrollback.

## Steps

1. Identify the target: the topic named in the argument, or with no argument
   your most recent message — especially any question or request still waiting
   on the user. Done when that target is named.
2. Restate it under the rules below, using headings and bullets so a longer
   explanation stays skimmable. Done when the restatement is answerable from
   this one message alone.
3. If the target contained a question or decision, re-ask it after that
   context is visible. Done when the pending question is either re-asked or
   confirmed not to exist.

## Rules

- Apply [`.agents/references/change-workflow.md`](../../references/change-workflow.md)
  `### User Interaction` for plain language, standalone context, and decision
  presentation. Where a technical term is unavoidable, explain it in one short
  sentence or parenthetical the first time it appears.
- Apply root [AGENTS.md](../../../AGENTS.md) `## Directives` one-term-per-concept
  rule.
