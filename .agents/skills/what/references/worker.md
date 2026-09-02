# What Worker

The restatement steps and the rules they follow. Triggers live in
[`../SKILL.md`](../SKILL.md).

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

- Use plain language; the user is not a domain expert. Where a technical term
  is unavoidable, explain it in one short sentence or parenthetical the first
  time it appears.
- Stick to the one established repository term for each concept and explain it
  once, instead of paraphrasing with synonyms — paraphrase variety is itself a
  source of confusion.
- Supply full context. The user has not read the source code, any plan file,
  or earlier session scrollback, and text emitted before a question-tool call
  may never be displayed — put everything needed to understand and answer into
  rendered message text the user is guaranteed to see, and only then ask.
- For any question or decision, state what the answer changes or blocks, and
  give options, trade-offs, and a recommendation where relevant.
