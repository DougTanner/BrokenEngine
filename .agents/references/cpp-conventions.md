# C++ Conventions

These are the C++ conventions every implementer and reviewer of C++ applies.

- Error handling at trust boundaries only: assume function parameters from within the codebase are valid — no defensive validation between our own functions. Do validate anything opaque to the current code unit: network input, file reads, OS/third-party API results.
- No useless ASSERTs: an ASSERT that throws one line before the code would crash anyway adds false safety — remove it; prefer making the condition impossible in calling code, or recovering gracefully. `/repo-code-review` lists the preferred fixes in order, from best to last resort.
- Log levels: `kVerbose` — per-frame / high-frequency. `kDebug` — one-time (startup, connect). `kInfo` — state transitions, important one-shots (default threshold). `kWarning` — investigate (timeouts, desync); may spam. `kError` — failures; always logged. Runtime-threshold and compile-floor mechanics: `Common/Log/AGENTS.md`.
- Managers: Singletons via `gp*` globals (`gpGraphics`, `gpAudioManager`)
- DirectX Math: Prefer aligned versions (`Float4A` not `Float4`)
	- XMVECTOR W invariant: Positions W=1.0; directions / velocities / normals / offsets W=0.0; color alpha defaults 1.0 (opaque)
	- Function form, not operators: `XMVectorAdd`/`Subtract`/`Multiply`/`Divide`/`Scale`/`Negate` — never `vec + vec`, `f * vec`, `-vec`.
	- Rotating a vector uses `XMVector3RotateSafe`/`XMVector3InverseRotateSafe`: the SDK versions leave a rounding residue in W that breaks the invariant above, so `Common/ExternalHeaders.h` re-zeroes W and makes the raw names fail to compile.
- Base classes: Include/use game versions, not Base versions — `game::gpGame` not `GameBase` directly
- Workbuffer: Use `gpThreadLocal->mWorkbuffer` for temp allocations instead of local `std::vector`/`std::string`.
- Allocation tracking: Heap allocations in the main loop trigger `DEBUG_BREAK()`. When unavoidable, wrap with `ScopedSuppressAllocationTracking` + `// Heap:` comment. See `Engine/Source/Memory/AGENTS.md`
- LOG formatting: logging in allocation-tracked Game/Engine code must remain allocation-free; /repo-code-review owns accepted formatting and wrapper details
- Standard library / external headers: PCH-backed `#include`s go in `Common/ExternalHeaders.h`; PCH-less AgentTools use `Tools/ToolCommon/ToolCliCommon.h`. Rules and exceptions: `Common/AGENTS.md` and `Tools/ToolCommon/AGENTS.md`
- Flags over booleans: Use `common::Flags<EnumType>` instead of multiple `bool` variables.
- Multithreading: Use `common::gpMultithreading->Dispatch()` or `common::PersistentWorker` for data-parallel work.
