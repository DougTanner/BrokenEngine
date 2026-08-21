# Agent Harness Command Reference

Engine-shared commands, identical for every game project. Each project's own command schemas — server, client, and client command behavior — live in `Projects/<Project>/Documents/AgentHarness.md` (default `Projects/BrokenEngineSandbox/Documents/AgentHarness.md`).

Every field below belongs under request `params`; every returned field belongs under response `result`. Sending a side-specific command to the other executable returns `unknown command`.

## Shared commands

- `ping`: no params. Returns `{"build":"server"|"client","tick":int}`; tick is `-1` before game creation.
- `quit`: no params. Requests clean shutdown; server autosaves. Returns `{}`.
- `get_logs`: `{"count"?:64,"pattern"?:"ECMAScript regex","category"?:"Default|Temp|Audio|Graphics|Loading|NavData|Network|Input|Replay"}`. Pattern scans the whole selected wrapping ring (512 cross-category lines when category is omitted; 128 lines for one category), then returns the last `count` matching lines chronologically as `{"lines":[...]}`. Matching is case-sensitive; use character classes rather than `(?i)`.
- `set_log_level`: `{"level":"Verbose|Debug|Info|Warning|Error","category"?:name}`. Levels below a compile-time floor clamp upward. Returns one effective level or a category map.
- `crash_report_fixture`: no params. Debug-only: a non-`kbDebugInput` build fails with `crash_report_fixture requires kbDebugInput build` whatever the params are. In a `kbDebugInput` build params must be exactly `{}` — anything else fails with `crash_report_fixture requires empty params`. Both failures are ordinary command failures that leave the process running. On success it deliberately never responds: the endpoint writes its crash report and exits, so AgentHarness reports `no response (timed out or peer closed)` and returns exit code 1 while the game process itself exits with code 0.
