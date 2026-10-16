<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-12T22:41:30.813Z","dependsOn":["Documents/Plans/Engine/HardenServerForUnattendedHosting.md"]} -->
# Provision the Azure server VM and create the /deploy-server skill

## Context

This is the second of four Plans that put the dedicated server on the internet.
`Documents/Plans/Engine/HardenServerForUnattendedHosting.md` (the prerequisite)
makes the server safe to run unattended on a public port. This Plan creates the
Azure resources, and the repository skill that creates them. Next,
`Documents/Plans/Engine/ConnectClientToHostedServer.md` embeds the VM's static
public IP in the client, and
`Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md` deploys and runs
the server.

User decisions (this session, binding):

- Host: Azure, a Windows Server VM on a normal pay-as-you-go subscription, with no Dev/Test offer. The user must explicitly confirm before any Azure resource is created, because creation starts billing.
- Audience: anyone on the internet. The game port UDP 27015 is open to any source. RDP (TCP 3389) and SSH (TCP 22) are restricted to the user's own public IP.
- The client reaches the server by its static public IP, compiled in as a constant, and the Remote Server button connects to it. That is why `ConnectClientToHostedServer.md` follows this Plan, which produces the IP.

Main-session decisions the user did not object to (these are not user decisions):

- The client uses the raw IP, not an Azure DNS label.
- Operational scripts live in one new repository skill, `/deploy-server` (`.agents/skills/deploy-server/`).

Facts carried forward from the deleted Azure hosting guide (formerly under `Documents/`), corrected against Microsoft sources (research of 2026-10-12):

- Prices are East US list (retail) pay-as-you-go prices from the Azure Retail Prices API (https://prices.azure.com/api/retail/prices, documented at https://learn.microsoft.com/en-us/rest/api/cost-management/retail-prices/azure-retail-prices). The calculator can differ by agreement.
  - `Standard_D2s_v6` Windows: $0.193/hour while running. That is 2 vCPU and 8 GiB (https://learn.microsoft.com/en-us/azure/virtual-machines/sizes/general-purpose/dsv6-series).
  - Standard SSD E10 (128 GiB): $9.60/month plus $0.002 per 10K transactions (https://learn.microsoft.com/en-us/azure/virtual-machines/disks-types).
  - Standard static public IPv4: $0.005/hour, which is $3.65/month.
  - Egress: the first 100 GB/month is free, then $0.087/GB (https://azure.microsoft.com/en-us/pricing/details/bandwidth/).
- Billing states (https://learn.microsoft.com/en-us/azure/virtual-machines/states-billing):
  - A deallocated VM is "Not billed" for compute, but "Disks and Networking continue to incur charges". The disk and the IP therefore bill $13.25/month whatever the VM state.
  - "Stopped" is billed. That state results from the `PowerOff` API (`az vm stop`) or "invoking shutdown from within the guest OS". Stopping always means `az vm deallocate` (https://learn.microsoft.com/en-us/cli/azure/vm#az-vm-deallocate).
- Cost examples:
  - Light use, 75 running hours a month: $14.48 compute + $13.25 = about $27.73/month.
  - Running every hour of the month (730 h): $140.89 + $13.25 = about $154.14/month.
  - Disk transactions and egress beyond 100 GB are extra.
- Standard-SKU public IPs are static-only and keep their address across deallocate and start. Basic SKU was retired on 2025-09-30 (https://learn.microsoft.com/en-us/azure/virtual-network/ip-services/public-ip-addresses). The old guide's "make the IP static" step is unnecessary.
- The default NSG rule `DenyAllInbound` denies everything. `az vm create` adds an any-source RDP rule unless `--nsg-rule NONE` (https://learn.microsoft.com/en-us/azure/virtual-network/network-security-groups-overview, https://learn.microsoft.com/en-us/cli/azure/vm#az-vm-create). `az vm open-port` takes no source or protocol parameter, so it cannot express a restricted rule.
- The CLI and portal create Gen2 Trusted Launch VMs by default, at no extra cost (https://learn.microsoft.com/en-us/azure/virtual-machines/trusted-launch). Dsv6 is Gen2-only (dsv6-series page) and NVMe-only: "For v6, Ebsv5, and later VMs, you need to transition from SCSI to NVMe" (https://learn.microsoft.com/en-us/azure/virtual-machines/nvme-overview). This Plan passes `--disk-controller-type NVMe` explicitly rather than relying on a CLI default. The CLI marks that parameter Preview.
- The CLI's Windows Server image alias list has no 2025 alias, so the script uses the URN.
- `az vm auto-shutdown --time` is "The UTC time of day ... Format: hhmm" (https://learn.microsoft.com/en-us/cli/azure/vm#az-vm-auto-shutdown). No Microsoft page states the power state an auto-shutdown leaves, so `DeployAndRunHostedServer.md` observes it once.
- A budget only alerts. "Resources aren't affected, and your consumption isn't stopped". Budgets "are evaluated against these costs every 24 hours" (https://learn.microsoft.com/en-us/azure/cost-management-billing/costs/tutorial-acm-create-budgets). The `az consumption` command group is Preview (https://learn.microsoft.com/en-us/cli/azure/consumption/budget).
- The admin password must be at least 12 characters (https://learn.microsoft.com/en-us/azure/virtual-machines/windows/quick-create-portal). `az vm create` enforces Azure's complexity rule.

## Design

### Choices and their evidence

- VM size `Standard_D2s_v6` (2 vCPU, 8 GiB, $0.193/hour above), the size the deleted hosting guide used; the repository has no load data to size it otherwise.
- Image URN `MicrosoftWindowsServer:WindowsServer:2025-datacenter-azure-edition:latest`, a non-`-core` Windows Server 2025 image. The server needs a desktop window (`Engine/Source/Main.cpp:192-235`), and Windows Server 2025 installs the OpenSSH Server that `DeployAndRunHostedServer.md` uses (https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_install_firstuse).
- A StandardSSD_LRS OS disk, the lowest SSD tier, priced above as E10. A Standard static public IP, because Basic is retired (above).
- Auto-shutdown enabled (`az vm auto-shutdown`, time in UTC), a daily backstop against a VM left running at $0.193/hour. `DeployAndRunHostedServer.md` observes the power state it leaves.
- Networking uses `--nsg-rule NONE` plus explicit `az network nsg rule create` rules, never `az vm open-port`, because `az vm create` otherwise adds an any-source RDP rule and `az vm open-port` cannot restrict a source (above).
- LAN discovery UDP 27016 and the agent port get no NSG rule. The discovery responder binds every interface and answers any sender (`Engine/Source/Network/NetworkDiscoveryResponder.cpp:20-22,39-50`), and the agent server binds loopback only (`Engine/Source/Agent/AgentCommandServer.cpp:23`).
- A monthly Cost Management budget alert is part of provisioning, because nothing else reports spending. A budget only alerts and never stops resources (above).

### Skill layout

This Plan creates the skill with the provisioning script, which uses the Azure CLI; `DeployAndRunHostedServer.md` adds the rest. `/deploy-server` is a main-session skill, because its steps ask the user to sign in, supply values, and confirm spending. `/external-skill-creator` authors and validates it. It holds three files:

- `SKILL.md`:
  - When to use: provisioning, and the operations later Plans add.
  - Prerequisites. The Azure CLI must be installed; `az version` checks it, and the user installs the CLI when it is missing. The user signs in with `! az login`.
  - The confirmation rule: never run a step that creates an Azure resource or starts billing until the user has confirmed it in their own message, after seeing the resource list and cost summary.
  - The provisioning invocation.
  - Two raw Azure CLI operations: `az vm deallocate -g brokenengine-server-rg -n brokenengine-server` to stop billing, and `az network nsg rule update -g brokenengine-server-rg --nsg-name brokenengine-server-nsg -n <Admin-RDP|Admin-SSH> --source-address-prefixes <new IPv4>/32` for when the user's public IP changes.
- `scripts/New-ServerVm.ps1`: the provisioning script.
- `references/azure-facts.md`:
  - the resource names below, and the prices, billing states, and cost examples above with their source URLs and query date;
  - the provisioned OS disk tier, recorded from the acceptance check.

It never records the public IP. That IP has one source of truth: this Plan's final step writes it into `ConnectClientToHostedServer.md`, which later makes the C++ constant the only copy.

### Fixed names and settings

| Item | Value |
| --- | --- |
| Region | `eastus` (the prices above are East US) |
| Resource group | `brokenengine-server-rg` |
| NSG | `brokenengine-server-nsg` |
| VM | `brokenengine-server` |
| Public IP | `brokenengine-server-ip`, `--public-ip-sku Standard` |
| Admin user | `beadmin` |
| Auto-shutdown | `-ShutdownTimeUtc` parameter, default `0800` |
| Budget | `brokenengine-server-budget`, Cost, Monthly, $50, scoped to the resource group, with alerts at 80% and 100% of actual cost |

### `New-ServerVm.ps1`

Invocation: `pwsh -NoProfile -File .agents/skills/deploy-server/scripts/New-ServerVm.ps1 -AdminSourceAddress <IPv4> [-ShutdownTimeUtc <hhmm>]`. It prints one JSON result object in the form `/external-skill-creator` prescribes for repository scripts, with `status`, `code`, `message`, and on success `publicIp`. It runs these steps in order and stops at the first failure with a typed code:

1. Validate the inputs. `-AdminSourceAddress` must parse as an IPv4 address; `-ShutdownTimeUtc` must match `^([01]\d|2[0-3])[0-5]\d$`.
2. Check the environment. `az version` must succeed. `az account show` must succeed, otherwise return a code whose message tells the user to run `! az login`. The signed-in account's `user.name` becomes the budget alert address.
3. Read the admin password. Take the first line of `$env:LOCALAPPDATA\BrokenEngine\DeployServer\admin-password.txt` and refuse to continue if it is shorter than 12 characters.
4. Refuse to update. `az group exists -n brokenengine-server-rg` must be `false`. This script only creates; it never modifies existing resources.
5. `az group create -n brokenengine-server-rg -l eastus`.
6. `az network nsg create -g brokenengine-server-rg -n brokenengine-server-nsg`. Then create three inbound Allow rules with `az network nsg rule create`, each passing protocol, port, source, and priority explicitly. Rules exist before the VM, so the VM is never exposed by a default rule.
   - `Game-UDP-27015`: priority 100, `Udp`, source `*`, destination port 27015.
   - `Admin-RDP`: priority 110, `Tcp`, source `<AdminSourceAddress>/32`, port 3389.
   - `Admin-SSH`: priority 120, `Tcp`, source `<AdminSourceAddress>/32`, port 22.
7. Create the VM with `az vm create -g brokenengine-server-rg -n brokenengine-server --image MicrosoftWindowsServer:WindowsServer:2025-datacenter-azure-edition:latest --size Standard_D2s_v6 --storage-sku StandardSSD_LRS --disk-controller-type NVMe --public-ip-address brokenengine-server-ip --public-ip-sku Standard --nsg brokenengine-server-nsg --nsg-rule NONE --admin-username beadmin --admin-password <password>`. After it succeeds, delete the password file.
8. Set the shutdown schedule with `az vm auto-shutdown -g brokenengine-server-rg -n brokenengine-server --time <ShutdownTimeUtc>`.
9. Create the budget with `az consumption budget create-with-rg -g brokenengine-server-rg -n brokenengine-server-budget`. It is Cost, Monthly, amount 50, and runs from the first day of the current UTC month for one year, ending on the same day a year later. It has two `Actual` notifications, at thresholds 80 and 100, addressed to the account from step 2. Spell the `--time-period` and `--notifications` arguments the way the command's own help (`--notifications ??`) documents.
10. `az network public-ip show -g brokenengine-server-rg -n brokenengine-server-ip --query ipAddress -o tsv` becomes `publicIp`.

### Executor steps

Steps that only the user can do are explicit questions to the user:

1. Author the skill through `/external-skill-creator`, which also validates it.
2. Ask the user to confirm the Azure CLI is installed, then to run `! az login` and pick the pay-as-you-go subscription as the CLI default. If they want a different one, they run `! az account set --subscription <name>`.
3. Ask the user for their public IPv4 address.
4. Ask the user to create `%LOCALAPPDATA%\BrokenEngine\DeployServer\admin-password.txt` containing a VM admin password they choose, of at least 12 characters with upper case, lower case, digits, and symbols. Tell them to keep it for RDP, because the script deletes the file after use. The password never enters the conversation.
5. Show the user the confirmation summary and wait for an explicit yes. The summary holds:
   - the subscription from `az account show`;
   - every resource in the table above;
   - the shutdown time;
   - the cost lines above: $0.193/hour while running, $13.25/month always, and about $27.73/month at 75 hours.
   If the user wants another shutdown time, pass it as `-ShutdownTimeUtc`.
6. Run `New-ServerVm.ps1`.
7. Run the acceptance checks below.
8. Edit `Documents/Plans/Engine/ConnectClientToHostedServer.md`. In its `## Server address` section, replace `none` on the `Recorded address:` line with the `publicIp` value.
9. Deallocate the VM with the raw command in `SKILL.md`. It stays deallocated until `DeployAndRunHostedServer.md` runs.

Risk tier: Tier 2. This is new tool behavior in one skill. It changes no repository runtime surface: determinism, wire, save, threading, and the server's trust rules are untouched. The external risks are spending and exposure. Explicit user confirmation gates the spending, and the NSG rules above fix the exposure.

## Critical files

- `.agents/skills/deploy-server/SKILL.md` (new)
- `.agents/skills/deploy-server/scripts/New-ServerVm.ps1` (new)
- `.agents/skills/deploy-server/references/azure-facts.md` (new)
- `Documents/Plans/Engine/ConnectClientToHostedServer.md` (`## Server address` line only)

## In scope

- The three new skill files above, plus whatever registration and validation `/external-skill-creator` requires for a new skill.
- Creating the Azure resources listed in the table, once, after the user's explicit confirmation.
- The `Recorded address:` line in `ConnectClientToHostedServer.md` `## Server address`.

## Out of scope

- Enabling `sshd`, installing SSH keys, VM-side configuration, firewall rules inside Windows, accounts, auto-logon, deployment, and start/stop scripts. `DeployAndRunHostedServer.md` owns all of these.
- Any C++ change, and any edit to another Plan beyond the one line named above.
- An update or repair path in `New-ServerVm.ps1`, VM resize, and teardown or deletion scripts.
- A Dev/Test subscription, Visual Studio subscription credit, savings plans, Azure Bastion, just-in-time access, IPv6, an Azure DNS label, a Visual Studio remote-debugging port, and load testing.

## Acceptance criteria

- `/external-skill-creator` validation of `.agents/skills/deploy-server/` passes.
- `New-ServerVm.ps1` returns `status` success and a `publicIp`, and `admin-password.txt` no longer exists.
- `az network nsg rule list -g brokenengine-server-rg --nsg-name brokenengine-server-nsg -o json` lists exactly the three rules above, with those protocols, ports, priorities, and sources.
- `az vm show -g brokenengine-server-rg -n brokenengine-server -d` reports `Standard_D2s_v6`, the 2025 Azure Edition image reference, and a `StandardSSD_LRS` OS disk. `az disk show` on that disk gives its size and tier, which `references/azure-facts.md` records.
- `az network public-ip show -g brokenengine-server-rg -n brokenengine-server-ip` reports `sku.name` `Standard` and `publicIPAllocationMethod` `Static`.
- The VM's auto-shutdown schedule exists with the chosen UTC time. Read it with `az resource show` on `microsoft.devtestlab/schedules/shutdown-computevm-brokenengine-server` in the resource group.
- `az consumption budget list -g brokenengine-server-rg` lists `brokenengine-server-budget` at amount 50, Monthly.
- From the user's PC, `Test-NetConnection <publicIp> -Port 3389` reports `TcpTestSucceeded : True` while the VM runs.
- `ConnectClientToHostedServer.md`'s `Recorded address:` line equals `az network public-ip show ... --query ipAddress`.
- At the end, `az vm get-instance-view -g brokenengine-server-rg -n brokenengine-server --query "instanceView.statuses[?starts_with(code,'PowerState')].code"` reports `PowerState/deallocated`.
