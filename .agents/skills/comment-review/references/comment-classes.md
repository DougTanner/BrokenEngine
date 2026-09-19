# Comment Classes

The classes `/comment-review` reports, the severity each carries, and the facts
no finding may remove. `Documents/C++StyleGuide.txt` rule 64 is the authority
these classes enforce; they add no rule it does not state.

| class | rule 64 clause it enforces | what the class adds | severity |
|---|---|---|---|
| boilerplate | never restates the signature or a house pattern in template fields, never uses banner separators, never merely explains a language feature or house pattern the declaration already shows | "Uses X to …" counts as a template field | Required |
| history | never describes the previous implementation, the change that produced the code, or a comparison against replaced code | trigger words: "now", "no longer", "previously", "instead of" | Required |
| speculative | never describes a hypothetical or future code path, and never argues that the code is correct | a rebuttal of a review is such an argument | Required |
| navigation | never carries an `AGENTS.md` navigation pointer as its content | — | Required |
| false | every statement is true of the adjacent code as it stands | — | Required |
| dense | says it in the fewest sentences that carry it | a block over 8 lines, or a fact the block or its function already states elsewhere | Recommended |

## Examples

- boilerplate — `// Returns: true on success` above a `bool` function restates
  the declaration below it, and `// =====================` is a banner
  separator. A template field is boilerplate when its content is generic
  ("thread-safe", "uses local resources") or restates what the signature shows;
  one carrying a real threading or lifetime contract is preserved by rewriting
  it as a plain present-tense sentence, never deleted.
- history — `// Previously scaled by 100; now stored in meters.` describes the
  change that produced the current code.
- speculative — `// The bucket list is never empty here. If it were, the
  loop below would return without writing.` The first sentence states the
  invariant, so it is preserved; only the hypothetical-path sentence goes.
- navigation — `// See AGENTS.md for the threading rules.` Delete a comment
  whose sole content is the pointer; otherwise keep the technical statement and
  remove only the pointer.
- false — no fixed example applies: the test is to read the adjacent
  code and accept the class only when that code contradicts the stated fact,
  rather than to match a pattern. When the claim is about runtime correctness,
  the replacement slot is `route: /repo-code-review`, because either the comment
  or the code is wrong and only that review decides which.
- dense — a nine-line block spelling out a thread-safety precondition one
  sentence carries: the constraint is real, so it is shortened, never deleted.

## Preserve list

Never report a finding that removes a fact rule 64's "What a comment should say"
clause lists, and keep every such fact in a replacement text.

A long comment carrying only those facts is `dense` at most, never
`boilerplate`, and its replacement shortens the prose without dropping a fact.
