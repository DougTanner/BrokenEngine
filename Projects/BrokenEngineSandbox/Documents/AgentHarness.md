# BrokenEngineSandbox Agent Harness

Project-specific launch configuration, verification routes, durable caveats, and command schemas for driving BrokenEngineSandbox through the [`/agent-harness` skill](../../../.agents/skills/agent-harness/SKILL.md). The skill owns provisioning, claiming, ownership, request/response transport, lifecycle, release, the shared launch and replay recipes, and the engine-owned command schemas. The focused references below own the project details; read only the route needed by the scenario.

## Common convention

Every command request uses a JSON envelope with `cmd`, optional `params`, and
optional `id`; every response places returned fields under `result`. A
side-specific command sent to the other executable returns `unknown command`.
The engine-owned command schemas — shared, client, and server — and the
cell-local position shape every result uses are in
[command-reference.md](../../../.agents/skills/agent-harness/references/command-reference.md);
this project's own commands, and the fields it adds to the engine commands it
takes over, are split into [server](AgentHarness/commands-server.md) and
[client](AgentHarness/commands-client.md) references.

## Scenario routing

| Scenario | Read |
| --- | --- |
| Before any launch | This hub, the shared [launch recipe](../../../.agents/skills/agent-harness/references/launch.md), and the project [launch notes](AgentHarness/launch.md) |
| Common verification setup | [verification](AgentHarness/verification.md), the [command reference](../../../.agents/skills/agent-harness/references/command-reference.md), [server commands](AgentHarness/commands-server.md), and [client commands](AgentHarness/commands-client.md) |
| Replay determinism or transfer capture | The shared [replay reference](../../../.agents/skills/agent-harness/references/replay.md), the project [replay](AgentHarness/replay.md), the [command reference](../../../.agents/skills/agent-harness/references/command-reference.md), [server commands](AgentHarness/commands-server.md), and [client commands](AgentHarness/commands-client.md) |
| Persistent-GUID replay scenario | [replay](AgentHarness/replay.md), the [command reference](../../../.agents/skills/agent-harness/references/command-reference.md), [server commands](AgentHarness/commands-server.md), and [client commands](AgentHarness/commands-client.md); use `describe_ui`, `click`, and `describe_scene` for the natural client path |
| Cross-cell subscription | [cross-cell](AgentHarness/cross-cell.md), the [command reference](../../../.agents/skills/agent-harness/references/command-reference.md), [server commands](AgentHarness/commands-server.md), and [client commands](AgentHarness/commands-client.md) |
| Server Monitor window | The shared [server-monitor recipe](../../../.agents/skills/agent-harness/references/server-monitor.md); add the [command reference](../../../.agents/skills/agent-harness/references/command-reference.md) only when querying server state |
| Endpoint command lookup | The [command reference](../../../.agents/skills/agent-harness/references/command-reference.md) for engine commands, then [server](AgentHarness/commands-server.md) or [client](AgentHarness/commands-client.md), matching the endpoint |

## Machine-readable launch configuration

The claim consumer `scripts/Invoke-HarnessClaim.ps1` and the launch script
`scripts/Invoke-HarnessLaunch.ps1` read exactly one current assignment set from
this hub through the shared `Get-AgentHarnessLaunchConfiguration`
(`.agents/scripts/AgentScriptCommon.psm1`). The claim uses `$Output`,
`$ServerExe`, and `$ClientExe` to check the requested executables before
provisioning and claiming, and the launch script starts them. The launch script
owns the process-check sequence, the readiness wait, and the game argument set;
humans must follow the complete shared
[launch recipe](../../../.agents/skills/agent-harness/references/launch.md),
which owns the launch-script calls, the fresh-state choices, and the readiness
text, with the project values from `## Launch values` below.

```powershell
$ROOT = '<absolute adopted worktree>'
$Output = Join-Path $ROOT 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output'
$ServerExe = Join-Path $Output 'BrokenEngineSandboxServer.Debug.exe'
$ClientExe = Join-Path $Output 'BrokenEngineSandbox.Debug.exe'
```

The claim and launch scripts, through `Get-AgentHarnessLaunchConfiguration`, and the `/agent-harness` release-only recovery map these Debug names to the requested configuration — `.Profile.exe` for Profile, and the unsuffixed `.exe` for Release; do not edit this hub for another build.

## Launch values

The project values the shared
[launch recipe](../../../.agents/skills/agent-harness/references/launch.md)
substitutes; its output directory and executable names come from the
machine-readable launch configuration above.

| Launch value | BrokenEngineSandbox |
| --- | --- |
| `<Project>` (the claim's `-Project`) | `BrokenEngineSandbox` |
| `<server process-name prefix>` | `BrokenEngineSandboxServer` |
| `<server game name>` (server `-GameName` and app-data child) | `Broken Engine Sandbox Server` |
| `<client game name>` (client `-GameName` and app-data child) | `Broken Engine Sandbox` |

## Focused references

- [Launch](AgentHarness/launch.md) — the project's client persisted state and launch caveats; the shared [launch recipe](../../../.agents/skills/agent-harness/references/launch.md) owns executable startup, process identity, readiness, and fresh client state.
- [Verification](AgentHarness/verification.md) — common setup, chosen-omni placement, and durable caveat routing.
- [Replay](AgentHarness/replay.md) — transfer capture, persistent GUID/cell crossing, and the project's replay control recipes; the shared [replay reference](../../../.agents/skills/agent-harness/references/replay.md) owns determinism acceptance and manifest integrity.
- [Cross-cell](AgentHarness/cross-cell.md) — client subscription movement and full-state adoption.
