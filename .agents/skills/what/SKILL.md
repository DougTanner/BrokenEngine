---
name: what
description: Re-explain the agent's most recent message, question, or request in plain language with full standalone context, then re-ask any pending question. Use when the user invokes /what because the last message was unclear, jargon-heavy, or assumed knowledge of the code, a plan file, or earlier session scrollback.
argument-hint: [topic]
disable-model-invocation: true
---

# What

## Purpose

The user did not understand your last message. Rewrite it so it stands alone,
following the repository User Interaction rules.

## When to use

- The user invokes `/what` because the last message was unclear or
  jargon-heavy.
- The user invokes `/what` because the last message assumed knowledge of the
  code, a plan file, or earlier session scrollback.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The restatement steps and the rules
  they follow.
