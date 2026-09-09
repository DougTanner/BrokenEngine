# Skill Validation Evidence

Use this procedure after authoring or revising a skill. The repository package
contract lives in [`frontmatter-schema.md`](frontmatter-schema.md), and
client behavior lives in [`client-compatibility.md`](client-compatibility.md).

## Structural Validation

In author mode, run the PowerShell validator against the
`external-skill-creator` package and then the finished skill. Require `VALID`
and exit `0` from both, and record both command results in the author handoff.
These author mechanical checks do not replace independent review. Return the
validate-mode request to the manager, which dispatches a fresh reviewer for the
semantic and inbound-reference review and owns any finding-fix cycle.

The validator and independent semantic review are the authority for repository
frontmatter, companion files, package layout, bundled links, section placement,
handoff shape, and semantic behavior. Client parsing does not replace these
checks.

Measure every changed Markdown file with the command owned by
`/progressive-disclosure-review` and record its `bt-token-v1` count against that
skill's thresholds.

## Codex Loader Check

Run this bounded check when the change affects Codex loading or the handoff will
claim Codex loader compatibility. It uses no model turn and verifies discovery
and parsing only.

1. Record `codex --version`, the package path, and the lookup directory. For a
   repository skill, use this worktree as the lookup directory. For an
   uninstalled package, place a copy under a temporary lookup directory at
   `.agents/skills/<name>/`. Done when the lookup directory contains only the
   intended fixture changes.
2. Create an empty temporary state directory and start `codex app-server
   --stdio` with `CODEX_HOME` set only for that process. Keep its input open.
   Done when the process accepts JSON messages or its startup failure is
   recorded.
3. Send newline-delimited `initialize`, `initialized`, and `skills/list`
   messages in that order. Pass the absolute lookup directory in `cwds` and
   set `forceReload` to `true`; wait for initialization before listing skills.
   Done when the list response arrives or a 20-second timeout or process failure
   is recorded.

   ```json
   {"id":1,"method":"initialize","params":{"clientInfo":{"name":"skill_compat_audit","version":"1.0"}}}
   {"method":"initialized","params":{}}
   {"id":2,"method":"skills/list","params":{"cwds":["/absolute/lookup/directory"],"forceReload":true}}
   ```

4. Inspect `result.data[].skills` and `result.data[].errors`. Require the target
   name and path with `enabled: true` and no target-package error. Record
   unrelated errors separately. Done when the target has a bounded loader
   verdict backed by the response.
5. Stop the process and remove only the temporary state and fixture paths made
   for this check. Done when those paths are gone and personal configuration
   and credentials remain untouched.

Use the App Server schema for the exact message envelope supported by the
installed version. Retain the version, request, target result, and errors in a
bounded `Temp/` evidence file when later reviewers need the details.

## Focused Host Checks

Run a harmless focused check in each intended host only when the authored skill
changes client-dependent behavior or the handoff claims that behavior works.
Check the affected surface: explicit or implicit invocation, chaining,
argument substitution, path or shell gating, tool controls, delegation, or a
representative workflow. Record the host version, input, expected observation,
actual observation, and temporary output path.

Use a fresh conversation when prior explicit invocation could contaminate an
implicit-invocation observation. Follow the host's normal permissions for any
new session or delegation. Do not create model turns solely to replace an
unverified result; when a host or trace is unavailable, report the affected
behavior as unverified and narrow compatibility claims accordingly.

## Evidence Classes

Report each intended client and version separately:

- structural — author mechanical self-and-target results, with the independent
  repository `/external-skill-creator` validate-mode result pending until the
  manager completes that review;
- loader — discovery and parsing result, or unverified;
- runtime — client behavior directly observed, with the checked case;
- documentation — authoritative behavior used to design the controls; and
- unverified — every claimed-relevant behavior that no executed check settled.

The current session following revised instructions is workflow execution
evidence. It does not prove that a fresh host loaded the revised file or that an
invocation policy works.
