<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-19T22:17:17.210Z","dependsOn":[]} -->
# Remove the executed UpdateThirdPartySubmodules feature document

## Context
`Documents/Features/Engine/UpdateThirdPartySubmodules.md` describes, as future
work, moving every ThirdParty submodule to its mainline head, adding `branch =`
lines, and adding nlohmann/json as its own submodule. That work is already in
the tree:

- Every gitlink equals the "Head SHA seen" column of the document's
  `## Branch choices` table (for example `ThirdParty/imgui` `9d9a621f`,
  `ThirdParty/tinygltf` `0d9ce1a9`, `ThirdParty/mimalloc` `6f45cc52`;
  `git ls-tree HEAD ThirdParty/`).
- `.gitmodules` has 24 entries, each with a `branch =` line, including
  `[submodule "ThirdParty/json"]` at `.gitmodules:85-88`.
- `Common/ExternalHeaders.h:307-311` and `Tools/ToolCommon/ToolCliCommon.h:19-20`
  include `json/single_include/nlohmann/json.hpp`; `ExternalHeaders.h:380-384`
  and `ThirdParty/Prebuilts/Source/DataPacker/tinygltf.cpp:6-9` define
  `TINYGLTF_NO_INCLUDE_JSON` and include that json first; no `5311` pragma
  remains in either header.

The document's present-tense claims are therefore false: `:11-14` says only
lz4 carries a branch line and that nlohmann::json comes from tinygltf's
vendored copy, and `:319-322` and `:513-518` cite C5311 pragma lines in
`ExternalHeaders.h` and `ToolCliCommon.h` that no longer exist. Its line
citations into `ExternalHeaders.h` also drift with every edit of that header.

## Design
Recommended (the author's choice; the claiming session confirms it with the
user at approval): delete the document. `Documents/Features/AGENTS.md` scopes
the tree to ideas saved for another day; an executed one is not, and the code
and Git history now carry its outcome. No tracked file outside `Temp/` names
the document (`grep -rn UpdateThirdPartySubmodules` over `.agents`,
`Documents`, and every `AGENTS.md`), so no citation needs rewriting; the
executing session reruns that search.

Alternative, not recommended: keep it and rewrite `## Context` and the stale
json, tinygltf, and C5311 statements as a completed record.

## Critical files
- `Documents/Features/Engine/UpdateThirdPartySubmodules.md`

## In scope
- Deleting `Documents/Features/Engine/UpdateThirdPartySubmodules.md`, plus any
  citation of it the rerun search finds.

## Out of scope
- Any submodule, `.gitmodules`, ThirdParty, or source change.
- Other Feature documents.

## Risk tier and invariants
Tier 1 — documentation only.

## Acceptance criteria
- The document is gone and `git grep -n UpdateThirdPartySubmodules` returns
  nothing.
