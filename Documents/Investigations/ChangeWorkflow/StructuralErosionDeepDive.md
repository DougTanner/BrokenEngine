# Structural Erosion Deep Dive

What the structural-erosion line on the code-quality history chart measures, what drives its up and down periods, and whether controlling it deserves emphasis. Findings only; the recommendation waits for the user's decision.

Evidence base: the history table
`.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl`
read at `8b74870d` (rows 0-1168, 768 measured), and `/code-quality-metrics`
commit Snapshots (`Target Engine/Source`, `Scope Recursive`, `-Commit <sha>`,
the capture a history row stores per `HistoryContract.md`
`## Capture and drift`) run with the analyzer at `8b74870d`. The analyzer and
normalizer scripts last changed in `9c746fa0` (row 1139), so that analyzer
measures every row from 1139 on. Commit `1e9e821d` (**not reachable from
`main`**) re-measured rows 724-1138 (its first-parent positions 78-492) with it.

## Computation

- Function unit: a tree-sitter `function_definition` with a body. A nested definition is its own unit, and a lambda belongs to its enclosing unit (`MetricContract.md` `## Output`).
- Mass of a unit: `CC * sqrt(SLOC)`, built per function in `Analyze-CodeQualityMetrics.py` (the `funcs` rows).
- `structuralErosion = H / D`, where the numerator H is the summed mass of units with CC above 10 and the denominator D is the summed mass of all units (`metrics()`). `aggregate()` sums file numerators and denominators for areas. A row stores the whole supported corpus, not only `Engine/Source`.
- With `L = D - H`, the mass of units at CC 10 or less, `E = H / (H + L)`. Each corpus metric here splits a move into an H effect and an L effect, averaging the two orders of applying each change (a two-factor Shapley split, which sums exactly to the move). H is attributed per function from `highComplexityFunctions`. L is attributed per file from `files` rows (file denominator minus file numerator), because a Snapshot lists no per-function rows at CC 10 or less.
- Granularity: one function entering H moves E by about `mass * (1 - E) / D`. At the current `D ≈ 69,800` and `E ≈ 0.57`, a typical CC-11, 30-SLOC function crossing the threshold moves E by about +0.0004. The largest single function, mass 1,696, moved it by +0.011.

## Periods (rows 0-1168)

Segmentation rule: a zigzag over `structuralErosion`. A period ends at the running extreme once the series reverses by at least the threshold: 0.02 for the archive rows and 0.005 from row 646 on, where moves are smaller. Dates are committer dates.

### Archive rows 0-645: unattributable here

Archive SHAs resolve only in offline archives of the old repositories (`history/README.md`), and which analyzer measured these rows cannot be checked from this repository. These periods are characterized from table values and `supported`/`parsed` only, and are **not attributable here**.

| Rows | Dates | Erosion | Verbosity | supported / parsed |
| --- | --- | --- | --- | --- |
| 0-17 | 2024-10-02 .. 2025-06-02 | 0.6892 -> 0.6974 (+0.0082) | 0.0331 -> 0.0339 | 172/166 -> 172/166 |
| 17-121 | 2025-06-02 .. 2026-01-22 | 0.6974 -> 0.5983 (-0.0991) | 0.0339 -> 0.0600 | 172/166 -> 192/181 |
| 121-132 | 2026-01-22 .. 2026-01-31 | 0.5983 -> 0.6569 (+0.0586) | 0.0600 -> 0.0578 | 192/181 -> 204/193 |
| 132-156 | 2026-01-31 .. 2026-02-15 | 0.6569 -> 0.6211 (-0.0357) | 0.0578 -> 0.0591 | 204/193 -> 195/185 |
| 156-166 | 2026-02-15 .. 2026-02-19 | 0.6211 -> 0.6512 (+0.0301) | 0.0591 -> 0.0693 | 195/185 -> 200/189 |
| 166-178 | 2026-02-19 .. 2026-02-24 | 0.6512 -> 0.6221 (-0.0291) | 0.0693 -> 0.0787 | 200/189 -> 210/209 |
| 178-192 | 2026-02-24 .. 2026-03-02 | 0.6221 -> 0.6709 (+0.0488) | 0.0787 -> 0.0833 | 210/209 -> 226/225 |
| 192-211 | 2026-03-02 .. 2026-03-12 | 0.6709 -> 0.5493 (-0.1216) | 0.0833 -> 0.0856 | 226/225 -> 318/317 |
| 211-240 | 2026-03-12 .. 2026-03-18 | 0.5493 -> 0.5743 (+0.0250) | 0.0856 -> 0.0582 | 318/317 -> 325/324 |
| 240-242 | 2026-03-18 .. 2026-03-20 | 0.5743 -> 0.5108 (-0.0635) | 0.0582 -> 0.0453 | 325/324 -> 333/332 |
| 242-358 | 2026-03-20 .. 2026-05-16 | 0.5108 -> 0.6082 (+0.0974) | 0.0453 -> 0.0442 | 333/332 -> 419/418 |
| 358-404 | 2026-05-16 .. 2026-07-01 | 0.6082 -> 0.4658 (-0.1424) | 0.0442 -> 0.0379 | 419/418 -> 456/455 |
| 404-562 | 2026-07-01 .. 2026-07-29 | 0.4658 -> 0.5602 (+0.0944) | 0.0379 -> 0.0378 | 456/455 -> 507/507 |
| 562-645 | 2026-07-29 .. 2026-08-10 | 0.5602 -> 0.5579 (-0.0023) | 0.0378 -> 0.0384 | 507/507 -> 513/513 |

The series' extremes are archive rows: the maximum 0.6974 at row 17 (`e648aa64`) and the minimum 0.4658 at row 404 (`e10c809e`). The largest single steps are all archive rows (`supported` change in parentheses): 241 `a33c5e4f` -0.0608 (+8 files), 197 `e6aeef62` -0.0499 (+74), 195 `d96df900` -0.0423 (+1), 211 `25708b4f` -0.0378 (+9), 395 `cc8fcbc4` -0.0341 (0), 186 `d6dd9798` +0.0281 (+2). Rows 0-503 carry parse omissions (`parsed < supported` in 504 rows), and no row from 504 on does.

### Rows 646-1168

| Period | Rows | Dates | Erosion (table) | Verbosity (table) | Measured rows |
| --- | --- | --- | --- | --- | --- |
| P1 | 646-650 | 2026-08-10 .. 08-11 | 0.5579 -> 0.5590 (+0.0011) | 0.0384 -> 0.0383 | 1 |
| P2 | 650-651 | 2026-08-11 .. 08-12 | 0.5590 -> 0.5451 (-0.0139) | 0.0383 -> 0.0381 | 1 |
| P3 | 651-677 | 2026-08-12 .. 08-15 | 0.5451 -> 0.5516 (+0.0066) | 0.0381 -> 0.0382 | 14 |
| P4 | 677-724 | 2026-08-15 .. 08-20 | 0.5516 -> 0.5254 (-0.0262) | 0.0382 -> 0.0379 | 17 |
| P5 | 724-1135 | 2026-08-20 .. 10-05 | 0.5254 -> 0.5790 (+0.0535) | 0.0379 -> 0.0364 | 186 |
| P6 | 1135-1160 | 2026-10-05 .. 10-09 | 0.5790 -> 0.5677 (-0.0112) | 0.0364 -> 0.0356 | 16 |
| P7 | 1160-1168 | 2026-10-09 .. 10-12 | 0.5677 -> 0.5677 (0) | 0.0356 -> 0.0356 | 1 |

## Attribution from row 646 on

Every SHA in this section is reachable from `main` unless it is marked **not reachable from `main`**. Rows 1141-1159 name commits since squashed into `3c3cfce5` (row 1160). The objects for those commits still exist in this repository, so their Snapshots reproduce only while the objects survive. Every period endpoint below is reachable from `main`. In the tables:

- "H / L effect" is the share of the period's erosion move.
- A function's share is its mass change as a share of the period's net H change (dH). Churn makes individual shares exceed 100% in sum.
- The agent group means paths under `Engine/Source/Agent/`, `Projects/BrokenEngineSandbox/Source/Agent/`, and `Tools/AgentHarness/`. It includes the harness fixture commands, renamed to harness rigs in P6.

| Period | dE | H effect | L effect | dH | dL | Main drivers (share of dH) |
| --- | --- | --- | --- | --- | --- | --- |
| P1 646-650 | +0.0011 | 96% | 4% | +145 | -4 | `DataPacker/Source/Main.cpp::PublishManifestAndPack` entered at CC 12 (50%); `AgentCommands.cpp::MissileSharedRowMatches` entered at CC 14 (37%) |
| P2 650-651 | -0.0139 | 59% | 41% | -1,131 | +636 | One commit, `2c607ef8` (52 C++ files). `DataPacker/Source/Main.cpp::RunExportJobs` (CC 58, mass 800) left H while its name stays in the file and the file's L rose by 379 (71%); `Attribution.cpp::CopyThirdPartyLicenses` (CC 27) left (20%); `Main.cpp::main` (CC 17) left (11%) |
| P3 651-677 | +0.0066 | 81% | 19% | +729 | -140 | Spread: 17 changed functions net +435, and 6 existing functions crossed above CC 10 (+466), e.g. `DeviceManager.cpp::LoadPipelineCache` to CC 18 (19%) and `ParseLaunchOptions` CC 29 -> 35 (13%); `TweaksScreenParticles.cpp::RenderParticlesSection` left (-23%) |
| P4 677-724 | -0.0262 | 57% | 43% | -2,012 | +1,285 | A decomposition campaign. Table steps: 681 `ea1855d7` "Decompose ProfileManager game screen formatting" -0.0067, 686 `712ce395` "Decompose DataPacker export jobs" -0.0064, 680 `279c5f97` "Decompose camera update stages" -0.0041, 688 `9f647fea` "Decompose DataPacker FileManager worktree handling" -0.0037; these four make 80% of the period. Functions: `ProfileManager.cpp::FormatGameScreens` CC 43 left (30%), `PlayersNavigation.cpp::ComputeNavigation` CC 58 -> 18 (28%), `AudioRepair.cpp::DeclipChannel` left (17%), `Camera.cpp::Update` left (17%) |
| P5 724-1135 | +0.0535 | 118% | -18% | +9,422 | +1,169 | Growth, mostly harness fixture commands: agent group +5,305 (56%), other runtime +2,283 (24%), DataPacker +1,232 (13%), other tools +602 (6%). Sub-periods follow |
| P6 1135-1160 | -0.0112 | 112% | -12% | -2,098 | -171 | Over-engineering removal ending in squash `3c3cfce5` "Opus 5.5 over-engineering pass". 45 functions kept path and name and lost CC (-973, 46%); the 12 fixture commands renamed to harness rigs shrank (-502, 24%), e.g. `CommandAudioStreamingFixture` CC 105 -> `CommandAudioStreamingHarnessRig` CC 92 (-277). Largest table steps: 1141 `59d1a0fb` -0.0033, 1143 `45a7d5b6` -0.0028, 1153 `998c1b3c` -0.0028, 1151 `9aac806a` -0.0023 — all **not reachable from `main`** |
| P7 1160-1168 | 0 | - | - | 0 | 0 | Row 1164 `89640522` is the only measured row and repeats row 1160's values |

P5 sub-periods:

| Rows | dE | H / L effect | Main drivers (share of dH) |
| --- | --- | --- | --- |
| 724-899 | +0.0146 | 117% / -17% | `AgentCommands.cpp::CommandRegistryFixture` new at CC 45, 287 SLOC (33%); `BakeRoute.cpp::LoadMesherMesh` CC 14 -> 35 (10%); `AgentScene.cpp::CommandDescribeScene` CC 18 -> 32 (9%); agent group 57% |
| 899-900 | +0.0078 | 108% / -8% | One commit, `d3d2f6c9` "Preserve harness network state across replay commands": `CommandClientCancelledSubscriptionFixture` CC 34 (29%) and `CommandServerPreHandshakeAckFixture` CC 32 (27%) new; agent group 65% |
| 900-940 | +0.0073 | 114% / -14% | glTF validation commits (e.g. 935 `227c1f3c`, 934 `4890edcd`): `SceneSkeletonLoader.cpp::LoadSkeletonData` CC 13 -> 34 (20%), `SceneAnimationLoader.cpp::LoadAnimations` crossed to CC 25 (16%), `PackChunks.cpp::LoadPackFiles` CC 38 -> 48 (12%); DataPacker 60% |
| 940-941 | +0.0154 | 139% / -39% | One commit, `1a288ca9` (54 C++ files, `supported` 529 -> 543): `AgentCommandsAudioStreaming.cpp::CommandAudioStreamingFixture` new at CC 105, 261 SLOC, mass 1,696 (50%; alone about +0.011 of the +0.0154); agent group 80% |
| 941-1077 | +0.0010 | H +0.0039 / L -0.0030 | Offsetting churn: H +656 against L +373 |
| 1077-1078 | +0.0043 | 54% / 46% | One commit, `42c379cb` "C++ and code style" (506 files): mostly renames (net about 0). Four functions crossed to exactly CC 11 (`ValidateCachedChunkBody`, `ProcessUpdatePlayerRequests`, `TrackClientTransfers`, `OnPlayerSpawned`, +227 mass), and L fell 248 across 98 files. Above a CC-15 threshold this step is only +0.0007 |
| 1078-1135 | +0.0031 | 52% / 48% | `ServerSimulationFixtures.cpp::CommandInjectPayload` new at CC 56 (+641) replaced `CommandReplayTransferFixture` (-345) and `CommandServerPreHandshakeAckFixture` (-335) |

## Root-cause verdicts

- **(a) Threshold crossing — minor contributor, not the cause of the periods.**
  - One crossing moves E by about +0.0004.
  - In P5, functions entering H at CC 11-12 whose name already existed in the file (candidate crossings) sum to +679 of +9,422 (7%).
  - Crossings explain about half of the H rise in the 1077-1078 step, which nearly vanishes above a CC-15 threshold.
  - Every period of at least 0.005 keeps its sign under thresholds of 15 and 20 and under two threshold-free weightings (see `## Sensitivity`).
- **(b) Denominator dilution — secondary, mostly a by-product of decomposition.**
  - The L effect has the same sign as the move in P1 (4%), P2 (41%), P3 (19%), and P4 (43%), and is large only in P2 and P4. There it is mass moving from a decomposed complex function into its new simple helpers in the same file (P2: `Main.cpp` L +379 as `RunExportJobs` left), not independent new code.
  - In the growth periods, new low-CC code damped the rise: P5 L effect -18%, and -39% at row 941.
- **(c) Corpus membership — not an artifact from row 646 on; archive unknown.**
  - From row 646 on, `parsed == supported` in every row, so no parse omission moves the value.
  - Files enter and leave (513 -> 557), but they carry real functions. Engine/game moves pair as near-equal added and removed file L (P4: `NetworkSerialization.cpp` +216 / -212).
  - In the archive, parse omissions in rows 0-503 and jumps such as row 197 (+74 files, -0.0499) cannot be separated here.
- **(d) Analyzer or measurement differences — ruled out from row 646 on; archive unknown.**
  - Fresh Snapshots reproduce the stored row's `structuralErosion` and `verbosity` to all 12 digits at 13 of 14 checked rows.
  - The exception is the root row 646, where the fresh value is 0.557880533969 against 0.557882659401 stored (-2.1e-6), with verbosity 0.03843218075 against 0.038432914146. That is noise-level and does not affect any period.
  - Which analyzer measured the archive rows cannot be checked.
- **(e) Genuine growth or decomposition of complex functions — confirmed main driver.**
  - The H effect is at least 57% of every period's move and over 100% in P5 and P6.
  - The down periods are decomposition or simplification work: P2, P4's "Decompose ..." commits, and P6's over-engineering pass.
  - The up periods are new large branchy functions, mostly harness fixture commands, which take 56% of P5's dH.
  - The series is lumpy because the top 10 functions hold 11% of all mass (19% of H) at row 1164, and the largest alone holds 2%.

## Verbosity comparison

Computed from the table over measured rows:

| Rows 646-1168 | Erosion | Verbosity |
| --- | --- | --- |
| Range | 0.5254-0.5790 | 0.0355-0.0384 |
| Range / mean | 0.096 | 0.079 |
| Net move / summed absolute steps | 0.063 | 0.19 |
| Top-5 steps' share of summed absolute steps | 32% | 25% |

- Erosion and verbosity steps barely correlate: 0.11 from row 646 on, 0.21 in the archive.
- Relative to its own level, erosion moves about as much as verbosity. It reads as random because it is two-directional: decomposition campaigns and harness growth alternate, so only 6% of its movement is net, against 19% for verbosity's steady decline. It is also lumpier, since the largest single functions each carry about 1-2% of all mass.
- The chart amplifies this. Each series' axis spans its full-history range: erosion 0.4658-0.6974, verbosity 0.0321-0.0856 (the archive's verbosity peak). The row 646-1168 erosion range therefore fills 23% of its full-history data range, while verbosity fills 5.5%.

## Sensitivity

Scratch computations over the Snapshots in `## Reproduction`. Every variant keeps D and changes only the numerator over the listed functions.

| Steps | CC > 10 (current) | CC > 15 | CC > 20 | Ramp, 0 at CC 10 to full mass at CC 20 | `(CC - 10) * sqrt(SLOC)` |
| --- | --- | --- | --- | --- | --- |
| 650 -> 651 | -0.0139 | -0.0142 | -0.0129 | -0.0132 | -0.0104 |
| 677 -> 724 | -0.0262 | -0.0410 | -0.0370 | -0.0345 | -0.0239 |
| 724 -> 899 | +0.0146 | +0.0192 | +0.0245 | +0.0198 | +0.0144 |
| 940 -> 941 | +0.0154 | +0.0190 | +0.0200 | +0.0176 | +0.0170 |
| 1077 -> 1078 | +0.0043 | +0.0007 | +0.0005 | +0.0018 | +0.0015 |
| 1135 -> 1160 | -0.0112 | -0.0040 | -0.0034 | -0.0082 | -0.0086 |
| Range / mean over the 13 points from 646 | 0.096 | 0.156 | 0.216 | 0.138 | 0.173 |

- The period shape is not a CC-10 artifact.
- A different threshold or weighting amplifies the swings rather than smoothing them, so replacing the formula would not make the line calmer.

## Recommendation

**Keep structural erosion advisory as it is now. Do not add emphasis on controlling the corpus ratio.**

- It measures something real: every period traces to named decomposition work or to new large functions.
- But the ratio is a coarse, lumpy summary of a few dozen functions, and one new harness command moved it by 0.011. Driving the ratio down invites the denominator and helper-proliferation gaming `Remediation.md` already forbids, and gives no guidance a reviewer cannot get directly from the function list.
- The useful signal is the per-function mass ranking (`highComplexityFunctions`), which already names what to decompose. P4 shows that targeted decomposition moves the line when the work is wanted.

Follow-on options for the user's decision. Option 1 is recommended; none is implemented.

1. No change: keep the metric, chart, and guidance as they are.
2. Chart readability: draw both series on a shared relative scale, or scale the axes to rows 646 on, so equal relative movement looks equal. This changes the history SVG generator and its contract.
3. Function-level advisory: report the largest new or grown `highComplexityFunctions` entries by mass at review or landing, instead of reading the ratio.
4. Area split: report the agent group separately. It holds 26% of H at row 1164 and drove 56% of P5's rise, so it masks runtime code trends.
5. Formula replacement (CC 15/20 threshold or a ramp): not recommended; `## Sensitivity` shows the same periods with larger swings.

## Reproduction

Run `/code-quality-metrics` Snapshot with `Target Engine/Source`, `Scope Recursive`, and `-Commit <sha>`, using the analyzer at `8b74870d`. Read `current.corpusMetrics.structuralErosion` and `current.highComplexityFunctions`.

| Row | Commit | Value | Numerator (H) | Denominator (D) | supported = parsed |
| --- | --- | --- | --- | --- | --- |
| 646 | `e571f6f11f62e5cb7f9f57c2705a0a38e3391d40` | 0.557880533969 | 34567.987 | 61963.064 | 513 |
| 650 | `b4556e529f7a0795cdc6d0e8202747220287377f` | 0.558952750958 | 34713.132 | 62103.876 | 513 |
| 651 | `2c607ef898c8e0114863113c28cad06a9038a12a` | 0.545082088602 | 33582.127 | 61609.302 | 513 |
| 677 | `a92acd1fd1bd3fb2c6eb41055de4f92e859640fd` | 0.551639083832 | 34311.474 | 62199.135 | 511 |
| 724 | `48cebb3f42bde7b9c0bdd00dff5541520011b578` | 0.525436041242 | 32299.495 | 61471.792 | 525 |
| 899 | `274f5266e4ff215370862cdf069f49064e323870` | 0.540064449731 | 34594.794 | 64056.788 | 529 |
| 900 | `d3d2f6c952e6681b78d13e370e666fcf0a4766ab` | 0.547890413070 | 35797.901 | 65337.703 | 529 |
| 940 | `b3a955b68085d8d3ff0cb8e48340d2f6d6485a59` | 0.555214780705 | 37032.331 | 66699.109 | 529 |
| 941 | `1a288ca944bd1b791de9208c851312afa9848c95` | 0.570565272513 | 40394.770 | 70797.806 | 543 |
| 1077 | `49809ee1db3e59b6b8f3a8226c1535f17db3bd50` | 0.571523533580 | 41050.381 | 71826.231 | 557 |
| 1078 | `42c379cba316be4fe7b203a6235ff0662b21e588` | 0.575848773312 | 41446.666 | 71974.914 | 556 |
| 1135 | `a3c0d438df1f02960c00624f7eda7e3a1e02f965` | 0.578958509396 | 41721.604 | 72063.203 | 555 |
| 1160 | `3c3cfce55b73ef882aa99f3eebe8bc218c04164d` | 0.567714808613 | 39623.319 | 69794.409 | 557 |
| 1164 | `896405224055125ed9f0a5bb94e944753c3ce925` | 0.567714808613 | 39623.319 | 69794.409 | 557 |

All 14 commits are reachable from `main`. Table-only citations of rows 1141, 1143, 1151, and 1153 name commits **not reachable from `main`**: `59d1a0fb9176547a34af1c06f0c2ee766702a9d2`, `45a7d5b61425cd06895b72c87d18ebedc1f824db`, `9aac806a6a2eb1a14391cdb55370c0e99027c000`, and `998c1b3c62da1bf6133e248c4ac52b269e684d2f`.
