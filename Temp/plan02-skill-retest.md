# Plan02 agent-harness focused skill retest

## Result

Status: PASS
Findings: none
Changed files: Temp/plan02-skill-retest.md — durable review evidence only; no target edits.
Decisive checks: pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/external-skill-creator — exit 0; complete stdout: VALID .agents/skills/external-skill-creator/SKILL.md
Decisive checks: pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/agent-harness — exit 0; complete stdout: VALID .agents/skills/agent-harness/SKILL.md
Decisive checks: pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Find-SkillInboundReferences.ps1 -SkillName agent-harness — exit 0; status pass/code ok; 76 hits across seven roots; truncated false.
Decisive checks: git diff b01f2286e10144f36a8031f4daad51c060015151 -- .agents/skills/agent-harness — only correction changes retained-area list from shadow/lighting/smoke to shadow/smoke.
Decisive checks: command-reference.md:269-286 versus PresentationContinuityProbe.cpp:13-51 — semantic PASS, static source evidence; top-level publishedFrames, basisCoord, water, shadow and smoke match, including all documented nested keys and array sizes; lighting is absent from both.
Decisive checks: SKILL.md and references/worker.md — semantic PASS for focused correction; read-only schema inspection, empty-parameter contract and command routing remain coherent; static semantic evidence.
Decisive checks: SmokeUniforms.cpp:19-27 — semantic PASS; plane-zero publication supports the adjacent smoke description; static source evidence.
Decisive checks: seven-root inbound sweep — PASS; all references classified under Inbound classification below; existing run-as-evidence paragraph covers inline worker execution and invocation controls do not prevent required routing.
Decisive checks: trigger contrast and focused host checks — not triggered; description, When-to-use surface and invocation policy unchanged; no agents/openai.yaml exists; client loader/runtime behavior remains unverified and is not claimed.
Decisive checks: git diff --check -- .agents/skills/agent-harness — exit 0; no whitespace errors.
Build required: none
Evidence: Temp/plan02-skill-retest.md ## Inbound classification — classification scope and current sweep result.
Evidence: Temp/plan02-adversarial.md ## ADV002 evidence and refutation — original defect hypothesis independently checked against current source.
Evidence: .agents/skills/agent-harness/references/command-reference.md ## Client commands — corrected returned-object schema.
Residuals: none

## Inbound classification

Fresh sweep output: schemaVersion broken-engine-skill-inbound-references/v1; status pass; code ok; totalHits 76; recordLimit 150; truncated
false. Root counts: AGENTS.md 7; .agents/skills/ 36; .agents/references/ 2; Documents/ 31; .claude/agents/ 0; .codex/agents/ 0; .opencode/
0. All returned records were inspected. The full matching record list is also retained in Temp/plan02-skill-validation.md ## agent-harness
inbound sweep; that prior output is supporting inventory, not proof of this retest's semantic verdict.

Workflow requirements: root and subsystem AGENTS routing; change-workflow build/scenario routing; add-collection, compile, create-follow-up-plans, external-diagnose-bug and next-plan workflow references; feature, investigation, plan and profiling instructions requiring live runs, screenshots or captures. These require the skill when their conditions apply. Plan-audit and plan-simplicity-review prohibitions exclude live work in those roles and do not disable the skill elsewhere.

References and examples: execution-card and verify-acceptance command-reference links; package metadata/self-reference; bundled script invocations and explanatory script comments; external-diagnose-bug license history; external-skill-creator interpreter reference; feature acceptance examples, plan exclusions and historical script paths. Profiling's GamePid placeholder is an example, not a separate invocation policy.

Run-as-evidence verdict: PASS. SKILL.md ## Handoff requires every nested run's full retained handoff, Status and Criterion results, plus resulting working-tree summary. This covers the applicable inline worker evidence contract. No disabled invocation metadata exists that conflicts with inbound workflow requirements.

## Scope and limits

Worktree: C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad. Baseline:
b56f20fa44d0424c2fdf5003157563684ed747f5. Round origin: b01f2286e10144f36a8031f4daad51c060015151. Focus: ADV002 correction only; previous
semantic package verdicts were treated as hypotheses, not substituted for current producer inspection. No unrelated package review, builds,
runtime/harness commands, source edits, commits, branch switches or delegation occurred. Plan03 retains runtime acceptance.
