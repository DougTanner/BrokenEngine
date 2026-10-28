# BrokenEngineSandbox Data - Game Asset Inputs

Game-specific source and intermediate assets consumed by DataPacker. Reusable engine assets belong under `Engine/Data`.

- Preserve per-asset licenses and attribution sidecars. `LICENSE.md` requires checking each asset's terms.
- Filename tags, `Raw/`, audio loop/music naming, intermediate formats, and packed output rules are owned by DataPacker ExportJobs (`../../../DataPacker/Source/ExportJobs/AGENTS.md`).
- Shaders (`Shaders/AGENTS.md`) contains only the project's place where new layout fields can be added safely; engine shader rules remain in `Engine/Data/Shaders`.
- [ModelViewer source metadata authoring](../../ModelViewer/AGENTS.md#source-metadata-authoring)
  owns live alignment editing, Save and preservation of user-authored sidecars.
- Do not add AGENTS.md files to individual asset folders.
