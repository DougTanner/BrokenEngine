# Jev for the hand-read style rules in `/code-style-review`

Open question: can Jev take over the hand-read pass of `/code-style-review`,
the rules `.agents/scripts/Find-SessionCandidates.ps1` emits no candidates
for? Part of the series in `JevDecisionModelWorkflowUses.md`. Piloted once
against the live API over the 41 hand-labelled code blocks in
`.agents/skills/code-style-review/references/style-rule-judgment/cases.json`,
which `.agents/scripts/Test-StyleRuleJudgment.ps1 -CasesPath` re-measures
with the questions `## The instructions` records; the Plan that wired the
result into the worker landed, and `/code-style-review` gates its hand read
on the script's session mode. The result is that one
request per changed function, carrying one `noul` per rule, orders the
hand-read pass for seven of the ten rules at one threshold, rule 3 needs a
higher one, rule 61 goes to a scanner instead, and rule 56 needs a
scanner-extracted name list as its state.

## The decision today

`.agents/skills/code-style-review/references/worker.md:59-71` (step 7)
hand-reads rules 3 and 56 across every changed range because the scanner
emits no candidates for them, and routes the gated rules — 14, 16 (including
its vector `.at()` clause), 21, 49, 51, 62, and the "always write `std::`"
half of 41 — through the judgment script in step 6 (`worker.md:36-58`). Every
one of those is a yes/no over a function-sized span, and before the gate the
worker read whole ranges to answer them. The scanner's own kinds (2, 15, 19,
27, 28, 29, 32, 41's `using
namespace`, 50, 52, 57, 58) are deterministic and stay out of this document.

One more judgment of the same shape lives outside the style guide: the log
level a new `LOG` call should carry, fixed as five options with one-line
definitions in `.agents/references/cpp-conventions.md:7`. It was not piloted;
a scanner for new `LOG` calls is trivial, so it can join the Plan as a
`choice` over the five levels once measured the same way.

## The question to Jev

One request per block of code, state `{ code: "<the block>" }`, and one
`noul` per rule in the same request, so Jev answers every rule in one pass
over the same state. A block is a whole function or class body; the pilot's
blocks ran from one line to 59 lines. The verdict for a rule is the `noul`
probability against a per-rule threshold.

Three phrasings were tried, the first two on the first 33 of the blocks below,
and only one works:

- Rule text pasted verbatim under "does this code violate the following
  rule" (`true` = a forbidden construct is present, `false` = follows or does
  not apply). Recall was perfect but rule 56 flagged 25 of 29 clean blocks
  and rule 61 scored 0.93 on a block whose `if` had braces: the model answers
  "does this rule apply to this code", not "is it broken".
- The same text asked positively ("does every part comply") with the answer
  inverted: no better.
- A construct-shaped instruction that names the forbidden pattern and lists
  the codebase's exceptions in the instruction itself, with `true` = that
  construct is present and `false` = every instance is one of the listed
  correct forms. This is the phrasing measured below. The exceptions must be
  the repository's, not the guide's: bare `size_t` and the fixed-width
  integer types for rule 41, `Num` inside `Numerator` and `Number` for rule
  14, square brackets on a raw pointer array, C array, or `std::span` and
  inside comments or strings for rule 16, `||` in a Boolean assignment or in
  a non-exit body for rule 62, and for rule 49 the multi-statement invariant
  and codec-adapter carve-outs the guide already states. The ten instruction
  texts are in `## The instructions` below.

The 41 blocks: 12 clean hand-written blocks built around the traps above, 21
hand-written blocks each planting one to three violations, six real functions
from `Engine/Source/Frame/TimeStep.cpp`, `AreaDamage.cpp`, and
`Collections/Explosions/ExplosionsSpawn.cpp`, and two of those real functions
with one name changed to plant a rule 14 or rule 56 violation deep inside a
39-line or 54-line body. Two sweeps of the same 410 rule-block judgments agreed
on all but three verdicts at 0.5, each a probability within 0.06 of the
threshold crossing it; 21 of the 410 probabilities moved by 0.05 or more, the
largest a rule 61 score falling from 0.91 to 0.54 on a clean block. So a
verdict near a threshold is read from the probability, never treated as
exact, as the series overview already requires. The 41 blocks cost 89,000
input tokens per block-form sweep; the tracked script, which also sends the
rule 56 name-list request per block, costs 146,000 per corpus run.

## Results by rule

Lowest probability on a planted violation against highest on a clean block,
over both sweeps (82 judgments per rule):

| rule | violations | lowest | clean | highest | verdict |
|---|---|---|---|---|---|
| 14 `Num` | 4 | 0.81 | 78 | 0.10 | Jev, threshold 0.5 |
| 16 `operator[]` | 6 | 0.84 | 76 | 0.37 | Jev, threshold 0.5 |
| 21 pointer + size | 2 | 0.98 | 80 | 0.08 | Jev, threshold 0.5 |
| 41 bare std names | 2 | 0.98 | 80 | 0.43 | Jev, threshold 0.5 |
| 49 accessor / pass-through | 14 | 0.63 | 68 | 0.51 | Jev, threshold 0.5 |
| 51 wrapped arguments | 4 | 0.66 | 78 | 0.48 | Jev, threshold 0.5 |
| 62 packed guards | 6 | 0.84 | 76 | 0.06 | Jev, threshold 0.5 |
| 3 Hungarian prefixes | 4 | 0.97 | 78 | 0.82 | Jev, threshold 0.9; see below |
| 61 braces on `if` | 4 | 0.98 | 78 | 0.98 | scanner, not Jev |
| 56 abbreviations | 10 | 0.25 | 72 | 0.72 | Jev over a name list; see below |

Rule 16 did not fire on the pointer-array `[iSpawnIndex]` writes in
`ExplosionsSpawn.cpp` (0.31 and 0.37) or on `map[key]` inside a comment and a
string (0.06). Rule 62 stayed at 0.04 on `||` in a Boolean assignment and at
0.02 on an `&&` guard. Rule 49 caught the getter, the `Is*` with a
composed expression, the one-call pass-through, the setter spread over two
lines, and the real `TimeStep::ClearAccumulator` and `AbsorbUnusedTicks`
(0.89 to 0.99); its one clean block over 0.5 was a two-statement bind function
at 0.51 and 0.47 across the sweeps. Rule 51 separated the wrapped call and the
wrapped 122-column condition from the allowed split past 140 columns (0.44 to
0.48) and from a single long line (0.05 and 0.06).

Rule 3 scores every planted violation at 0.97 or above but scores three of
the six real functions at 0.55 to 0.82, on names the guide's table does not cover
(`std::chrono::nanoseconds realDeltaNs`, `FXMVECTOR vecPosition`, struct
members without `m`). A threshold of 0.9 separates the pilot's four
violations from every clean block, but four positives is too few to fix a
threshold; the Plan below measures it on real changes before relying on it.

Rule 61 is the one construct-shaped phrasing could not fix: the Allman brace on
its own line after `if (...)` reads as "no brace" to the model, and stating
the Allman form as correct in the instruction moved nothing. The rule is a
two-line regex — an `if` or `else` line whose body starts on the same line
or whose next non-blank line is not `{` — so it joins
`Find-SessionCandidates.ps1` as a scanner kind
and never goes to Jev.

Rule 56 over a whole block missed `fDist` planted in the 39-line
`AreaDamage::Get` (0.25 in both sweeps) while catching the same kind of name
in an eight-line block (0.61 and 0.57): the vendor's state-dilution warning applied to
one short name among twenty-one. Two alternatives were measured:

- State `{ identifiers: [<every name the block declares or uses>] }` with one
  `noul` per name in the same request, the name in the instruction, after a
  scanner strips comments, strings, and keywords. Every planted name scored
  0.81 or above except `CalcVelocity` at 0.59, and `fDist` at 0.89 in the
  same 39-line function that hid it. At a threshold of 0.7 the clean blocks
  produced two flags in 36, `ack` (0.85) and `pVecPositions` (0.74), both
  member names after a `.` or `->`; the tracked script's scanner also strips
  those, so its corpus run asks neither and reports zero rule 56 false flags
  at 0.7, with `CalcVelocity` (0.54) still the one miss.
- One request per name with state `{ identifier, line }`: worse. Ordinary
  names drift up without their siblings (`rFrame` 0.55 to 0.73, `miUnitCount`
  0.65) and `CalcVelocity` fell to 0.38. The sibling list carries the naming
  convention, so the list form is the one to use.

The list form also surfaced real names the letter of the rule forbids and the
codebase keeps: `ack` (0.85), `vecDiff` (0.61), and `kuiNumThreads` (0.55
under rule 56 as well as 0.98 under rule 14). Whether `Diff` and `ack` are
exceptions is a style-guide decision, not a model one; the Plan lists them
for the user.

## What still needs a full model

Every fix, and the meaning-preservation decision the auto-fix requires
(`references/worker.md:107-110`). Jev replaces the reading of unchanged ranges,
not the judgment on a flagged block: the worker still reads a flagged
function against the guide before it fixes or routes anything, so the shape
is "Jev shortens the list the worker reads" as `JevDecisionModelWorkflowUses.md`
requires. A whole-file question is still no substitute: the handoff row needs
file, line, rule number, and correction (`SKILL.md:44`), and the function
enumeration supplies the first two.

## What would make this a Plan

The pilot met the success bar set before it ran (at least 90% of violations
flagged with under 30% of compliant blocks flagged) for rules 14, 16, 21, 41,
49, 51, and 62 at threshold 0.5. Rule 56 in the list form at 0.7 fell one name
short of it, 8 of 9 planted names, with `CalcVelocity` at 0.59 the miss. The
remaining measurement is rule 3's threshold and the rule 56 exception list,
both of which need real session changes rather than hand-written blocks. The
seven rules gate the read since the Plan landed; the second measurement — the
next test — is a rerun of the script's session mode over the last ten landed
C++ commits with the flagged blocks hand-labelled for rules 3 and 56, before
either of those two rules' flags is allowed to shorten the read.

## Decisions a Plan needs

1. Decided — the unit of state is one changed function or class body, found by
   an Allman-shape enumerator (a column-0 `{` not opened by `namespace`,
   through the matching column-0 `}`) over the changed ranges
   `Get-SessionChangeInventory.ps1 -Regions` already reports; a changed range
   outside any such body (a namespace-scope declaration) is its own block.
   The enumerator is a session mode of `Test-StyleRuleJudgment.ps1`, and its
   output rows carry `path`, `line`, `endLine`, and `flagged` (each entry a
   rule, its probability, and for rule 56 the flagged names).
2. Decided — one request per block carrying the seven block-level `noul`s
   (14, 16, 21, 41, 49, 51, 62) plus rule 3, and one request per block
   carrying the rule 56 name list with one `noul` per name; both written by
   a check script that calls `Invoke-Jev.ps1` in-process like
   `Test-CitationSupport.ps1` and reports per block the rules at or above
   threshold, ordered by probability.
3. Decided — rule 61 becomes a `style-rule-61` scanner kind in
   `Find-SessionCandidates.ps1`: an `if` or `else` line (the rule covers only
   those bodies) that carries a statement on the same line or whose next
   non-blank line does not start with `{`, `&&`, or `||`, excluding
   `else if`. Rule 2 forms stay hand-read.
4. Decided — the instructions are the construct-shaped texts of the pilot,
   carried in the check script as data, with the exception lists above; the
   thresholds are 0.5 for the seven rules, 0.7 for rule 56 names, and 0.9 for
   rule 3, written in the skill's references.
5. Superseded by user direction at landing — the worker reads only flagged
   blocks for the seven rules that met the bar, halts on any unusable result,
   and falls back to the hand read only on the user's "skip jev"; rules 3 and
   56 stay hand-read until the second measurement above.
6. Open, for the user: whether `Diff` (`vecDiff` 0.61) and `ack` (0.85 in
   the pilot's list run) join the rule 56 exception list in
   `Documents/C++StyleGuide.txt:264`; the codebase uses both, and neither
   reaches the 0.7 threshold in the tracked script's corpus run.
7. Open, for the user: whether the `LOG` level `choice` joins this Plan or
   waits for its own measurement.
8. The shared decisions in `JevDecisionModelWorkflowUses.md`.

## The instructions

The construct-shaped `noul` texts the results above were measured with, one
per rule. Each carries its own criteria pair: `true` restates the forbidden
construct with one or two examples, `false` restates the correct forms the
instruction lists; the `false` text for rule 3 adds "or there are no variable
declarations to judge", for rule 21 "a lone pointer parameter without a size,
or a std::span, is fine", and for rule 49 "or no function is defined". The
state is `{ code }`, referenced as `code` in each text.

- 3: Is there a variable in `code` whose name lacks the Hungarian prefix its
  type requires, or carries a prefix for a different type? Required prefixes:
  k constexpr, s static, g global, m class member (classes only, never struct
  members), p pointer, r reference, c char, i signed integer, ui unsigned
  integer, b bool, e enum value, f float or double, f2/f3/f4 XMFLOAT, vec
  XMVECTOR, mat XMMATRIX. These take NO prefix and are never violations:
  std::string and std::string_view, containers (std::vector, std::array,
  maps, sets), std::span, iterators, common::crc_t values (named crc), Vulkan
  handles (named with a vk prefix or Vk suffix), and instances of classes or
  structs. Function names are not judged.
- 14: Does an identifier in `code` use "Num" as a word meaning a count, where
  "Count" is required (kuiNumThreads is wrong, kuiThreadCount is right)?
  "Num" inside an ordinary English word such as Numerator, Number, or
  Enumerated is not a violation.
- 16: Does `code` index a std::map, std::unordered_map, or std::vector with
  operator[] (square brackets) in actual code, rather than .at(),
  insert_or_assign, or try_emplace? Square brackets inside a comment or a
  string literal, on a C array, on a std::span, or on a std::array are not
  violations.
- 21: Does a function declaration or definition in `code` take a raw pointer
  parameter paired with a separate size or count parameter (such as `const
  uint8_t* pData, size_t uiSize`) where std::span should be used, or does the
  code use std::bitset?
- 41: Does `code` name a C++ standard-library type or function without its
  std:: prefix, such as bare `vector`, `string`, `unordered_map`, `min`, or
  `move`? Types from other namespaces, engine types, Vulkan types, and DirectX
  Math types are not standard-library names. The fixed-width and size types
  size_t, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, and
  uint64_t are written without std:: by convention and are never violations.
  A line that already writes std:: is not a violation.
- 49: Does `code` define a trivial accessor or pass-through function: a
  getter, setter, Is*, Can*, drain, or take method that reads or writes one
  piece of state, or any function whose entire body is a single return of
  state, a single assignment, or a single call forwarded to another object?
  Spreading that one statement over several lines does not change the
  answer. A function whose body performs several statements that must happen
  together, or a serialization or codec adapter that defines a layer
  contract, is not a violation.
- 51: Does `code` split a function call's arguments or a declaration's
  parameters across more than one line (other than a lambda or
  brace-initializer argument), or wrap an if, while, assignment, or return
  Boolean expression across lines when that expression joined onto one line
  would be 140 columns or fewer? A split of a Boolean expression that is
  longer than 140 columns is allowed. A single very long line is never a
  violation.
- 62: Does `code` contain an if statement whose condition joins two or more
  independent guard conditions with || and whose body is a single exit
  statement (return, continue, or break), where each condition on its own
  should have been a separate if with its own exit? An || inside a Boolean
  assignment or a non-exit body, an && condition, and separate ifs with their
  own bodies are not violations.
- 56, block form (measured, then replaced by the list form): Does a variable
  or function name in `code` contain an abbreviated, truncated, or contracted
  word, such as ctx, cmd, buf, cnt, msg, calc, idx, tmp, prev, or pos, in
  place of the complete word? These are NOT abbreviations: the loop counters
  i, j, k; iterator names it or ending in It; Hungarian type prefixes (p, r,
  i, ui, b, f, e, c, m, k, g, s, vec, mat, f2, f3, f4, vk); and established
  identifiers of this codebase such as Vk, Crc, Ui, Xm, Gpu, Cpu, Uuid, Id,
  Rtt, Fps.
- 61 (measured, then rejected for a scanner): Does `code` contain an if or
  else whose body is a statement not enclosed in curly braces? This codebase
  uses Allman style: the line after `if (condition)` or `else` is an opening
  brace `{` on its own line, then the body, then `}`; that is CORRECT and is
  not a violation. A violation is when the line after `if (condition)` or
  `else` is a statement such as `return;` or `++iCount;` instead of an
  opening brace, or when the statement sits on the same line as the if.
- 56, list form, state `{ identifiers: [...] }` and one question per name
  with the name substituted: Is the C++ identifier `<name>` (one of the names
  listed in `identifiers`, all taken from the same code) an abbreviation:
  does it contain a shortened, truncated, or contracted word such as ctx,
  cmd, buf, cnt, msg, calc, dist, idx, tmp, prev, pos, or num in place of the
  complete word? Strip any leading Hungarian type prefix (p, r, i, ui, b, f,
  e, c, m, s, g, k, vec, mat, f2, f3, f4, rvec) before judging. NOT
  abbreviations: the loop counters i, j, k; the iterator name it or a name
  ending in It; a type name ending in _t; established acronyms Crc, Ui, Gpu,
  Cpu, Uuid, Id, Ns, Ms, Us; and ordinary whole English words. The name list
  is every identifier in the block after stripping comments, string literals,
  keywords, the namespace names `std`, `common`, `engine`, `game`, and
  `data`, fixed-width type names, all-capital macros, single-character names,
  names after `.`, `->`, or `::`, and names starting `vk`, `Vk`, `VK_`, `XM`,
  or `k` plus a capital.
