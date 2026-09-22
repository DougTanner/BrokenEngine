# External Workflow Skills Audit

## Scope and conclusion

Twenty supplied workflow projects were independently evaluated against Broken Engine at baseline `ab7d62357801233fd32e80a0af2c336ecf89aad5`. Sixteen sources were available and inspected at pinned commits; 10, 13, 15, and 17 returned HTTP 404 and remain unavailable, without any judgment about their quality or identity. No external package was installed or executed, and no licensed source is copied.

The local loop already has the useful common structure: explicit intent and scope, bounded delegation, evidence-backed review, correction, acceptance, and authorized landing. Most candidates duplicate one of those owners or add a parallel scheduler, memory store, score, dashboard, or approval mechanism. Four narrow ideas survive because they strengthen an existing owner without adding a mandatory gate: contrast cases for changed skill triggers, capability-aware worker waits, an advisory direct-file boundary warning, and opt-in observed transcript usage. The changed-trigger contrast and capability-aware waiting adaptations were executed in this session; the other two have Plans linked below.

## Numbered decisions

| # | Pinned source | Verdict | Reason / accepted Plan |
| --- | --- | --- | --- |
| 01 | [frontend-design-pro `79b3820`](https://github.com/davepoon/buildwithclaude/tree/79b3820d2f5891ce1c9e01b25fa8bcd3f903e47c/plugins/frontend-design-pro) | Reject | Web art direction and HTML review duplicate the game UI contract and harness evidence. |
| 02 | [animated-sketch-diagram `cfc0061`](https://github.com/OLDyade/animated-sketch-diagram/tree/cfc00615f600c28318a85f88505113c5b9a8b399) | Reject | Optional animation style; Mermaid already carries workflow topology without a browser/GIF pipeline. |
| 03 | [Archcore `87a1b5d`](https://github.com/archcore-ai/cli/tree/87a1b5d3f78a89b77ca76a3460ac6edabcf019e5) | Reject | Retrieval, dependency hints, and summaries overlap bounded reads and evidence handoffs. |
| 04 | [Shipwright `762326a`](https://github.com/Wynelson94/shipwright/tree/762326a342fdfaca2d016f9acce39070dcae39d0) | Reject | Web deployment wrapper; grades cannot replace invariant-level acceptance. |
| 05 | [slopmop catalog `79b3820`](https://github.com/davepoon/buildwithclaude/tree/79b3820d2f5891ce1c9e01b25fa8bcd3f903e47c/plugins/slopmop), [upstream `0dae507`](https://github.com/ScienceIsNeato/slop-mop/tree/0dae507e76a45134b3b72cb2be42339a3d435ddc) | Reject | No proven weakened validation or costly repeated check supports another controller or cache. |
| 06 | [Wenlan `bb2f468`](https://github.com/7xuanlu/origin/tree/bb2f468ba14d5ac24f009940fa9c4318cbf9bd17) | Reject | Persistent briefs and recall would duplicate Plans, claims, execution cards, and continuation capsules. |
| 07 | [dsh-deepread `9462f1e`](https://github.com/xiehuan123/dsh-deepread/tree/9462f1e546b94a7f27e77abaf3d9ee3fedf7f746) | Reject | Source/inference discipline exists; lossy study summaries cannot prove Plan completeness. |
| 08 | [obsidian-skills `3ccff53`](https://github.com/kepano/obsidian-skills/tree/3ccff5338ea700537839b21900aa5358a0402c98) | Reject | Personal knowledge workflows do not expose a missing Change Workflow mechanism. |
| 09 | [claude-md-kit `3407dca`](https://github.com/fablerlabs/claude-md-templates/tree/3407dcae4fa7e45b67512b749eed999fd0e6ce88) | Reject | Its audits, migrations, and scorecards duplicate stronger repository-specific owners. |
| 10 | [Lifecycle-Inno/claude-ops](https://github.com/Lifecycle-Inno/claude-ops) | Unavailable | HTTP 404; no commit, license, or source-derived proposal. |
| 11 | [crosstalk `0fefe87`](https://github.com/d7omdev/crosstalk/tree/0fefe87652b1c8e88f513d2ae77ab556df6d8d1b) | Reject | Messaging topology and deviation receipts overlap structured briefs, handoffs, and conflict reporting. |
| 12 | [claude-wayfinder `e906955`](https://github.com/glitchwerks/claude-wayfinder/tree/e906955a5bf962247a87d755ae63495538ce36b5) | Accept narrow adaptation | Positive and near-miss cases now run only when trigger surfaces change; this adaptation was executed in the session in [semantic review](../../../.agents/skills/external-skill-creator/references/semantic-review.md). |
| 13 | [nicolai-bernse/backlogd](https://github.com/nicolai-bernse/backlogd) | Unavailable | HTTP 404; no implementation evidence or responsible proposal. |
| 14 | [magic-cc-codex-worker `f0e4796`](https://github.com/wenqingyu/magic-cc-codex-worker/tree/f0e4796f772108d894281a757d2cdc1e9bbb0ea3) | Accept narrow adaptation | Native capability-aware waits are documented while the scheduler and worker registry remain unchanged; this adaptation was executed in the session in [worker reporting](../../../.agents/references/subagent-reporting.md#whether-a-worker-is-still-running-and-interruption). |
| 15 | [explorium-ai/vibe-prospecting](https://github.com/explorium-ai/vibe-prospecting) | Unavailable | HTTP 404; a similarly named product was not substituted. |
| 16 | [project-boundary `07c5250`](https://github.com/justi/claude-code-project-boundary/tree/07c5250ec55434c97d3fed48ed29da193c7663a5) | Accept narrowed adaptation | Future Tier-2 `Edit|Write` advisory reuses canonical path helpers; it never blocks or emits a permission decision: [Plan](../../Plans/ChangeWorkflow/WarnClaudeDirectFileDestinationsOutsideWorktree.md). |
| 17 | [TLS-Radar/tlsradar](https://github.com/TLS-Radar/tlsradar) | Unavailable | HTTP 404; private, renamed, removed, or mistaken identity remains possible. |
| 18 | [claude-hud `79b3820`](https://github.com/davepoon/buildwithclaude/tree/79b3820d2f5891ce1c9e01b25fa8bcd3f903e47c/plugins/claude-hud) | Reject | Host status may already cover passive visibility; transcript reconstruction adds no acceptance evidence. |
| 19 | [claude-pager `6e6f7c2`](https://github.com/bharat7gupta/claude-pager/tree/6e6f7c26e1c06a081059612a0c2185d0021ea197) | Reject | Native `preferredNotifChannel` already controls delivery. Official hooks documentation says `idle_prompt` fires about 60 seconds after any completed response, not specifically for required input; a custom Windows helper has no proven missing benefit. [Notification contract](https://code.claude.com/docs/en/hooks#notification). |
| 20 | [claude-prospector `2e03a07`](https://github.com/glitchwerks/claude-prospector/tree/2e03a07e5fa997c6f1f684b9f571005e58ee7d15) | Accept narrow adaptation | Add only an explicitly requested, provenance-bounded observed-usage summary; no billing, dashboard, score, or mandatory gate: [Plan](../../Plans/ChangeWorkflow/AddOptionalObservedTranscriptUsageSummary.md). |

## Durable boundaries

Future adaptations remain original repository work. They preserve current approval, claim, review, and landing authority; add no unit tests or mandatory stage; and treat external mechanisms as inspiration rather than imported evidence. Observed transcript usage, main-context size, elapsed time, and billed cost remain separately named quantities. Host runtime behavior that has not been exercised remains an implementation acceptance check, not a conclusion of this audit.
