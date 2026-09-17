# Client Compatibility

Read this reference when a skill adds or changes client-specific metadata, prompt substitutions, interaction tools, delegation, or invocation policy. Keep the main `SKILL.md` workflow portable and place client-only mechanics here or in another focused reference.

## Shared Package

Claude Code, Codex, and OpenCode consume the skill directory and `SKILL.md`, but their policy surfaces are independent. Write portable instructions in ordinary language, then configure every supported client explicitly. Never infer invocation behavior from an `external-` name.

Repository frontmatter follows [`frontmatter-schema.md`](frontmatter-schema.md). The repository validator, not a client installation, owns its accepted fields and relationships.

## Behavior Map

| Concern | Claude Code owner below | Codex owner below | OpenCode owner below |
| --- | --- | --- | --- |
| Invocation and activation | Frontmatter controls in `## Claude Code` | Companion policy in `## Codex` | Agent and command configuration in `## OpenCode` |
| Tool and prompt behavior | Claude-only controls and syntax in `## Claude Code` | Native host mechanisms in `## Codex` | Agent permissions and portable body instructions in `## OpenCode` |

Repository schema acceptance, client loader acceptance, documented behavior, and
observed runtime behavior are separate evidence. A loader accepting a field does
not establish that the client enforces it.

## Claude Code

Claude Code reads the repository frontmatter controls. For a genuinely user-only skill, set `disable-model-invocation: true`; this prevents automatic use and native skill-to-skill invocation. Omit it when a documented workflow must invoke the skill, and make the description state the exact automatic or chaining contexts. Set `user-invocable: false` only for a model-only skill: it hides the skill from the menu and prevents explicit `/skill-name` invocation. `when_to_use` is appended to `description` within their combined 1,536-character cap, `paths` gates activation by matching files, and `shell` selects Bash or PowerShell for commands. This package permits automatic validate-mode selection while author mode remains explicitly requested.

Users invoke a listed Claude skill explicitly as `/skill-name`; `.claude/skills` exposes the repository package.

The `external-architecture-review` and `external-refactor-clean` skills are native-chain exceptions: they may be invoked by an explicitly documented parent workflow even though ordinary implicit matching stays disabled. Model the exception with client policy, not the `external-` prefix.

Claude-only prompt features belong in a compatibility reference:

- `$ARGUMENTS` is the full invocation argument string; `$ARGUMENTS[N]` and `$N` select zero-based positional arguments; `$name` selects a named argument declared by the `arguments` field, which this repository schema does not accept. Claude appends supplied arguments only when no argument placeholder receives them. Escape a literal argument substitution with one backslash, for example `\$ARGUMENTS`.
- `${CLAUDE_SESSION_ID}`, `${CLAUDE_SKILL_DIR}`, `${CLAUDE_PROJECT_DIR}`, and `${CLAUDE_EFFORT}` expose session, package-directory, project-directory, and effort values.
- Inline shell injection uses a backtick-delimited command prefixed by `!`; its output replaces the placeholder before the prompt loads. The multiline form uses a fenced block whose info string is `!`. Describe that form in prose inside skills because a literal multiline opener can be executed by the loader. Managed policy can disable this feature.
- `AskUserQuestion` is Claude's structured choice prompt, and `Agent` is its delegation tool. State the human interaction or delegation outcome in shared instructions; name these tools only in Claude-specific guidance.
- The word `ultrathink` enables Claude Code extended thinking. Do not include it in portable instructions unless that client behavior is intentional.

Use `allowed-tools` only for tools the workflow actually needs; it preapproves
those tools for the invoking turn. This repository bans `disallowed-tools`,
which removes tools for that turn; follow the schema's body-instruction
alternative.

## Codex

Users invoke a Codex skill explicitly as `$skill-name`. Codex reads optional client metadata from `agents/openai.yaml`; implicit-invocation policy lives under `policy`, independently of Claude frontmatter, and follows the schema linked above. Repository acceptance of Claude-only frontmatter is structural, while its implemented Codex behavior remains unverified. The minimal companion file is:

```yaml
policy:
  allow_implicit_invocation: false
```

Codex does not interpret Claude substitutions, shell injection, `AskUserQuestion`, `Agent`, or the Claude extended-thinking keyword as portable skill behavior. Express the intended outcome in the shared workflow and use Codex-native interaction, collaboration, shell, and permission mechanisms at execution time.

## OpenCode

OpenCode discovers repository skills from `.agents/skills`, but reads only its
documented portable metadata subset. It does not enforce this repository's
Claude `allowed-tools` field; `disable-model-invocation` is enforced on
OpenCode by the `.opencode/opencode.json` `permission.skill` map, where each
user-only skill has a hand-added `"<skill-name>": "ask"` entry that prompts the
user before the skill loads. Keep tool and delegation bounds in the
shared body, and enforce worker delegation denial in the OpenCode agent
permission configuration.

Users invoke an OpenCode command as `/command-name`. A command that exposes a
user-only workflow must state the user's intent explicitly in its template and
pass `$ARGUMENTS`; `/next-plan` is such an adapter. OpenCode's tool named `bash`
uses the repository-configured `pwsh` shell, so shared instructions continue to
use the canonical PowerShell command form rather than assuming Bash syntax.

## Cross-Client Check

Before claiming support across the three clients, verify:

1. shared instructions do not require one client's syntax;
2. Claude frontmatter matches Claude automatic, manual, and chaining behavior;
3. `agents/openai.yaml` matches Codex implicit and explicit behavior;
4. OpenCode agent/command configuration and the `.opencode/opencode.json` `permission.skill` map supply any invocation and permission behavior its skill loader ignores;
5. every referenced resource exists and is linked from `SKILL.md` when needed;
6. `/external-skill-creator` validate mode passes for frontmatter and bundled links.

Use [`validation.md`](validation.md) to record those checks by evidence class.
The current client semantics above are documented by
[Anthropic's skills reference](https://code.claude.com/docs/en/skills), while
Codex companion policy and loader mechanics are documented by
[OpenAI's skill guide](https://learn.chatgpt.com/docs/build-skills) and
[App Server guide](https://learn.chatgpt.com/docs/app-server). OpenCode skill,
agent, command, and shell behavior is documented by its
[skills](https://opencode.ai/docs/skills/),
[agents](https://opencode.ai/docs/agents/),
[commands](https://opencode.ai/docs/commands/), and
[configuration](https://opencode.ai/docs/config/) references. Record the
installed client versions and observation dates when runtime behavior matters.
