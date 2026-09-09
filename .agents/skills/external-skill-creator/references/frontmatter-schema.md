# Repository Skill Package v2

This is the repository authoring contract for Claude-facing `SKILL.md`
frontmatter and optional Codex `agents/openai.yaml`, not either client's full
parser specification. The allowlist, YAML forms, lengths, package layout, and
field relationships below are repository restrictions. Client invocation
controls remain independent.

## Document shape

- Encode the file as UTF-8. Use LF or CRLF line endings; do not use bare CR.
- Put an exact `---` opening delimiter on line 1 and an exact `---` closing delimiter after the frontmatter.
- Put at least one non-whitespace character in the Markdown body after the closing delimiter.
- Write each key once at top level as `key: value`.
- Use a plain scalar, a single- or double-quoted scalar, an exact lowercase boolean, a flow list, or the folded-text marker `>-` according to the field table.
- For `>-`, indent every nonblank content line and stop at the next top-level key or closing delimiter.
- Do not use comments, block lists, mappings, anchors, aliases, tags, arbitrary nesting, or other YAML block styles.

## Fields

| Key | Required | Accepted form | Constraint |
|---|---:|---|---|
| `name` | yes | text scalar | Equals parent directory; lowercase letters, digits, and single internal hyphens; at most 64 characters (repository rule) |
| `description` | yes | text scalar or `>-` | Nonempty; at most 1,024 characters (repository rule) |
| `when_to_use` | no | text scalar or `>-` | Nonempty; combined with `description` at most 1,536 characters (documented Claude limit; Claude appends it to `description`) |
| `allowed-tools` | no | flow list | String items |
| `paths` | no | flow list | Nonempty string items |
| `argument-hint` | no | text scalar | Nonempty |
| `disable-model-invocation` | no | boolean | Exact `true` or `false` |
| `user-invocable` | no | boolean | Exact `true` or `false` |
| `context` | no | enum | `fork` |
| `agent` | no | text scalar | Nonempty; requires `context: fork`, and `context: fork` requires `agent` |
| `model` | no | text scalar | Nonempty |
| `effort` | no | enum | `low`, `medium`, `high`, `xhigh`, or `max` |
| `shell` | no | enum | `bash` or `powershell` |

`arguments`, `hooks`, `background`, `license`, `compatibility`, and `metadata`
are documented Claude fields deliberately excluded from this repository
subset. A client parser accepting an excluded or unknown field does not add it
to this allowlist or prove that the client implements its behavior. Extend this
schema and the validator together when a repository skill first needs one of
these fields.

`disallowed-tools` is excluded by repository policy because Claude removes its
listed tools from the *invoking* context for the remainder of the turn. A skill
written for a delegated reviewer could therefore strip the caller's delegation,
editing, and user-interview ability when invoked inline. State intended tool
bounds in the body and use host permission configuration for enforcement.
Claude's `allowed-tools` pre-approves its listed tools for the invoking turn;
omitting a tool restricts nothing. Neither field is a portable permission
boundary.

## Scalar and list forms

Plain scalars occupy the remainder of one line after `:`. Quoted scalars may use YAML-style doubled single quotes or the double-quoted escapes `\"`, `\\`, `\n`, `\r`, `\t`, and `\uFFFF`. A quoted scalar must close on the same line.

Flow lists use brackets and comma-separated scalar items, for example:

```yaml
allowed-tools: [Read, Edit, "Bash(git diff *)"]
paths: ["**/*.vert", "**/*.frag"]
```

Nested lists or mappings are invalid. `argument-hint` remains a text scalar even when its text contains brackets, such as `[filename]`.

Folded text uses `>-`; consecutive nonblank lines fold with spaces and blank lines preserve paragraph breaks. Length checks apply to the parsed text, not indentation.

## Package members

Every file under the skill directory must be a top-level `SKILL.md`, `LICENSE`, or `LICENSE.txt`, the file `agents/openai.yaml`, or a file beneath `references/`, `scripts/`, or `assets/`; any other file is reported as invalid.

## Bundled links

Markdown links whose relative destination begins with `references/`, `scripts/`, or `assets/` must resolve beneath the skill directory. URL fragments and query strings do not participate in the filesystem check. Absolute paths, URI destinations, and links outside those bundled directories are not bundled links under this rule.

## Consumption shape, section order, and placement

`SECTION001` checks consumption shape, section placement, and order against
[`skill-skeleton.md`](../../../references/skill-skeleton.md) and its linked
main-session and subagent type checklists. A `## ` heading outside a fenced code
block that appears in a file the selected shape disallows, or after a heading
the skeleton orders later, is reported at its own line. Headings the skeleton
does not name are ignored, and an omitted section is valid.

## Handoff vocabulary

`VOCAB001` checks handoff vocabulary. Inside any fenced code block in `SKILL.md`, a line at column zero beginning exactly `Status:` or `Findings:` followed by a space or the end of the line carries closed vocabulary. After removing `<...>` placeholders, each `|`-separated value of a `Status:` line must be one of the status words, and each value of a `Findings:` line one of the severity words, that the shared handoff form in `.agents/references/subagent-reporting.md` `## Handoffs` lists. Prose outside a fenced block, and a line starting with anything else such as `Routed Findings:` or a list marker, is not checked.

## Handoff form

`HANDOFF001` checks that a `## Handoff` section does not re-render the shared handoff form. Inside a fenced code block under a `## Handoff` heading in `SKILL.md`, a line at column zero beginning `Status:`, `Findings:`, `Changed files:`, `Decisive checks:`, `Build required:`, `Evidence:`, `Executor:`, or `Residuals:` is reported at its own line. All eight names are invalid there, including a fixed-value row, which is written as prose instead. `.agents/references/subagent-reporting.md` `## Handoffs` owns the form. A `Status:` or `Findings:` line reports both `HANDOFF001` and, when its token is off-vocabulary, `VOCAB001`.

## Codex companion file

`interface`, `policy`, and `dependencies` are independently optional top-level objects in `agents/openai.yaml`; at least one must exist. Quote every string, indent with spaces, and omit comments. A present object requires:

- `interface`: nonempty `display_name` and 25–64-character `short_description`; optional nonempty `icon_small`, `icon_large`, `brand_color`, and `default_prompt`. Icons resolve inside the skill, color is `#RRGGBB`, and a default prompt names `$skill-name`.
- `policy`: exact boolean `allow_implicit_invocation`. `false` disables Codex implicit invocation but preserves explicit invocation.
- `dependencies`: nonempty `tools`; each item has quoted `type: "mcp"`, `value`, and `description`, with optional quoted `transport` and HTTPS `url`.

This companion file supplies Codex UI, invocation policy, and dependency
wiring. Loading or listing the package does not prove its implicit-invocation
policy or runtime behavior. Body tool and delegation bounds remain
authoritative. Never infer that Claude `disable-model-invocation: true`
requires a Codex companion file.

## Result contract

The mechanical command accepts exactly one target:

```powershell
pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path <skill-directory-or-SKILL.md>
```

Repository targets must be below `.agents/skills/`. Pass `-Fixture` only for a disposable external fixture.

- `VALID <path>` with exit `0`: every mechanical check passed.
- One or more line-ordered `INVALID <path>:<line> <code>: <message>` diagnostics with exit `1`: target content is invalid.
- `SETUP_ERROR <code>: <message>` with exit `2`: invocation, path resolution, file read, or internal validation failed.

For validator changes, create disposable packages under a temporary directory;
never track fixtures. Require `VALID`/0 from valid main-session and subagent
`-Fixture` packages; `INVALID`/1 from main-session section misordering and
subagent section misplacement;
`VALID`/0 with paired `context` and `agent` metadata in each shape;
`INVALID`/1 with `RELATIONSHIP001` for an incomplete pair; `INVALID`/1 after
corrupting a known field; `INVALID`/1 carrying `HANDOFF001` from a fixture whose
`## Handoff` fence contains a `Status:` line; `INVALID`/1 for broken bundled
links and off-contract handoff vocabulary; `SETUP_ERROR`/2 for nonexistent and
out-of-scope targets; and `VALID`/0 from the repository self-check.
