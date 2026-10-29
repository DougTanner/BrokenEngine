# Rebase resolution style and membership evidence

## Scope

- Worktree: `C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad`; comparison: `d170b18b843a3a96af227cef467b824b4aa214c5..1c3608533ba58d1a1327b51928ed4dced5426c53`.
- Reviewed the C++ regions identified in `Temp/lighting-rebase-evidence.md` `## Conflicts and resolutions`: CameraBase projection/comparison and framing members; Global/Lighting/Main/Water uniform call sites; ModelViewer camera update; GraphicsSettings persistence; TweaksScreen sun slider.
- The scope covered nine C++ paths: `Engine/Source/Graphics/CameraBase.cpp`, `.h`, four `Engine/Source/Graphics/Render/*Uniforms.cpp` files (Global, Lighting, Main, Water), `Projects/ModelViewer/Source/Graphics/Camera.cpp`, `Engine/Source/Ui/GraphicsSettings.cpp`, and `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp`. No ranges were unavailable.

## Style review

- Read `Documents/C++StyleGuide.txt`; hand-read the selected ranges for the required rules. No style violations were found.
- `Find-SessionCandidates.ps1`, baseline `d170b18b843a3a96af227cef467b824b4aa214c5`, scoped to the
  nine paths: `pass`, 4 candidates, not truncated. All four were Rule 44 candidates and were
  rejected: `CameraBase.cpp:431,433` load from `XMFLOAT3 f3ScreenPosition`, which is not aligned
  storage; `MainUniforms.cpp:253,256` store into shader `vec4`, which derives from `XMFLOAT4` and is
  not an aligned type. No temporary instrumentation candidates were reported. Result:
  `Temp/code-style-review-candidates-scoped.json`.
- `Invoke-IncludeOrderCheck.ps1 -Fix` on the in-scope C++ paths: `pass`, 7 files scanned, 0 violations, 0 rewrites.
- Search for `// TEMP START` / `// TEMP END` in all nine paths: no matches. `git diff --check` on the scoped C++ paths: exit 0.
- No source edits, renames, routed findings, or style-driven builds.

## Project membership verification

- Used `Resolve-VcxprojMembership.ps1` in verify mode for the nine paths: `pass`, 9 records, no omissions. Engine source records resolve to whole-file client affinity; ModelViewer camera resolves to ModelViewer. Scoped `Projects/AGENTS.md` `## Standard Project Layout`, ModelViewer `AGENTS.md` `## Engine integration` and `## Build and data ownership`, plus sandbox Visual Studio 2026 `## vcxproj File Inclusion Rules`, were checked.
- No `ClCompile Include`, `ClInclude Include`, or `None Include` item lines changed between oldtip and newtip across the three projects and filters; the project-file differences retain main's configuration collapse and its filter declaration removals. Current project entries match the expected memberships below, and every current project/filter pair checker passed.

| Path | Sandbox client | Sandbox server | ModelViewer | Filter |
|---|---:|---:|---:|---|
| `Engine/Source/Graphics/CameraBase.cpp` | yes | no | yes | `Engine\Graphics` |
| `Engine/Source/Graphics/CameraBase.h` | yes | no | yes | `Engine\Graphics` |
| `Engine/Source/Graphics/Render/GlobalUniforms.cpp` | yes | no | yes | `Engine\Graphics\Render` |
| `Engine/Source/Graphics/Render/LightingUniforms.cpp` | yes | no | yes | `Engine\Graphics\Render` |
| `Engine/Source/Graphics/Render/MainUniforms.cpp` | yes | no | no | `Engine\Graphics\Render` |
| `Engine/Source/Graphics/Render/WaterUniforms.cpp` | yes | no | yes | `Engine\Graphics\Render` |
| `Engine/Source/Ui/GraphicsSettings.cpp` | yes | no | yes | `Engine\Ui` |
| `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp` | yes | no | no | `Engine\Ui\Screens\TweaksScreen` |
| `Projects/ModelViewer/Source/Graphics/Camera.cpp` | no | no | yes | `Viewer` |

- Pair checks: BrokenEngineSandbox client `pass` (562 project/filter items); server `pass` (279); ModelViewer `pass` (288). No membership or filter edits were needed; verify-only membership work requires no build.

