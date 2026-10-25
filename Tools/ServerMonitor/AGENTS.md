# Server Monitor

## Ownership

This standalone Windows program displays statistics, the Map tab, and the Profile
tab from a local or remote server. It compiles standard and Windows headers and
`Common/ServerMonitorProtocol.h` by path; it compiles no engine or game code.
The channel contract belongs to `../../Engine/Source/Server/AGENTS.md`
`## Monitor Channel`.

## Connection and Threading

The optional first command-line argument is the server host; the default is
`127.0.0.1`. The secret is read once from
`%APPDATA%\ServerMonitor\ServerMonitorSecret.txt`; a missing file gives an empty
key. Secret trimming and the authentication response are shared Common helpers.

The UI thread owns networking, the received model, and painting. Its single
50 ms timer uses nonblocking sockets and retries disconnected connections every
2 seconds. Authentication retains one 32-byte response and its sent-byte offset:
positive sends advance it, would-block preserves it, and connection state changes
to connected only after all bytes have been accepted. Disconnect clears the model
and pending response before reconnecting. Each snapshot replaces the model and
invalidates the window. Server records are trusted under the root trust policy.

## Presentation

The capture recipe in
`../../.agents/skills/agent-harness/references/server-monitor.md` depends on
pixel geometry: process DPI awareness, tab bar left 250, tabs 100 by 32, Copy
button 80 by 28 with margin 10, Consolas height 28, statistics line height 36,
profile line height 32, and 4 pixels between profile sections. Map bounds come
from active cells with one cell of padding; the monitor formats coordinate labels
from the received cell records. Copy concatenates the profile section text.

## Build

Use `/compile -Target ServerMonitor`, which builds Release x64. The project also
has a Debug configuration for interactive development. It has no PCH and no
packed-data dependency.
