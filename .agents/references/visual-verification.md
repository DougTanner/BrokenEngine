# Visual and Rendering Verification

Human-in-the-loop verification workflow for features whose primary acceptance
criterion is visual quality.

## When to use

Use this workflow for visual, rendering, shader, lighting, water, smoke,
particle, material, or UI-appearance features where human visual judgment
decides acceptance.

## Verification loop

1. Implement the approved change and run `/compile` for the affected targets,
   including DataPacker export when shaders or assets changed.
2. Stop before running automated visual `/agent-harness` checks.
3. Open the active project's Visual Studio solution files via PowerShell
   `Start-Process` so the user can build and run in Visual Studio:
   - Game project:
     `Projects/<Project>/Platforms/VisualStudio2026/<Project>.sln` and
     `Projects/<Project>/Platforms/VisualStudio2026/<Project>Server.sln`
     (for example, `BrokenEngineSandbox.sln` and
     `BrokenEngineSandboxServer.sln`).
   - ModelViewer:
     `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.sln`.
4. Tell the user what visual behavior to inspect and which Tweaks controls or
   camera views apply.
5. Wait for user visual feedback. Iterate on requested adjustments through this
   same loop, and proceed to finalization only after the user signs off.

## Boundary with `/agent-harness`

Keep [`/agent-harness`](../skills/agent-harness/SKILL.md) for non-visual
runtime checks (replay determinism, state queries, crash or startup tests) and
for explicit user requests (screenshots or RenderDoc captures). Use this
workflow instead of `/agent-harness` for human visual sign-off.
