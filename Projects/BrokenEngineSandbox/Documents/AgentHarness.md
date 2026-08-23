# BrokenEngineSandbox Agent Harness

Project-specific launch configuration, verification routes, durable caveats, and command schemas for driving BrokenEngineSandbox through the [`/agent-harness` skill](../../../.agents/skills/agent-harness/SKILL.md). The skill owns provisioning, claiming, ownership, request/response transport, lifecycle, and release. The focused references below own the project details; read only the route needed by the scenario.

## Common convention

Every command request uses a JSON envelope with `cmd`, optional `params`, and optional `id`; every response places returned fields under `result`. Parameters are an external trust boundary, and a side-specific command sent to the other executable returns `unknown command`. The five engine-shared command schemas are in [command-reference.md](../../../.agents/skills/agent-harness/references/command-reference.md); game commands are split into [both-endpoint](AgentHarness/commands-both.md), [server](AgentHarness/commands-server.md), and [client](AgentHarness/commands-client.md) references.

## Scenario routing

| Scenario | Read |
| --- | --- |
| Before any launch | This hub and [launch](AgentHarness/launch.md) |
| Common verification setup | [verification](AgentHarness/verification.md), [server commands](AgentHarness/commands-server.md), and [client commands](AgentHarness/commands-client.md) |
| Replay determinism or transfer capture | [replay](AgentHarness/replay.md), [server commands](AgentHarness/commands-server.md), and [client commands](AgentHarness/commands-client.md) |
| Persistent-GUID replay scenario | [replay](AgentHarness/replay.md), [server commands](AgentHarness/commands-server.md), and [client commands](AgentHarness/commands-client.md); use `describe_ui`, `click`, and `describe_scene` for the natural client path |
| Cross-cell subscription | [cross-cell](AgentHarness/cross-cell.md), [server commands](AgentHarness/commands-server.md), and [client commands](AgentHarness/commands-client.md) |
| Server monitoring window | [server-window](AgentHarness/server-window.md); add [server commands](AgentHarness/commands-server.md) only when querying server state |
| Endpoint command lookup | [both-endpoint](AgentHarness/commands-both.md), [server](AgentHarness/commands-server.md), or [client](AgentHarness/commands-client.md), matching the endpoint |

## Machine-readable launch configuration

The claim consumer `scripts/Invoke-HarnessClaim.ps1` reads exactly one current assignment set from this hub with the regular expressions in its launch-document parser. It uses `$Output`, `$ServerExe`, and `$ClientExe` to check the requested executables before provisioning and claiming. Humans must follow the complete [launch recipe](AgentHarness/launch.md), which owns process checks, fresh-state choices, readiness, and the concrete arguments.

```powershell
$ROOT = '<absolute adopted worktree>'
$Output = Join-Path $ROOT 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output'
$ServerExe = Join-Path $Output 'BrokenEngineSandboxServer.Debug.exe'
$ClientExe = Join-Path $Output 'BrokenEngineSandbox.Debug.exe'
```

The Debug suffix is replaced by the requested configuration only by the claim script; do not edit this hub for another build.

## Focused references

- [Launch](AgentHarness/launch.md) — executable startup, process identity, readiness, fresh client state, and an additional client.
- [Verification](AgentHarness/verification.md) — common setup, chosen-player placement, and durable caveat routing.
- [Replay](AgentHarness/replay.md) — determinism, transfer fixtures, persistent GUID/cell crossing, and manifest integrity.
- [Cross-cell](AgentHarness/cross-cell.md) — client subscription movement and full-state adoption.
- [Server window](AgentHarness/server-window.md) — DPI-aware GDI capture and click recipe.
