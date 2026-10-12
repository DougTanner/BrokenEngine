<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-12T22:45:12.247Z","dependsOn":["Documents/Plans/Engine/ConnectClientToHostedServer.md"]} -->
# Deploy the Release server to the Azure VM and run it unattended

## Context

This is the last of four Plans that put the dedicated server on the internet. When this Plan is claimed, the earlier three have landed:

- `Documents/Plans/Engine/HardenServerForUnattendedHosting.md` made the server save its world on `WM_ENDSESSION`, fail fast on a game-port bind failure, and drop peers that never handshake.
- `Documents/Plans/ChangeWorkflow/ProvisionAzureServerVm.md` created the `/deploy-server` skill, its `New-ServerVm.ps1`, and the deallocated VM `brokenengine-server` in `brokenengine-server-rg`. The VM has NSG rules for UDP 27015 (any source) and TCP 3389 and 22 (the user's IP only). `references/azure-facts.md` holds the prices and billing facts.
- `Documents/Plans/Engine/ConnectClientToHostedServer.md` made Main Menu's Remote Server button connect to the VM's static IP (`kRemoteServerAddress`, `Projects/BrokenEngineSandbox/Source/Game.cpp`).

This Plan adds the scripts that configure the VM, copy the server to it, start and stop it, and prove the whole path end to end.

User decisions (this session, binding):

- Server start is automatic on boot: Windows auto-logon plus a logon-triggered scheduled task runs the server launch. Starting the VM from Azure therefore brings the server up with no RDP. The server cannot run as a Windows service, because it needs an interactive desktop window (`Engine/Source/Main.cpp:192-235`).

Main-session decisions the user did not object to (these are not user decisions):

- Deploy only the Release server, `BrokenEngineSandboxServer.exe`, never Debug. Debug exposes save, load, reset, pause, and time-speed controls to every client (gap G11; `Documents/Architecture/Network.md` `kbDebugInput` gate).
- Visual Studio remote debugging is out of scope for this chain.
- Unattended dialogs (gap G2) get no new flag. The deployed launch reuses `--agent-port <port>`, which already takes every no-dialog branch (`Main.cpp:76,854,899`, `Engine/Source/CrashReport.cpp:114-115`) and binds loopback only (`Engine/Source/Agent/AgentCommandServer.cpp:23`).
- The operational scripts go in the `/deploy-server` skill.

## Design

This Plan adds to `/deploy-server`: package and copy, the VM-side launch, auto-logon and scheduled-task setup, start, and clean stop. Its acceptance is end to end, because only a client on the user's PC crossing the internet exercises the NSG, the Windows Firewall rule, and the auto-logon start together: a Release client built with `ConnectClientToHostedServer.md`'s IP connects through the Remote Server button and plays, then the scripts stop and restart the server and the world resumes from the autosave.

### How the server reaches its final autosave

What Microsoft documents:

- The Deallocate REST operation (behind `az vm deallocate`) says only that it "Shuts down the virtual machine and releases the compute resources" (https://learn.microsoft.com/en-us/rest/api/compute/virtual-machines/deallocate). It offers `forceDeallocate` but documents no guest notification and no time limit.
- Power Off documents `skipShutdown`, "The parameter to request non-graceful VM shutdown ... Default value for this flag is false" (https://learn.microsoft.com/en-us/rest/api/compute/virtual-machines/power-off). That describes Power Off, not Deallocate.
- The auto-shutdown page says only that it can "reduce costs by shutting down the VMs" (https://learn.microsoft.com/en-us/azure/virtual-machines/auto-shutdown-vm).

No Microsoft primary source states that deallocation or auto-shutdown delivers `WM_ENDSESSION` to the interactive session, or within what time. `WM_ENDSESSION` itself allows termination as soon as the handler returns (https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-endsession). This Plan therefore does not rely on the guest shutdown for a scripted stop:

- Scripted stop: `Stop-ServerVm.ps1` first sends the agent `quit` command over loopback on the VM. It waits for the server process to exit, which happens after its post-loop final autosave (`Main.cpp:489-494`, routed through `FinalAutosave()` by the hardening Plan). Only then does it deallocate.
- Nightly auto-shutdown: no script runs. The server's `WM_ENDSESSION` handler saves if Windows delivers that message. Otherwise the loss is bounded by the hourly periodic autosave (`kAutosaveInterval`, `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h:28`). The acceptance below observes which case happens once, and records it.

Agent wire format, from `Engine/Source/Agent/AgentCommandServer.cpp`:

- A request is a 4-byte little-endian length followed by that many bytes of UTF-8 JSON (`:193-208`). The response uses the same framing (`:539-541`).
- The envelope keys are `cmd`, `params`, and `id`; any other key is rejected (`:361-368`). The response carries `ok` and `result` (`:318-319`).
- `quit` takes no parameters and sets `gbQuit` (`Engine/Source/Agent/AgentCommandsShared.cpp:107-111`).

The VM-side stop script speaks this framing with `System.Net.Sockets.TcpClient`. Deploying `Tools/AgentHarness` is not an option: its socket commands require repository harness-ownership tokens (`Tools/AgentHarness/AGENTS.md`), which do not exist on the VM.

### VM layout and launch

| Path on the VM | Holds | Writable by `beserver` |
| --- | --- | --- |
| `C:\BrokenEngine\Server\BrokenEngineSandboxServer.exe` | Release server | no |
| `C:\BrokenEngine\Server\Data\Islands.manifest`, `Islands.pack` | the only data the server opens (`Engine/Source/File/PackChunks.cpp:104-108,264-271`) | no |
| `C:\BrokenEngine\State\` | `--app-data-directory`. The server appends `Broken Engine Sandbox Server\`, giving `ServerAutosave.save`, its timestamped backups, and crash reports | yes (Modify) |
| `C:\BrokenEngine\Logs\` | one log per launch, `server-<yyyyMMdd-HHmmss UTC>.log` | yes (Modify) |
| `C:\BrokenEngine\Scripts\` | the VM-side scripts | no |

The deploy set is the exe plus `Data\Islands.manifest` and `Data\Islands.pack`, nothing else, because those two are the only data files the server opens. The launch pins `--app-data-directory` to a fixed VM path, because by default the server writes its saves and crash reports under the launching account's roaming AppData (gap G5, `Engine/Source/File/FileManager.cpp:132-152`).

`Start-Server.ps1` creates `C:\BrokenEngine\Logs` if it is absent and names each log per launch, because `--log-file` does not create its parent and truncates an existing file (gap G7, `Common/Log/Log.cpp:72-81`). It then starts the server with `Start-Process`, working directory `C:\BrokenEngine\Server`, and this command line:

`BrokenEngineSandboxServer.exe --data-directory C:\BrokenEngine\Server\Data --app-data-directory C:\BrokenEngine\State --log-file C:\BrokenEngine\Logs\server-<stamp>.log --agent-port 27100`

`Start-Server.ps1` exits without waiting on the server process. Not waiting is the simplest consistent choice: the scheduled task then ends at once, so no task setting acts on the running server, and a repeated start reaches the server's own single-instance check, which exits without a dialog under `--agent-port` (`Main.cpp:847-858`).

- `--data-directory` and `--app-data-directory` must be absolute existing directories (`Engine/Source/LaunchOptions.cpp:68-140`). `Initialize-ServerHost.ps1` creates both.
- `--loopback-only` is omitted, so the game port binds all IPv4 interfaces (`Engine/Source/Network/Server/Server.cpp:32`).
- The game port is fixed at UDP 27015 (`kiDefaultPort`, `Engine/Source/Network/NetworkProtocol.h:63`).
- This Plan chooses agent port 27100. `Start-Server.ps1` and `Stop-Server.ps1` each name it, and `SKILL.md` documents it.
- The deployed launch keeps the `HIGH_PRIORITY_CLASS` the server sets on itself (`Main.cpp:865-866`), with no override.

### Accounts, auto-logon, and the scheduled task

- `beadmin` is the administrator from `ProvisionAzureServerVm.md`, used by SSH and the user's RDP.
- `beserver` is a dedicated local non-administrator account, a member of `Users`. `Initialize-ServerHost.ps1` creates it with a random 32-character password from `System.Security.Cryptography.RandomNumberGenerator` that meets Windows complexity rules. The password never leaves the VM and is never printed.
- Auto-logon: set `AutoAdminLogon` `1`, `DefaultUserName` `beserver`, and `DefaultPassword` under `HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon` (https://learn.microsoft.com/en-us/troubleshoot/windows-server/user-profiles-and-logon/turn-on-automatic-logon). That page warns that the password "is stored in the registry in plain text", remotely readable by Authenticated Users. This Plan accepts that because the account is non-administrative and used only for this, and because the NSG admits remote-administration ports only from the user's IP. The same page notes that an interactive console logon by a different user rewrites `DefaultUserName`, so `SKILL.md` tells the user to RDP as `beadmin` in a normal session, never `mstsc /admin`.
- Scheduled task `BrokenEngineServer`:
  - Trigger `New-ScheduledTaskTrigger -AtLogOn -User beserver`.
  - Principal `New-ScheduledTaskPrincipal -UserId beserver -LogonType Interactive`, so it runs in the auto-logon desktop session.
  - Action `powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File C:\BrokenEngine\Scripts\Start-Server.ps1`. `Bypass` appears only in this VM task action, never in a repository invocation. The reason is that `about_Execution_Policies` warns "during sign on, a PowerShell logon script could start execution before the Windows Desktop is ready", which fails the `RemoteSigned` zone check, and "Using an execution policy of **ByPass** or **AllSigned** doesn't require a Zone check".
  - Default settings, with no `New-ScheduledTaskSettingsSet` overrides. The task ends when `Start-Server.ps1` returns, so neither the default three-day execution time limit nor the multiple-instances policy applies to the running server.
  - `Publish-Server.ps1` also starts the task on demand, after it has stopped any running server.
- Windows Firewall: create `BrokenEngine-Game-UDP`, an inbound Allow rule for UDP local port 27015, `-Program C:\BrokenEngine\Server\BrokenEngineSandboxServer.exe`, `-Profile Any`. Remove any existing rule of that name first. The rule must exist before the first launch, because when no allow rule exists the user is prompted, and "If the user isn't a local admin and they are prompted, block rules are created. It doesn't matter what option is selected" (https://learn.microsoft.com/en-us/windows/security/operating-system-security/network-security/windows-firewall/rules). `beserver` is not an admin. The NSG remains the outer filter.

### Channels to the VM

- Bootstrap through `az vm run-command invoke --command-id RunPowerShellScript`, which needs no inbound port. Per https://learn.microsoft.com/en-us/azure/virtual-machines/windows/run-command it runs "as System", one script at a time, with output limited to the last 4,096 bytes.
- Everything after bootstrap, including the file copy, uses the user's PC's built-in OpenSSH client (`ssh`, `scp`) against the OpenSSH Server. This Plan chooses that transport because it installs nothing on either side and the NSG already admits TCP 22 from the user's IP. Microsoft documents that OpenSSH Server, "Beginning with Windows Server 2025 ... is now installed by default" and is enabled with `Start-Service sshd` and `Set-Service -Name sshd -StartupType 'Automatic'` (https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_install_firstuse).
- Key authentication: the local scripts run unattended and cannot type a password. `Initialize-ServerVm.ps1` therefore creates `$HOME\.ssh\brokenengine-server` (`ssh-keygen -t ed25519 -N ""`) when absent. The bootstrap writes its public key to `C:\ProgramData\ssh\administrators_authorized_keys` and sets that file's ACL with `icacls ... /inheritance:r /grant "*S-1-5-32-544:F" /grant "SYSTEM:F"`, as https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_keymanagement prescribes for administrator accounts.
- Every `ssh`/`scp` call passes `-i <key> -o IdentitiesOnly=yes -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$HOME\.ssh\brokenengine-server_known_hosts`. The remote shell is `cmd.exe`, so remote steps run as `powershell -NoProfile -File C:\BrokenEngine\Scripts\<script>.ps1`.
- Each local script reads the VM's address from Azure: `az network public-ip show -g brokenengine-server-rg -n brokenengine-server-ip --query ipAddress -o tsv`. No script stores a copy.

### Script set

Local scripts run on the user's PC in PowerShell 7 with `pwsh -NoProfile -File .agents/skills/deploy-server/scripts/<name>.ps1`. Each prints one JSON result object in `/external-skill-creator`'s form, and each first requires `az account show` to succeed, otherwise its message tells the user to run `! az login`.

- `Initialize-ServerVm.ps1`, one-time and safe to rerun:
  1. Start the VM if it is deallocated.
  2. Create the SSH key.
  3. Run `scripts/vm/Initialize-ServerHost.ps1` through run-command, with the public key as a parameter.
  4. `az vm restart`, so auto-logon takes effect.
  5. Poll over SSH, for up to 10 minutes, until `query user` lists an active `beserver` session.
- `Publish-Server.ps1`:
  1. Require the VM to be running.
  2. Resolve `BrokenEngineSandboxServer.exe` from the session worktree's `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/`. Resolve `Islands.manifest` and `Islands.pack` from the primary checkout's `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data/`, which is the Shared data both Release builds consume (`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`). The primary root is the parent of `git rev-parse --path-format=absolute --git-common-dir`.
  3. If `BrokenEngineSandboxServer` is running on the VM, run `Stop-Server.ps1` over SSH and fail if it fails.
  4. `scp` the VM-side scripts to `C:\BrokenEngine\Scripts\`, the exe to `C:\BrokenEngine\Server\`, and the two Islands files to `C:\BrokenEngine\Server\Data\`.
  5. Run `schtasks /Run /TN BrokenEngineServer` over SSH.
  6. Poll for up to 5 minutes until `Get-NetUDPEndpoint -LocalPort 27015` is owned by the server process.
- `Start-ServerVm.ps1`: `az vm start`, then poll over SSH for up to 10 minutes, retrying while `sshd` comes up, until the server process owns UDP 27015. It reports the newest log file name.
- `Stop-ServerVm.ps1`:
  1. If the VM is already deallocated, report that and exit successfully.
  2. Otherwise run `Stop-Server.ps1` over SSH, then `az vm deallocate`.
  3. If `Stop-Server.ps1` fails, do not deallocate. Exit with a code whose message gives the raw `az vm deallocate` command from `SKILL.md` for the user to choose.
  4. Report `ServerAutosave.save`'s `LastWriteTimeUtc` and the final power state.

VM-side scripts are copied to `C:\BrokenEngine\Scripts\` and run by Windows PowerShell 5.1, "the version of PowerShell that ships in Windows" (https://learn.microsoft.com/en-us/powershell/scripting/what-is-windows-powershell). PowerShell 7 is not on the image, and installing it would add a download step for three short scripts. They must therefore use only Windows PowerShell 5.1 syntax and cmdlets. That is a deliberate exception to the repository's PowerShell 7 default, and `SKILL.md` states it. Over SSH, Windows Server's default `RemoteSigned` execution policy runs them, because only downloads marked as coming from the internet need a signature and scp adds no such mark (https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_execution_policies). The logon task is the one exception, explained above.

- `vm/Initialize-ServerHost.ps1` runs as SYSTEM through run-command and is safe to rerun. It:
  - enables `sshd` and installs the administrator key;
  - creates the five directories and their `beserver` ACLs;
  - creates or resets `beserver` and adds it to `Users`;
  - writes the Winlogon values;
  - recreates the firewall rule;
  - registers the scheduled task with `-Force`.
- `vm/Start-Server.ps1`: the launch above.
- `vm/Stop-Server.ps1`:
  1. If no `BrokenEngineSandboxServer` process exists, exit `0`.
  2. Otherwise connect to `127.0.0.1:27100`, send `{"cmd":"quit","params":{},"id":1}` in the framing above, and read the response for up to 10 s. The response is informative only.
  3. `Wait-Process` for up to 120 s. Exit `0` when the process has exited, otherwise `1`.
  4. Print `ServerAutosave.save`'s `LastWriteTimeUtc`.

`SKILL.md` gains these operations, the explicit user confirmation before any step that starts the VM (it bills $0.193/hour while running, `references/azure-facts.md`), the RDP note, and the agent port. `references/azure-facts.md` gains a `## Nightly auto-shutdown` section with the observation below.

Risk tier: Tier 2. This is tool behavior inside one skill. No repository runtime surface changes: no C++, wire, save, determinism, or threading change, and the server's trust rules are untouched. The external exposure (UDP 27015 public) was fixed by `ProvisionAzureServerVm.md`. This Plan adds only the VM-side firewall rule that matches it.

## Critical files

- `.agents/skills/deploy-server/SKILL.md`, `references/azure-facts.md`
- `.agents/skills/deploy-server/scripts/Initialize-ServerVm.ps1`, `Publish-Server.ps1`, `Start-ServerVm.ps1`, `Stop-ServerVm.ps1` (new)
- `.agents/skills/deploy-server/scripts/vm/Initialize-ServerHost.ps1`, `Start-Server.ps1`, `Stop-Server.ps1` (new)

## In scope

- The seven new scripts above, and the `SKILL.md` and `references/azure-facts.md` additions named in the Design.
- Configuring the existing VM, deploying to it, starting it, stopping it, and changing then restoring its auto-shutdown time for the acceptance observation, all after the user's explicit confirmation.

## Out of scope

- Any C++ change, any change to `New-ServerVm.ps1` or the Azure resources it created (other than the temporary auto-shutdown time), and any other Plan.
- A Debug or Profile server deployment, PDB deployment, and Visual Studio remote debugging.
- Running the server as a Windows service. A non-interactive "run whether user is logged on or not" task. Re-asserting `DefaultUserName` after a console logon.
- Minidumps (gap G10), pruning autosave backups (gap G6) or logs, and copying autosaves off the VM.
- Deploying Local-mode (changed) island data. Changed islands invalidate the autosave, and the server then starts a fresh world (`Projects/BrokenEngineSandbox/Source/Game.cpp:42-47`).
- Installing PowerShell 7 on the VM, changing the Azure guest patch mode, IPv6, DNS names, Dev/Test pricing, Azure Bastion, and load testing.

## Acceptance criteria

1. `/external-skill-creator` validation of `.agents/skills/deploy-server/` passes.
2. `/compile` builds `BrokenEngineSandbox` client and server `Release|x64` with Shared data.
3. The executor confirms sign-in (`az account show`, otherwise asks the user for `! az login`). The executor tells the user that the next steps start the VM at $0.193/hour, and proceeds only on an explicit yes.
4. `Initialize-ServerVm.ps1` succeeds, and `query user` over SSH shows an active `beserver` session after the restart.
5. `Publish-Server.ps1` succeeds, and over SSH:
   - `Get-NetUDPEndpoint -LocalPort 27015` is owned by `BrokenEngineSandboxServer`;
   - `C:\BrokenEngine\Server` holds exactly the exe and `Data\Islands.manifest` and `Data\Islands.pack`;
   - `Get-NetFirewallRule -Name BrokenEngine-Game-UDP` is enabled with action Allow;
   - no enabled inbound Block rule names the server exe;
   - a `server-*.log` exists.
6. The executor gives the user the exact command to start the Release client: the session worktree's `Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output\BrokenEngineSandbox.exe` with `--data-directory` set to the primary checkout's `...\Output\Data`. The user starts it on their PC, clicks `REMOTE SERVER`, connects, plays for at least a few minutes (spawn, move a fleet), and confirms in their own message.
7. `Stop-ServerVm.ps1` succeeds. It reports the server process exited after `quit`, a `ServerAutosave.save` `LastWriteTimeUtc` later than the moment it sent `quit`, and `PowerState/deallocated`.
8. `Start-ServerVm.ps1` succeeds with no RDP session and no manual step on the VM. It reports a new log file while the previous one still exists. The user reconnects through `REMOTE SERVER` and confirms the world resumed where they left it (their units where they were).
9. Auto-shutdown observation:
   1. Read the current schedule time.
   2. Set `az vm auto-shutdown --time` to about 10 minutes ahead in UTC, with the server running.
   3. Poll `az vm get-instance-view` until the VM leaves `PowerState/running`, and record the final power state.
   4. Start the VM with `Start-ServerVm.ps1`. Record whether `ServerAutosave.save`'s `LastWriteTimeUtc` falls within the shutdown window, and whether the previous log ends with `WM_ENDSESSION: final autosave`.
   5. Write both findings in `references/azure-facts.md` `## Nightly auto-shutdown`. Either outcome is acceptable. The section states the resulting loss bound: none, or up to one `kAutosaveInterval`.
   6. Restore the schedule time read in step 1.
10. The VM ends deallocated, by `Stop-ServerVm.ps1`.
