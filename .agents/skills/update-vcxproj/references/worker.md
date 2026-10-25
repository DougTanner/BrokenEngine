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
   | `Tools/ModelViewer/Source/**` | ModelViewer | source item; `Viewer` |
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
   times.

   In fix mode, an `Engine/**` or `Common/**` path that gains membership — an
   addition, a rename's new name, or the added side of an affinity change —
   goes through the script, never a hand edit. Run it once per path batch,
   outside the per-pair loop: one run applies each record to every game pair
   the `Projects/AGENTS.md` `## Standard Project Layout` defines. On exit `1`
   it writes nothing; on `input.too-many-paths` or `resolver.truncated`, rerun
   in smaller batches until every path is applied, and on any other code report
   its `code` and `message`. For a `.h` under `Engine/Data/Shaders/`, pass the
   scoped-authority answer as `-ShaderHeaderMembership` (`None` for a GLSL-only
   client `None` item; `Client`, `Server`, or `Both` for C++-consumed
   `ClInclude` sides); one answer applies to every shader header in a run, so
   give headers with different answers separate runs:

   ```powershell
   pwsh -NoProfile -File .agents/skills/update-vcxproj/scripts/Update-EngineVcxprojItem.ps1 <path> [<path> ...] [-ShaderHeaderMembership <None|Client|Server|Both>]
   ```

   An `Engine/**` or `Common/**` removal or a rename's old name goes through
   its `-Remove` form, run before a rename's add run:

   ```powershell
   pwsh -NoProfile -File .agents/skills/update-vcxproj/scripts/Update-EngineVcxprojItem.ps1 -Remove <path> [<path> ...]
   ```

   The removed side of an affinity change, game, tool, and DataPacker paths,
   generated `$(GameDataDirectory)` headers, and DataPacker membership of
   `Common/**` stay hand edits. Done when every affected item's ownership and
   filter come from its record and those counts hold in the pair.
3. Run once after inspection or hand repair:

   ```powershell
   pwsh -NoProfile -File .agents/skills/update-vcxproj/scripts/Invoke-VcxprojPairCheck.ps1 -ProjectPath <project>.vcxproj
   ```

   After an `Update-EngineVcxprojItem.ps1` run, run this multi-project form
   once in place of those projects' single-pair runs, validating every game
   client and server project the standard layout defines, not only those named
   in its output, because its per-path records can be truncated:

   ```powershell
   pwsh -NoProfile -Command "& '.agents/skills/update-vcxproj/scripts/Invoke-VcxprojPairCheck.ps1' -ProjectPath '<project>.vcxproj','<project>.vcxproj'"
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
