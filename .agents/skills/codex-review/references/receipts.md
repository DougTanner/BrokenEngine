# Codex Review Receipts

Receipt fields and exit codes for the two scripts `SKILL.md` runs. `SKILL.md`
owns the steps that consume them.

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
`prompt.inventory-truncated` — name every untracked path or narrow the baseline
until the evidence is complete; `prompt.untracked-path-unknown` — a named path is
not an untracked file, which includes a gitignored path such as one under
`Temp/`; `prompt.head-untracked-conflict` — a commit-valued head has no untracked
side; `prompt.assigned-skill-unknown` — `-AssignedSkill` names no skill file, so
fix the name or pass `-AdHocRole`; `prompt.execution-card-required` — a
`plan-audit` scope carries no `execution card` marker.

## Review run receipt

The `.codex/codex-review.ps1` `.NOTES` header documents the well-formed-result
check, the automatic retry behind it, and every receipt field.
