# Codex Review Receipts

Receipt fields and exit codes for the prompt-assembly script `SKILL.md` runs.
`SKILL.md` owns the steps that consume them.

## Prompt assembly receipt

Every receipt carries `messageLength` and `messageTruncated`, because `message`
is capped at 256 characters and the flag says when it was cut. Exit `0` carries
`promptPath`, `promptBytes`, `fileCount`, `binaryExcluded`, `sectionsWritten`,
and `targetsPath`. For `repo-code-review`, `targetsPath` is the newly created
targets file next to the prompt and that file is embedded in the evidence. Other
assigned skills report `targetsPath: null`.

## Blocked exit codes

Exit `2` is blocked and its `code` names the fix: `prompt.path-exists` — choose
an unused prompt path, the existing file is left untouched;
`prompt.diff-too-large` — the evidence passed the 4 MB budget, so split the
review into smaller authorized scopes and never truncate the evidence;
`prompt.inventory-truncated` — narrow the baseline or the reviewed scope until
the evidence is complete; `prompt.assigned-skill-unknown` — `-AssignedSkill`
names no skill file, so fix the name or pass `-AdHocRole`;
`prompt.execution-card-required` — a `plan-audit` scope carries no
`execution card` marker; `prompt.execution-card-incomplete` — that scope's
execution card leaves the named field(s) unfilled or carries none of them, so
fill each one in `-ScopeFile` and re-run.

## Error exit codes

Exit `1` is a script error rather than a block, and its `code` names the failing
step. `prompt.execution-card-check-failed` is one of them: the checker behind
the two execution-card codes above could not be run or returned no usable
result, and the message carries that checker's own `code` and `message` when it
produced them.
