# Update vcxproj Membership — Worker

## Steps

Use targeted searches near same-directory/type anchors; never load or echo whole
XML. Preserve relative path form. For a new filter, add missing ancestors and a
unique lowercase-hex `{8-4-4-4-12}` GUID.

1. Classify special cases before generic extension rules:

   | Path | Project ownership | Item/filter |
   | --- | --- | --- |
   | GLSL stages/includes under engine/game `Data/Shaders` | game client | `None`; mirrored `Engine\\Data\\Shaders` or `Game\\Data\\Shaders` |
   | generated `$(GameDataDirectory)\\*.h` | scoped game targets; omit client-only `Shader.h` from server | `ClInclude`; `DataFiles` |
   | `.h` under shader trees | scoped game affinity if C++ consumed; otherwise client | `ClInclude` or `None`; owning shader filter |
   | `Tools/ToolCommon/**` | AgentHarness and WorktreeCli | source item; `ToolCommon[...]` |
   | `Tools/AgentHarness/**` | AgentHarness | source item; `AgentHarness[...]` |
   | `Tools/WorktreeCli/**` | WorktreeCli | source item; `WorktreeCli[...]` |
   | `DataPacker/Source/**` | DataPacker | source item; `DataPacker[...]` |
   | `Engine/Source/**` | client/server/both by structural affinity | source item; `Engine[...]` |
   | game `Source/**` | client/server/both by structural affinity | source item; `Game[...]` |
   | `Common/**` | game client/server; DataPacker only by scoped authority | source item; `Common[...]` |

   Resolve whole-file affinity and the owning row with one run over every
   affected path that exists on disk — additions, new names, affinity changes;
   never reconstruct the prologue scan or this table's lookup inline. A removal
   or an old rename name needs no record: the authorized change itself says the
   item goes.

   ```powershell
   pwsh -NoProfile -File .agents/skills/update-vcxproj/scripts/Resolve-VcxprojMembership.ps1 <path> [<path> ...]
   ```

   Each record carries `affinity` (`client`, `server`, `shared`, `unprovable`)
   with its `affinityCode`, and `mapping` (`resolved`, `conditional`,
   `unresolved`, `non-member`) with its `mappingCode`, `projects`, `itemType`,
   and `filter`. `resolved` is authoritative. `conditional` settles everything
   except the one named question — generated `$(GameDataDirectory)` headers,
   `.h` under a shader tree, DataPacker membership for `Common/**` — which
   scoped authority decides. `non-member` is a path deliberately never carried
   as a project item, a routine result and not a blocker. Stop and report the
   path, never guess, when `affinity` is `unprovable`, when `mapping` is
   `unresolved`, or when the run omits a record; on `files.truncated`, reinvoke
   in smaller batches until every path has a record. Preserve a documented
   forced-include exception and report `NOTE`. Done when every affected path on
   disk has a record whose `affinity` and `mapping` are read, or the blocking
   path is reported.

For each affected project pair:

2. Take a `resolved` record's ownership and logical filter as given. On a
   `conditional` record, answer only its one named question from scoped
   authority (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`)
   and take the rest of the record as given; re-derive nothing by hand. In fix
   mode, require each intended item once and forbidden/stale old-name items zero
   times. Done when every affected item's ownership and filter come from its
   record and those counts hold in the pair.
3. Run once after inspection or repair:

   ```powershell
   pwsh -NoProfile -File .agents/skills/update-vcxproj/scripts/Test-VcxprojPair.ps1 -ProjectPath <project>.vcxproj
   ```

   Require exit `0`. On compact failure, report its `code` and violations, fix
   only authorized XML, and rerun. A pass is authoritative for XML parsing,
   project/filter mirroring, filter declarations/ancestors, and GUID uniqueness;
   do not recreate those checks manually. Done when the run exits `0`.
4. Leave any unresolved invariant as `FAIL`; partial reconciliation is not
   success. Done when the returned status reflects every invariant's state.

## Rules

- It never delegates, edits source, builds, or defines
  landing-commit/finalization policy.
- Read root and scoped `AGENTS.md`, including the VisualStudio2026 project
  guidance; stop on conflicting authority.
