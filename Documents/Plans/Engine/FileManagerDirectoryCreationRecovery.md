<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:02.476Z","dependsOn":[]} -->
# Report FileManager directory-creation failures through startup handling

## Context

Final survivor `S005-C013` is a retained HIGH startup failure finding. `FileManager::FileManager` uses throwing `create_directory` calls for AppData and temp children (`Engine/Source/File/FileManager.cpp:106-162`). `wWinMain` constructs FileManager before the `MainThread` try/catch, so a conflicting file, denied parent, or other directory failure escapes the required `kError` startup log and modal/nonmodal policy.

The final disposition for `S005-C013` is recorded at `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:126`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:254` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:144`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to use the existing error-reporting path for each AppData/temp directory creation failure, preserving the failing path and startup agent/non-agent policy. Ensure construction cannot leave a partially published unusable FileManager; retain successful existing-directory and normal directory-creation behavior.

## Critical files

- `Engine/Source/File/FileManager.cpp:106-162` — path setup and directory creation.
- `Engine/Source/Main.cpp:417-427` — construction relative to the current startup catch.
- `Engine/Source/AGENTS.md` and `Engine/Source/File/AGENTS.md` — startup diagnostics/path contracts.

## In scope

- Error-aware AppData and temp directory creation during FileManager construction.
- Failure publication/cleanup before the runtime treats FileManager as usable.
- Valid existing-directory and successful-creation startup behavior.

## Out of scope

- Launch option parsing, path identity/reparse validation, crash-reporting internals, or DataPacker FileManager behavior.
- New startup UI, retry/backoff, directory migration, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped runtime startup I/O behavior). Trigger: an opaque filesystem result currently escapes startup failure handling, but the correction is confined to FileManager construction and its existing diagnostic policy without changing serialization, wire, deterministic, or threading structure.

Preserve these invariants:

- A directory-creation failure logs at `kError` and follows the documented agent/non-agent startup policy.
- No partially constructed FileManager or global is left usable after failure.
- Existing valid AppData/temp directories and successful creation retain current paths and cleanup.

## Acceptance criteria

- A conflicting file or denied directory creation for each root produces the expected `kError` diagnostic and startup failure policy before `MainThread` runs.
- Valid existing and creatable roots still construct, publish, and tear down FileManager normally.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `S005-C013`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:126`, source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:254`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:144`. No source fix or build was performed during routing.
