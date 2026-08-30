# IBL prepass failure aggregation and output recovery

Status: Open investigation; no implementation decision has been made.

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CPT/shard-0006/004` in the frozen C++ Plan Trace Audit.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Finding under investigation

`ProcessKtxCubemaps` and `ProcessFaceImageCubemaps` can throw while loading a
source, filtering it with CMFT, or writing the prefiltered output
(`DataPacker/Source/ExportJobs/ExportCubemapIbl.cpp:378-528`). The irradiance
loop has the same direct-failure shape
(`ExportCubemapIbl.cpp:239-323`). `MainThread` invokes both IBL prepasses
without folding a result into `bSuccess` and before the remaining export jobs
(`DataPacker/Source/Main.cpp:705-725`). A single malformed input, CMFT error,
or output error can therefore unwind before the later IBL pass, ordinary asset
exports, generated headers, and attribution work run, instead of becoming the
structured aggregate required for each asset type.

The earlier decision-complete route also assumed that existing temporary-output
cleanup would preserve a prior IBL result. The frozen IBL implementation does
not use the ordinary `ExportJob` temporary-file publication path. Each dirty
item calls `BeginOutputUpdate`, which creates the dirty sidecar and removes the
completed fingerprint metadata (`ExportCubemapIbl.cpp:105-121,270,407,497`).
The irradiance writer opens `outputPath` directly with
`std::ios::out | std::ios::binary` (`:310`), and `WriteFilteredCubemap` opens
its `rOutputPath` directly with the same mode (`:366-373`). Opening the final
path this way can truncate an existing complete output before all bytes have
been written. `CompleteOutputUpdate` records the fingerprint only after the
write succeeds (`:113-121`), so a failure leaves the recovery state and the
relationship between the final bytes, dirty marker, and sidecar unresolved.

`ReconcileIblOutputs` removes producer-owned orphan paths only when its mirrored
sidecar proves ownership (`ExportCubemapIbl.cpp:127-161`). It does not by
itself define how an expected output that was directly truncated during a
failed attempt is restored or invalidated. The durable source trace therefore
proves both the aggregate-failure gap and an open published-output recovery
boundary; the ignored shard report is supplementary provenance.

## Controlling contract and invariant

`DataPacker/Source/AGENTS.md:9` requires every asset type to run after earlier
failures and each type's failures to become one structured aggregate
diagnostic. `DataPacker/Source/ExportJobs/AGENTS.md` requires IBL cache
fingerprints, dirty markers, and mirrored sidecars, and states that incomplete
writes remain dirty. The same authorities describe ordinary export jobs as
writing temporary files and publishing only complete results, with a failed
temporary output unable to replace the prior published output.

The unresolved invariant is whether and how that complete-output guarantee is
implemented for the IBL prepass's direct final-path writers. A future choice
must keep the output, dirty marker, and fingerprint sidecar mutually truthful,
prevent one producer from removing another producer's file, and let later
asset types run while preserving all valid IBL bytes and fingerprints.

## Boundary and impact

The open boundary is the two IBL prepass entry points, their per-input failure
capture, `MainThread` result folding, and publication/recovery of the
irradiance and prefiltered intermediates. It includes source loading, CMFT,
direct output writes, dirty markers, completed sidecars, and orphan
reconciliation. It excludes CMFT algorithms, cubemap formats and settings,
cache fingerprint formulas, output naming, raw texture routing, ordinary
`RunExportJobs` aggregation, and runtime source.

If an input or write fails after the final path is opened, the previous
complete intermediate can be replaced by partial bytes or left with a dirty
marker and no completed fingerprint. If the exception instead escapes the
prepass, unrelated output types are not attempted and the promised aggregate
diagnostic is lost. Either failure path can leave the next run unable to tell
which bytes are safe to publish without an explicit recovery policy.

## Open choices

These alternatives are recorded for a future decision; none is selected here.

1. **Staged atomic publish.** Write each result to a same-directory temporary
   path, verify the complete intermediate, then publish it and its fingerprint
   sidecar in an order that leaves the previous complete output usable until
   the replacement is ready. Define the exact dirty-marker lifecycle,
   crash/interruption cleanup, temporary filename ownership, and rename or
   replace behavior on the output tree.
2. **Restoration around direct publication.** Keep the direct output path but
   preserve the prior output and its metadata before truncation, restoring the
   prior pair on any load, CMFT, or write failure. Define whether the backup is
   itself atomic and how process termination during write or restoration is
   recovered by the next run; a best-effort copy is not sufficient unless the
   output contract explicitly accepts that loss window.
3. **Another proven recovery mechanism.** Reuse an existing publication or
   cache-recovery mechanism if repository evidence establishes that it covers
   IBL's same-path outputs, dirty sidecars, and producer ownership. If no such
   mechanism exists, the future Plan must explicitly record why a new staged
   or restoration boundary is required rather than relying on the current
   direct writer.

The failure-aggregation boundary is also open until the recovery choice is
bound: the future implementation may capture one structured failure per input
inside each loop or use an equivalent per-prepass aggregate wrapper, but it
must not let an IBL exception skip later independent types.

## Decisive questions and acceptance evidence

- Does the DataPacker complete-output contract require preserving the last
  complete IBL intermediate when a direct writer fails, or is explicit
  invalidation/removal allowed? Which authority owns that decision?
- For each candidate recovery mechanism, what exact sequence keeps the final
  bytes, `.meta`, and `.meta.dirty` truthful across a failed write, process
  termination, retry, and orphan reconciliation?
- Can a focused exporter scenario force a malformed/unreadable KTX or face,
  CMFT failure, and output-write failure after an existing complete output is
  present, then prove the selected recovery outcome without an incomplete
  replacement?
- Does the same scenario show one structured aggregate diagnostic for the IBL
  asset type, with one identifiable entry/context per failed IBL input, the
  next IBL pass and ordinary exports still executing, and the final result
  remaining failure while valid outputs keep their current bytes and
  fingerprints?
- Do clean-cache reads, producer-owned orphan removal, duplicate source
  handling, generated headers, and attribution retain their current behavior?

The eventual executable Plan must select one recovery mechanism, specify the
sidecar/dirty-marker transaction and failure aggregation boundary, name the
exact exporter functions, and bind the focused failure and later-type
acceptance evidence. Expected future work is Tier 2 while it remains within
the DataPacker prepass subsystem; re-evaluate the tier if the selected
publication policy crosses a separate output-owner or runtime contract.

## Provenance

- Frozen source candidate: `CPT/shard-0006/004`.
- Frozen consolidated index: `Temp/CppPlanTraceAudit/80896f33661aaab99cf180a96db54600099be652/consolidated-index.md`.
- Durable source evidence: `DataPacker/Source/ExportJobs/ExportCubemapIbl.cpp:105-161,239-323,366-373,378-528` and `DataPacker/Source/Main.cpp:626-632,705-725`.
- The route was reclassified after adversarial review because the prior Plan's
  preservation assumption was not supported by the frozen direct/truncating
  writers. No source, asset, or scheduler change is part of this
  investigation.
