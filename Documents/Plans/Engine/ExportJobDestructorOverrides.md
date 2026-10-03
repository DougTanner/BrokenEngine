<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:24:58.169Z","dependsOn":[]} -->
# Mark export-job destructors as overrides

## Context

`DataPacker/Source/ExportJobs/ExportJob.h:16` declares the public `virtual ~ExportJob() = default;`. Six direct derived classes explicitly default their destructors with `virtual` but omit `override`. Adding `override` states the inherited destruction contract and makes a future loss of the base virtual destructor a compile-time error, without runtime work. `Documents/C++StyleGuide.txt` rule 29 already requires `override` on child virtual functions; no style-guide or AGENTS.md amendment is warranted.

Source evidence was checked against session baseline `d29fed456d3ede935c5e672f95f13d6733f0660c`. Existing Plans searched for `override`, `destructor`, and the six class names contain no owner for this change.

## Design

In each of the six declarations below, replace `virtual ~ExportX() = default;` with `~ExportX() override = default;`. Keep each declaration explicit, public, and defaulted. Deleting a user-declared destructor can affect implicit move generation; this Plan retains the declarations and changes only the spelling of their existing virtual contract. Do not add `final` or an explicit exception specification.

## Critical files

All paths in the table are beneath `DataPacker/Source/ExportJobs/`.

| File and inspected line | Exact replacement declaration |
| --- | --- |
| `ExportScene.h:34` | `~ExportScene() override = default;` |
| `ExportTexture.h:22` | `~ExportTexture() override = default;` |
| `ExportRaw.h:20` | `~ExportRaw() override = default;` |
| `ExportIsland.h:33` | `~ExportIsland() override = default;` |
| `ExportShader.h:17` | `~ExportShader() override = default;` |
| `ExportModel.h:21` | `~ExportModel() override = default;` |

Read-only contract references: `DataPacker/Source/ExportJobs/ExportJob.h`, `DataPacker/Source/AGENTS.md`, `DataPacker/Source/ExportJobs/AGENTS.md`, and `Documents/C++StyleGuide.txt` rule 29.

## In scope

Only the explicit defaulted destructor declarations of `ExportScene`, `ExportTexture`, `ExportRaw`, `ExportIsland`, `ExportShader`, and `ExportModel`, at the six paths above.

## Out of scope

- Other virtual methods, other export classes, or a repository-wide override cleanup.
- Changes to `ExportJob`, constructors, copy/move members, exception specifications, inheritance, access, or destructor bodies.
- Export behavior, input validation, cache or packed formats, version numbers, generated assets, and project membership.
- Documentation/style policy changes, new tests, and performance infrastructure.

## Risk tier and invariants

Future implementation is Tier 1: a mechanical, behavior-preserving declaration edit with no public signature or invariant change. Reclassify if implementation needs to alter destructor behavior, special-member generation, or any export contract.

Preserve virtual destruction and dispatch, the implicit exception specifications, explicit defaulting, access, special-member generation, class layout, and all runtime behavior. Add no allocation, branch, dispatch, or other runtime cost. Exported bytes and all cache identities remain unchanged.

## Acceptance criteria

1. Each of the six declarations exactly matches its replacement in the table; no leading `virtual` remains on those declarations.
2. The implementation diff contains only those six declaration edits; the base destructor and all other members remain unchanged.
3. DataPacker compiles and links successfully in `Release|x64` through `/compile`.
4. Review of the declarations confirms the stated invariants: the destructors remain explicitly defaulted overrides of the existing public virtual base destructor, with no added runtime work.

## Verification

Inspect the six-line source diff and the unchanged `ExportJob` base destructor, then run the DataPacker `Release|x64` target through `/compile`. Retain the build result as evidence for acceptance criterion 3. Apply the normal implementation C++ review and owning-documentation review; the latter should require no prose changes because no documented contract changes.

No asset export, `/agent-harness` run, runtime benchmark, or unit test is required: the diff establishes unchanged behavior and the build checks the override relationship. These are future implementation checks; no build or runtime check was run while authoring this Plan.
