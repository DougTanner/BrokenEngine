---
name: gaea2-diagnose
description: >-
  Diagnose why Gaea 2 rejected a .terrain file or failed a build. Use when the
  user reports an error like "File is corrupt or missing additional data",
  "Swarm failed to load", a node loading as deactivated (dashed border), or
  unexpected behavior after a /gaea2-save round-trip. Finds the latest Gaea 2
  session log (under %APPDATA%\QuadSpinner\Gaea\2.0\Logs — NOT Gaea 1's
  location), decodes its base64+gzip ERR payloads to extract the underlying
  Newtonsoft exception with JSON path, then cross-references the shipping
  Examples library to suggest the valid value or shape.
argument-hint: [--latest-swarm | --blob <base64> | --log <path>]
allowed-tools: [Read, Edit, PowerShell, Glob, Grep]
disable-model-invocation: true
---

# gaea2-diagnose

## Purpose

The Gaea 2 UI reports failures with a vague modal ("File is corrupt or missing
additional data"); the real Newtonsoft exception is in a log. Find the right
log, decode the exception, cross-reference the shipping samples for the valid
value, and tell the user what to change.

## When to use

- The user reports an error like "File is corrupt or missing additional data" or
  "Swarm failed to load".
- A node loads as deactivated (dashed border).
- Behavior is unexpected after a `/gaea2-save` round-trip.
- This skill diagnoses without editing `.terrain`: property fixes follow step 6
  below; for structural graph changes, direct the user to run `/gaea2-modify`.
- Runtime crashes in the Gaea binary itself go to QuadSpinner support, not this
  skill.

## Steps

1. Set up Python dispatch via `Invoke-Gaea2Python.ps1` per
   [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#python-wrapper-dispatch)
   and
   [wrapper-contract.md](../gaea2-shared/references/wrapper-contract.md#no-inline-reconstruction).
   Done when every later script call uses that dispatch form.

2. Pick the log stream by how Gaea was invoked. Done when one log stream and its
   reader are chosen.

   | Source of error | Log | How to read |
   |---|---|---|
   | File-open / file-save failure (UI) | `%APPDATA%\QuadSpinner\Gaea\2.0\Logs\YYYY-MM-DD_HH-MM-SS.txt` | `decode_gaea_err.py --latest` |
   | Build / "Swarm" failure inside the UI | same dir: `*-SWARM.txt`, `CRASH_SWARM_*.txt` | `decode_gaea_err.py --latest-swarm` |
   | DataPacker invoking headless Swarm | same dir: `*-SWARM.txt` — headless `Gaea.Swarm.exe` logs to the same QuadSpinner location as UI builds. DataPacker launches it in a new console and discards its output, so there is no DataPacker-side log | `decode_gaea_err.py --latest-swarm` |
   | Caller pasted a specific log path | that path | `decode_gaea_err.py --log <path>` |
   | Caller pasted a base64 `ERR` payload | inline | `decode_gaea_err.py --blob <text>` |

3. Decode the log. For the QuadSpinner log streams, dispatch the decoder through
   the step 1 wrapper. If a node loads dashed/deactivated with no `ERR` line in
   the log, skip to `../gaea2-shared/references/node-conventions.md` (see
   `### Common error patterns` below). Done when one decoded `ERR` is in hand,
   or the no-`ERR` dashed-node case has been routed to `node-conventions.md`.

   ```powershell
   pwsh -NoProfile -NonInteractive -Command "& '.agents/skills/gaea2-shared/scripts/Invoke-Gaea2Python.ps1' -Script decode_gaea_err.py -Arguments '--latest'"
   ```

   - The decoder's output arrives in the envelope's `stdout` field: each ERR
     with its timestamp, trimmed to the exception line by default.
   - Add `'--full'` to `-Arguments` only when the stack trace would actually
     help — Newtonsoft stack traces are noisy and the first two lines almost
     always name the problem.
   - If `--latest` reports no ERR lines, the error may have come from an earlier
     session — list `*.txt` in the log dir by mtime and try the previous one.
   - A DataPacker-invoked failure is a Swarm failure like any other: decode it
     with `--latest-swarm`.

4. Parse the exception. The decoded first line follows a predictable pattern.
   Done when all three of the extracted items below are in hand.

   ```
   Newtonsoft.Json.JsonSerializationException:
     Error converting value "<VALUE>" to type 'QuadSpinner.Gaea.Nodes.<EnumType>'.
     Path 'Assets.$values[0].Terrain.Nodes.<ID>.<Property>', line N, position M.
    ---> System.ArgumentException: Requested value '<VALUE>' was not found.
   ```

   Extract three things — together they tell you which sample query to run:

   - JSON path — names the node Id and the offending property
   - Type — the .NET enum/class that rejected the value
   - Value — what was written that Gaea didn't accept

5. Find the valid value via shipping samples. Use `inspect_samples.py`,
   dispatched through the same wrapper, to ask three kinds of questions (its
   answer is in the envelope's `stdout`). Done when the valid value or shape is
   known.

   ```powershell
   # What values does this enum actually accept?
   pwsh -NoProfile -NonInteractive -Command "& '.agents/skills/gaea2-shared/scripts/Invoke-Gaea2Python.ps1' -Script inspect_samples.py -Arguments '--enum','SatMap.Library'"

   # What does a working node of this type look like?
   pwsh -NoProfile -NonInteractive -Command "& '.agents/skills/gaea2-shared/scripts/Invoke-Gaea2Python.ps1' -Script inspect_samples.py -Arguments '--type','Erosion2'"

   # What ports does this node have? (for wiring questions)
   pwsh -NoProfile -NonInteractive -Command "& '.agents/skills/gaea2-shared/scripts/Invoke-Gaea2Python.ps1' -Script inspect_samples.py -Arguments '--ports','Combine'"
   ```

   Other useful queries when the error is less specific — same command,
   different `-Arguments`:

   ```
   # Where does this property name appear and with what values?
   -Arguments '--field-grep','Library'

   # What node types exist at all?
   -Arguments '--list-types'
   ```

6. Apply the fix and hand off to save. Once you know the valid value, edit
   `Temp/<basename>.md`, then ask the user to run `/gaea2-save` and retry the
   load in Gaea 2. If a second error appears, repeat from step 3 — Gaea fails
   fast on the first error, so subsequent issues only surface after the first is
   fixed. Done when the load succeeds in Gaea 2, or a second error has sent you
   back to step 3.

7. Promote the finding. If the discovered constraint is durable (a stable enum
   value, a required field, a node-deactivation rule), add a one-liner to
   `../gaea2-shared/references/node-conventions.md` under the matching section —
   that's where future invocations look first, so a fix that escapes into docs
   avoids the next round-trip. Done when a durable constraint has been recorded
   there, or the constraint is not durable.

## Rules

- Suspected Gaea-version drift: dispatch the bundled `check_gaea_version.py`
  through the step 1 wrapper (`-Script check_gaea_version.py`, no arguments);
  `/gaea2-load` also runs it on every load.
- Use the bundled
  [`decode_gaea_err.py`](../gaea2-shared/scripts/decode_gaea_err.py) for `ERR`
  payloads; its module documentation owns the payload format and truncation
  handling. Never hand-roll a decoder inline.
- The shipping examples under `C:\Program Files\QuadSpinner\Gaea 2\Examples\`
  are the reliable reference. Trust them over UI labels and over documentation —
  the Gaea UI library browser shows "Sandy", "Rocky", "Colorful", but the JSON
  enum the serializer expects is `Sand` / (absent for Rocky) / `Color`.
  Discrepancies like this recur.

### Confusable locations

- `%APPDATA%\QuadSpinner\Gaea\Logs\` (without `2.0\`) is Gaea 1's log path.
  Different format, irrelevant to Gaea 2 errors.
- If `2.0\Logs\` does not exist, Gaea 2 was never opened on this machine; if
  `%LOCALAPPDATA%\BrokenEngine\DataPackerCache\<Project>\` does not exist, no
  build has been attempted via the DataPacker pipeline.

### Common error patterns

UI / compressed-log errors (decoded form):

| Decoded message starts with… | Likely cause | Fix |
|---|---|---|
| `JsonSerializationException: Error converting value "X" to type 'Enum...'` | Invalid enum value (often UI-label vs JSON-name mismatch) | `--enum Type.Property` then write the JSON name |
| `JsonSerializationException: ... missing required member 'X'` | Required field absent | `--type <NodeType>` and copy the field from a sample |
| `JsonReaderException: Unexpected end of content` or "File is corrupt or missing additional data" (no decoded error) | Root-level field missing (e.g., `Macros`) | Compare root keys with a known-good sample |
| No ERR line at all, but a node loads dashed/deactivated | Id < 100, missing `Version: 2`, required input port not wired, or graph-rejected enum | See `../gaea2-shared/references/node-conventions.md` |
| ERR mentions `CompositeFailure` / a Swarm log | Build-time evaluation error, not a load error | Re-run with `--latest-swarm` |

Headless / DataPacker-invoked Swarm errors (from the same `*-SWARM.txt` logs;
the exit code comes from DataPacker's own bake failure):

| Message | Likely cause | What this means |
|---|---|---|
| `IOException: The handle is invalid` immediately after `Opening <name>.terrain...` | `Gaea.Swarm.exe` console-handle setup fails when its stdio is a pipe or file. Not a .terrain content issue. DataPacker already launches it with `CREATE_NEW_CONSOLE` for exactly this reason. | Test the .terrain by opening it in the Gaea UI to confirm the file itself is intact. If the UI loads it but Swarm via DataPacker still fails, the issue is in the Swarm process environment (caller/version/console setup), not the data. |
| `FileNotFoundException: Could not find file ...` | Wrong path, or DataPacker passing a stale path | Check the DataPacker invocation arguments. |
| `Unhandled exception. <Type>: <message>` followed by obfuscated stack frames like `at .(...)` | Generic .NET unhandled exception. The obfuscated names (rendered as `.` or as Unicode glyphs in some terminals) are intentional QuadSpinner code obfuscation — they convey no diagnostic value. | Look only at the exception type + message on the first line. The frames after it are noise. |
| Exit code `3762504530` (`0xE0434352`), surfaced in DataPacker's `Gaea.Swarm.exe exited with code ...` error | Standard CLR unhandled-exception SEH code — set by Windows whenever .NET propagates an unhandled exception, regardless of which exception. | Carries no diagnostic value beyond "the .NET process crashed." Use the Swarm log text, not the exit code. |
